#include <iostream>
#include <string>
#include <unistd.h>
#include <netdb.h>      
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  

using namespace std;

/**
 * Main function for the TCP client.
 * Connects to the given host and port using TCP,
 * then reads lines from the user and sends them to the server.
 */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <HOST> <PORT>\n";
        return 1;
    }

    const char* port = argv[2];

    addrinfo hints{}, *res;
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_STREAM; // Use TCP

    // Resolve the server address and port
    int status = getaddrinfo(argv[1], port, &hints, &res);
    if (status != 0) {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        return 1;
    }

    // Create a socket using the resolved address info
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    // Attempt to connect to the server
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect failed");
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res); // No longer needed after connection

    cout << "Connected to server. Type commands (e.g., ADD CARBON 50):\n";

    string line;
    while (getline(cin, line)) {
        line += "\n"; // Append newline to each command
        send(sock, line.c_str(), line.size(), 0); // Send command to server
    }

    close(sock); // Close the socket when done
    return 0;
}
