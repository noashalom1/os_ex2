#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>

using namespace std;

int main(int argc, char* argv[]) {
    string hostname, uds_path;
    int port = -1;
    int opt;

    while ((opt = getopt(argc, argv, "h:p:f:")) != -1) {
        switch (opt) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                uds_path = optarg;
                break;
            default:
                cerr << "Usage: " << argv[0] << " -h <HOST> -p <PORT> | -f <UDS socket path>" << endl;
                return 1;
        }
    }

    // סתירה בין סוגי התחברות
    if ((!hostname.empty() || port != -1) && !uds_path.empty()) {
        cerr << "ERROR: Cannot use both -h/-p and -f options together!" << endl;
        return 1;
    }

    // קלט לא מספיק
    if ((hostname.empty() || port == -1) && uds_path.empty()) {
        cerr << "ERROR: Must specify either hostname/port or UDS path!" << endl;
        return 1;
    }

    int sock;
    sockaddr_storage server_addr{};
    socklen_t addr_len;

    if (!uds_path.empty()) {
        sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("socket (UDS)");
            return 1;
        }

        sockaddr_un* addr = (sockaddr_un*)&server_addr;
        addr->sun_family = AF_UNIX;
        strncpy(addr->sun_path, uds_path.c_str(), sizeof(addr->sun_path) - 1);
        addr->sun_path[sizeof(addr->sun_path) - 1] = '\0'; // הגנה
        addr_len = sizeof(sockaddr_un);

        cout << "Connected to UDS datagram at " << uds_path << endl;

    } else {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("socket (INET)");
            return 1;
        }

        sockaddr_in* addr = (sockaddr_in*)&server_addr;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        if (inet_pton(AF_INET, hostname.c_str(), &addr->sin_addr) <= 0) {
            cerr << "Invalid address/hostname" << endl;
            return 1;
        }
        addr_len = sizeof(sockaddr_in);

        cout << "Connected to UDP server at " << hostname << ":" << port << endl;
    }

    cout << "Type commands (e.g., DELIVER WATER 2):" << endl;

    string line;
    while (getline(cin, line)) {
        line += "\n";

        if (sendto(sock, line.c_str(), line.length(), 0, (sockaddr*)&server_addr, addr_len) < 0) {
            perror("sendto failed");
            continue;
        }

        char buffer[1024] = {0};
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }

        buffer[n] = '\0';
        cout << "Server response: " << buffer << endl;
    }

    close(sock);
    return 0;
}
