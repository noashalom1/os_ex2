#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <HOSTNAME/IP> <PORT>" << endl;
        return 1;
    }

    const char* server_ip = argv[1];
    int port = atoi(argv[2]);

    // יצירת סוקט UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    cout << "Connected to molecule server at " << server_ip << ":" << port << endl;
    cout << "Enter commands like: DELIVER WATER 3\n";

    string line;
    while (getline(cin, line)) {
        line += "\n";
        sendto(sock, line.c_str(), line.length(), 0,
               (sockaddr*)&server_addr, sizeof(server_addr));

        char buffer[1024] = {0};
        socklen_t len = sizeof(server_addr);
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                         (sockaddr*)&server_addr, &len);

        if (n > 0) {
            buffer[n] = '\0';
            cout << "Server response: " << buffer << endl;
        } else {
            cerr << "Failed to receive response from server" << endl;
        }
    }

    close(sock);
    return 0;
}
