#include <iostream>
#include <string>
#include <unistd.h>
#include <netdb.h>      
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <HOST> <PORT>\n";
        return 1;
    }

    const char* port = argv[2];

    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;    
    hints.ai_socktype = SOCK_STREAM; // TCP

    int status = getaddrinfo(argv[1], port, &hints, &res);
    if (status != 0) {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        return 1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect failed");
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);


    cout << "Connected to server. Type commands (e.g., ADD CARBON 50):\n";

    string line;
    while (getline(cin, line)) {
        line += "\n";
        send(sock, line.c_str(), line.size(), 0);
    }

    close(sock);
    return 0;
}
