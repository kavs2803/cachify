#include "kv_store.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <iostream>

using namespace std::chrono;

struct ExpireItem {
    time_point<KVStore::Clock> when;
    std::string key;

    bool operator>(ExpireItem const& o) const {
        return when > o.when;
    }
};

struct KVStore::Impl {
    size_t stripes;
    std::vector<std::unordered_map<std::string, Entry>> maps;
    std::vector<std::mutex> locks; // stripe locks

    // expiry queue and related synchronization:
    std::priority_queue<ExpireItem, std::vector<ExpireItem>, std::greater<ExpireItem>> expiry_pq;
    std::mutex expiry_mutex;
    std::condition_variable expiry_cv;

    std::atomic<bool> stop_flag{false};
    std::thread cleaner;

    Impl(size_t s): stripes(s), maps(s), locks(s) {
        cleaner = std::thread([this](){ this->cleaner_thread(); });
    }

    ~Impl() {
        stop_flag = true;
        expiry_cv.notify_all();
        if (cleaner.joinable()) cleaner.join();
    }

    void schedule_expiry(const std::string &key, time_point<KVStore::Clock> when) {
        std::lock_guard<std::mutex> lg(expiry_mutex);
        expiry_pq.push(ExpireItem{when, key});
        expiry_cv.notify_one();
    }

    void cleaner_thread() {
        while (!stop_flag) {
            std::unique_lock<std::mutex> ul(expiry_mutex);
            if (expiry_pq.empty()) {
                expiry_cv.wait_for(ul, std::chrono::seconds(1));
            } else {
                auto next = expiry_pq.top();
                auto now = KVStore::Clock::now();
                if (next.when <= now) {
                    expiry_pq.pop();
                    ul.unlock();
                    // remove key if still expired
                    size_t stripe = std::hash<std::string>{}(next.key) % stripes;
                    std::lock_guard<std::mutex> lg(locks[stripe]);
                    auto &mp = maps[stripe];
                    auto it = mp.find(next.key);
                    if (it != mp.end() && it->second.expiry != time_point<KVStore::Clock>::max()
                        && it->second.expiry <= now) {
                        mp.erase(it);
                    }
                } else {
                    expiry_cv.wait_until(ul, next.when);
                }
            }
        }
    }
};

KVStore::KVStore(size_t stripes)
: impl_(new Impl(stripes)) {}

KVStore::~KVStore() {
    delete impl_;
}

void KVStore::set(const std::string &key, const std::string &value, uint64_t ttl_seconds) {
    size_t stripe = std::hash<std::string>{}(key) % impl_->stripes;
    auto expiry_time = KVStore::Clock::time_point::max();
    if (ttl_seconds > 0) {
        expiry_time = KVStore::Clock::now() + seconds(ttl_seconds);
    }

    {
        std::lock_guard<std::mutex> lg(impl_->locks[stripe]);
        impl_->maps[stripe][key] = Entry{value, expiry_time};
    }

    if (ttl_seconds > 0) {
        impl_->schedule_expiry(key, expiry_time);
    }
}

std::optional<std::string> KVStore::get(const std::string &key) {
    size_t stripe = std::hash<std::string>{}(key) % impl_->stripes;
    auto now = KVStore::Clock::now();
    std::lock_guard<std::mutex> lg(impl_->locks[stripe]);
    auto &mp = impl_->maps[stripe];
    auto it = mp.find(key);
    if (it == mp.end()) return std::nullopt;
    if (it->second.expiry != time_point<KVStore::Clock>::max() && it->second.expiry <= now) {
        // expired -> erase and return nullopt
        mp.erase(it);
        return std::nullopt;
    }
    return it->second.value;
}

bool KVStore::del(const std::string &key) {
    size_t stripe = std::hash<std::string>{}(key) % impl_->stripes;
    std::lock_guard<std::mutex> lg(impl_->locks[stripe]);
    auto &mp = impl_->maps[stripe];
    auto it = mp.find(key);
    if (it == mp.end()) return false;
    mp.erase(it);
    return true;
}

size_t KVStore::size() {
    size_t total = 0;
    for (size_t i = 0; i < impl_->stripes; ++i) {
        std::lock_guard<std::mutex> lg(impl_->locks[i]);
        total += impl_->maps[i].size();
    }
    return total;
}
