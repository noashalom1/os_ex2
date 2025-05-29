#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <HOST> <PORT>\n";
        return 1;
    }

    const char* host = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        return 1;
    }

    cout << "Connected to server. Type commands (e.g., ADD CARBON 50):\n";

    string line;
    while (getline(cin, line)) {
        line += "\n";
        send(sock, line.c_str(), line.size(), 0);
    }

    close(sock);
    return 0;
}
