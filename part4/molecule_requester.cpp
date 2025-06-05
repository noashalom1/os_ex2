#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>

using namespace std;

int main(int argc, char* argv[]) {
    string hostname;
    int port = -1;
    int opt;

    // Parse command-line options -h (hostname) and -p (port)
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

    // Check for missing arguments
    if (hostname.empty() || port == -1) {
        cerr << "ERROR: Both hostname (-h) and port (-p) are required!" << endl;
        return 1;
    }

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    // Resolve server address
    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP

    string port_str = to_string(port);
    int status = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &res);
    if (status != 0) {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        return 1;
    }

    cout << "Connected to molecule server at " << hostname << ":" << port << endl;
    cout << "Enter commands like: DELIVER WATER 3\n";

    string line;
    while (getline(cin, line)) {
        line += "\n"; // Add newline as required by protocol
        sendto(sock, line.c_str(), line.length(), 0,
               res->ai_addr, res->ai_addrlen); // Send UDP packet

        sockaddr_storage response_addr{};
        socklen_t len = sizeof(response_addr);
        char buffer[1024] = {0};

        // Receive response from server
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&response_addr, &len);
        if (n > 0) {
            buffer[n] = '\0'; // Null-terminate the response
            cout << "Server response: " << buffer << endl;
        } else {
            cerr << "Failed to receive response from server" << endl;
        }
    }

    freeaddrinfo(res); // Clean up address info
    close(sock);       // Close the socket
    return 0;
}
