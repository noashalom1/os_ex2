#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

int main(int argc, char* argv[]) {
    string hostname;
    int port = -1;
    int opt;

    // ניתוח הדגלים -h ו-p
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

    if (hostname.empty() || port == -1) {
        cerr << "ERROR: Both hostname (-h) and port (-p) are required!" << endl;
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
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

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        return 1;
    }

    cout << "Connected to server at " << hostname << ":" << port << endl;
    cout << "Type commands (e.g., ADD CARBON 50):\n";

    string line;
    while (getline(cin, line)) {
        line += "\n";
        send(sock, line.c_str(), line.size(), 0);
    }

    close(sock);
    return 0;
}
