#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>
#include <cstring>
#include <string>

using namespace std;

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = 6379;
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, host, &server.sin_addr);

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    cout << "Connected to Cachify at " << host << ":" << port << endl;
    cout << "Type commands (SET key value [ttl], GET key, DEL key, PING, SIZE, QUIT)" << endl;

    string line;
    while (true) {
        cout << "> ";
        if (!std::getline(cin, line)) break;
        if (line.empty()) continue;
        // send line with newline
        string out = line + "\n";
        ssize_t w = send(sock, out.data(), out.size(), 0);
        if (w < 0) { perror("send"); break; }
        // read response (single line or multi-line)
        char buf[4096];
        ssize_t r = recv(sock, buf, sizeof(buf)-1, 0);
        if (r <= 0) { cout << "Server closed\n"; break; }
        buf[r] = '\0';
        cout << buf;
        if (line == "QUIT") break;
    }

    close(sock);
    return 0;
}
