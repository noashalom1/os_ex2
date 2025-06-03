#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>

using namespace std;

int main(int argc, char* argv[]) {
    string hostname;
    int port = -1;
    string uds_path;
    int opt;

    // ניתוח הדגלים -h, -p, -f
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
                cerr << "Usage: " << argv[0] << " [-h <HOSTNAME/IP> -p <PORT>] | [-f <UDS_PATH>]" << endl;
                return 1;
        }
    }

    // בדיקה לסתירה
    if ((!hostname.empty() || port != -1) && !uds_path.empty()) {
        cerr << "ERROR: Cannot use both hostname/port and UDS path at the same time!" << endl;
        return 1;
    }

    // בדיקה לקלט חסר
    if ((hostname.empty() || port == -1) && uds_path.empty()) {
        cerr << "ERROR: Must specify either hostname/port or UDS path!" << endl;
        return 1;
    }

    int sock;
    if (!uds_path.empty()) {
        // התחברות דרך UDS
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket (UDS) failed");
            return 1;
        }

        sockaddr_un server_addr{};
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, uds_path.c_str(), sizeof(server_addr.sun_path) - 1);

        if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect (UDS) failed");
            return 1;
        }

        cout << "Connected to UDS server at " << uds_path << endl;
    } else {
        // התחברות דרך TCP
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket (INET) failed");
            return 1;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, hostname.c_str(), &server_addr.sin_addr) <= 0) {
            cerr << "Invalid address/hostname" << endl;
            return 1;
        }

        if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect (INET) failed");
            return 1;
        }

        cout << "Connected to TCP server at " << hostname << ":" << port << endl;
    }

    cout << "Type commands (e.g., ADD CARBON 50):" << endl;

    string line;
    while (getline(cin, line)) {
        line += "\n";
        send(sock, line.c_str(), line.size(), 0);
    }

    close(sock);
    return 0;
}
