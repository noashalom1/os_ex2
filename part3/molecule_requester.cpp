#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h> 


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

    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP

    string port_str = to_string(port);
    int status = getaddrinfo(server_ip, port_str.c_str(), &hints, &res);
    if (status != 0) {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        return 1;
    }

    cout << "Connected to molecule server at " << server_ip << ":" << port << endl;
    cout << "Enter commands like: DELIVER WATER 3\n";

    string line;
    while (getline(cin, line)) {
        line += "\n";
        sendto(sock, line.c_str(), line.length(), 0, res->ai_addr, res->ai_addrlen);

        sockaddr_storage response_addr;
        socklen_t len = sizeof(response_addr);
        char buffer[1024] = {0};
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&response_addr, &len);

        if (n > 0) {
            buffer[n] = '\0';
            cout << "Server response: " << buffer << endl;
        } else {
            cerr << "Failed to receive response from server" << endl;
        }
    }
    freeaddrinfo(res); 

    close(sock);
    return 0;
}
