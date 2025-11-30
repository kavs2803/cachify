#include "kv_store.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <sstream>
#include <vector>

using namespace std;

static const int DEFAULT_PORT = 6379;
static const int BACKLOG = 128;
static const int BUFFER_SIZE = 4096;

// Simple text protocol:
// Client sends lines: COMMAND args...\n
// Commands:
// SET key value [ttl_seconds]
// GET key
// DEL key
// PING
// Responses:
// +OK\n   (for SET/DEL successful)
// -ERR <message>\n
// $<len>\n<value>\n  (for GET found)
// $-1\n (for GET not found)
// :<number>\n (for integer reply, e.g. size)

string trim(const string &s) {
    size_t i = 0, j = s.size();
    while (i < j && isspace((unsigned char)s[i])) ++i;
    while (j > i && isspace((unsigned char)s[j-1])) --j;
    return s.substr(i, j - i);
}

vector<string> split_ws(const string &line) {
    vector<string> out;
    std::istringstream iss(line);
    string token;
    while (iss >> token) out.push_back(token);
    return out;
}

void handle_client(int client_fd, KVStore &store) {
    char buffer[BUFFER_SIZE];
    string partial;
    while (true) {
        ssize_t r = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        if (r <= 0) break;
        buffer[r] = '\0';
        partial.append(buffer, r);
        // process lines
        size_t pos;
        while ((pos = partial.find('\n')) != string::npos) {
            string line = partial.substr(0, pos);
            partial.erase(0, pos + 1);
            line = trim(line);
            if (line.empty()) continue;
            auto tokens = split_ws(line);
            if (tokens.empty()) continue;
            string cmd = tokens[0];
            for (auto &c : cmd) c = toupper((unsigned char)c);
            if (cmd == "PING") {
                string resp = "+PONG\n";
                send(client_fd, resp.data(), resp.size(), 0);
            } else if (cmd == "SET") {
                if (tokens.size() < 3) {
                    string resp = "-ERR wrong number of arguments for SET\n";
                    send(client_fd, resp.data(), resp.size(), 0);
                    continue;
                }
                string key = tokens[1];
                string value = tokens[2];
                uint64_t ttl = 0;
                if (tokens.size() >= 4) {
                    try {
                        ttl = std::stoull(tokens[3]);
                    } catch (...) { ttl = 0; }
                }
                store.set(key, value, ttl);
                string resp = "+OK\n";
                send(client_fd, resp.data(), resp.size(), 0);
            } else if (cmd == "GET") {
                if (tokens.size() != 2) {
                    string resp = "-ERR wrong number of arguments for GET\n";
                    send(client_fd, resp.data(), resp.size(), 0);
                    continue;
                }
                auto val = store.get(tokens[1]);
                if (!val.has_value()) {
                    string resp = "$-1\n";
                    send(client_fd, resp.data(), resp.size(), 0);
                } else {
                    string v = val.value();
                    string resp = "$" + to_string(v.size()) + "\n" + v + "\n";
                    send(client_fd, resp.data(), resp.size(), 0);
                }
            } else if (cmd == "DEL") {
                if (tokens.size() != 2) {
                    string resp = "-ERR wrong number of arguments for DEL\n";
                    send(client_fd, resp.data(), resp.size(), 0);
                    continue;
                }
                bool ok = store.del(tokens[1]);
                if (ok) send(client_fd, "+OK\n", 4, 0);
                else send(client_fd, "-ERR key not found\n", 19, 0);
            } else if (cmd == "SIZE") {
                size_t sz = store.size();
                string resp = ":" + to_string(sz) + "\n";
                send(client_fd, resp.data(), resp.size(), 0);
            } else if (cmd == "QUIT") {
                close(client_fd);
                return;
            } else {
                string resp = "-ERR unknown command\n";
                send(client_fd, resp.data(), resp.size(), 0);
            }
        }
    }
    close(client_fd);
}

int main(int argc, char **argv) {
    int port = DEFAULT_PORT;
    if (argc >= 2) port = atoi(argv[1]);

    KVStore store(128); // 128 stripes

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }
    if (listen(sock, BACKLOG) < 0) {
        perror("listen");
        close(sock);
        return 1;
    }
    cout << "Cachify server listening on port " << port << endl;
    while (true) {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(sock, (sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        // spawn thread to handle client
        std::thread t(handle_client, client_fd, std::ref(store));
        t.detach();
    }
    close(sock);
    return 0;
}
