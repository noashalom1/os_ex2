#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>     


using namespace std;
/**
 * Entry point for the TCP client program.
 * The client connects to a server using hostname and port provided via -h and -p flags.
 * Once connected, it reads user commands from stdin and sends them over the TCP socket.
 */
int main(int argc, char* argv[]) {
    string hostname;
    int port = -1;
    int opt;

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

    if (hostname.empty() || port == -1) { // Ensure both hostname and port were provided.
        cerr << "ERROR: Both hostname (-h) and port (-p) are required!" << endl;
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket.
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    addrinfo hints{}, *res; // Prepare address info for the server.
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    string port_str = to_string(port); // Resolve hostname and port to sockaddr structure.
    int status = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &res);
    if (status != 0) {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        return 1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) { // Connect to the resolved server address.
        perror("connect failed");
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res); // Free the address info memory once connected.


    cout << "Connected to server at " << hostname << ":" << port << endl;
    cout << "Type commands (e.g., ADD CARBON 50):\n";

    string line; // Read lines from user input and send them to the server.
    while (getline(cin, line)) {
        line += "\n";
        send(sock, line.c_str(), line.size(), 0);
    }

    close(sock); // Close the socket before exiting.
    return 0;
}
