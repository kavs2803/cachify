#pragma once

#include <string>
#include <optional>
#include <chrono>

struct KVResult {
    bool found;
    std::string value;
};

class KVStore {
public:
    using Clock = std::chrono::steady_clock;
    KVStore(size_t stripes = 64);
    ~KVStore();

    // Set a key with value, optional ttl_seconds (0 = no expiry)
    void set(const std::string &key, const std::string &value, uint64_t ttl_seconds = 0);

    // Get a key. Returns optional<string> (nullopt if not found/expired)
    std::optional<std::string> get(const std::string &key);

    // Delete a key, returns true if deleted
    bool del(const std::string &key);

    // Size (number of keys currently stored; may be approximate under concurrency)
    size_t size();

private:
    struct Entry {
        std::string value;
        Clock::time_point expiry; // time point; Clock::time_point::max() == no expiry
    };

    // Internal helpers hidden in the .cpp
    struct Impl;
    Impl *impl_;
};
