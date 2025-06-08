#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <netdb.h>

using namespace std;

int main(int argc, char* argv[]) {
    string hostname, uds_path;
    int port = -1;
    int opt;

    // Parse command-line options -h (hostname), -p (port) and -f(UDS socket path)
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

    // Check for missing arguments
    if ((!hostname.empty() || port != -1) && !uds_path.empty()) {
        cerr << "ERROR: Cannot use both -h/-p and -f options together!" << endl;
        return 1;
    }
    
    if ((hostname.empty() || port == -1) && uds_path.empty()) {
        cerr << "ERROR: Must specify either hostname/port or UDS path!" << endl;
        return 1;
    }

    int sock;
    sockaddr_storage server_addr{};
    string client_path;  // For UDS client socket file (needed for binding)

    socklen_t addr_len;

    if (uds_path.empty()) {
        // --- UDP ---
        addrinfo hints{}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        string port_str = to_string(port);
        int status = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &res);
        if (status != 0) {
            cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
            return 1;
        }

        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) {
            perror("socket (UDP)");
            freeaddrinfo(res);
            return 1;
        }

        memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
        addr_len = res->ai_addrlen;

        cout << "Connected to UDP server at " << hostname << ":" << port << endl;
        freeaddrinfo(res);
        } else {
            // --- UDS ---
            sock = socket(AF_UNIX, SOCK_DGRAM, 0);
            if (sock < 0) {
                perror("socket (UDS)");
                return 1;
            }

            // Bind client socket to unique path
            client_path = "/tmp/uds_client_" + to_string(getpid());
            sockaddr_un client_addr{};
            client_addr.sun_family = AF_UNIX;
            strncpy(client_addr.sun_path, client_path.c_str(), sizeof(client_addr.sun_path) - 1);
            if (bind(sock, (sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
                perror("bind (client UDS) failed");
                return 1;
            }

            // Set server address
            sockaddr_un* addr = (sockaddr_un*)&server_addr;
            addr->sun_family = AF_UNIX;
            strncpy(addr->sun_path, uds_path.c_str(), sizeof(addr->sun_path) - 1);
            addr_len = sizeof(sockaddr_un);

            cout << "Connected to UDS server at " << uds_path << endl;
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
        sockaddr_storage from_addr{};
        socklen_t from_len = sizeof(from_addr);
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&from_addr, &from_len);

        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }

        buffer[n] = '\0';
        cout << "Server response: " << buffer << endl;
    }
    if (!client_path.empty()) {
    unlink(client_path.c_str());
}


    close(sock);
    return 0;
}