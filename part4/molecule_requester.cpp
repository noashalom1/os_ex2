#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

using namespace std;

int main(int argc, char* argv[]) {
    string hostname;
    int port = -1;
    int opt;

    // קבלת הדגלים -h ו־-p
    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                cerr << "Usage: " << argv[0] << " -h <HOSTNAME/IP> -p <PORT>" << endl;
                return 1;
        }
    }

    // בדיקת תקינות קלט
    if (hostname.empty() || port == -1) {
        cerr << "ERROR: Both hostname (-h) and port (-p) are required!" << endl;
        return 1;
    }

    // יצירת סוקט UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, hostname.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Invalid address/hostname" << endl;
        return 1;
    }

    cout << "Connected to molecule server at " << hostname << ":" << port << endl;
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
