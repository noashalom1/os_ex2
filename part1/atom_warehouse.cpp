#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sstream>
#include <algorithm>
#include <csignal>  // for the signals 
#include <cstdlib>  // exit()
#define MAX_VALUE 1000000000000000000

using namespace std;

/**
 * @brief Handles SIGINT (Ctrl+C) signal and exits cleanly.
 */
void handle_sigint([[maybe_unused]] int sig) {
    cout << "\n📤 Caught SIGINT (Ctrl+C). Exiting cleanly...\n";
    exit(0);  // Makes the .gcda 
}

map<string, unsigned long long> atom_inventory = {
    {"CARBON", 0}, {"OXYGEN", 0}, {"HYDROGEN", 0}
};

/**
 * @brief Prints the current inventory of atoms.
 */
void print_inventory() {
    cout << "CARBON: " << atom_inventory["CARBON"]
         << ", OXYGEN: " << atom_inventory["OXYGEN"]
         << ", HYDROGEN: " << atom_inventory["HYDROGEN"] << endl;
}

/**
 * @brief Adds atoms to the inventory if the input is valid.
 * 
 * @param atom_type The type of atom to add (e.g., "CARBON").
 * @param amount_string String representing the number of atoms to add.
 */
void add_atoms(const string& atom_type, const string& amount_string) {
    if (!all_of(amount_string.begin(), amount_string.end(), ::isdigit)) {
        cerr << "Invalid command: amount must be a positive number!" << endl;
        return;
    }

    try {
        unsigned long long amount = stoull(amount_string); 
        if (atom_inventory[atom_type] + amount > MAX_VALUE) {
            cerr << "Invalid command: not enough place for the atoms!" << endl;
            return;
        }

        atom_inventory[atom_type] += amount;
    } catch (const exception& e) {
        cerr << "Error converting number" << endl;
    }
}

/**
 * @brief Parses and executes a command from the client.
 * 
 * @param command The full command string received.
 */
void handle_command(const string& command) {
    istringstream iss(command);
    string action, atom_type, amount_string;
  
    iss >> action >> atom_type >> amount_string;

    if (action != "ADD" || iss.fail()) {
        cerr << "Invalid command!" << endl;
        return;
    }

    transform(atom_type.begin(), atom_type.end(), atom_type.begin(), ::toupper);

    if (atom_inventory.find(atom_type) != atom_inventory.end()) {
        add_atoms(atom_type, amount_string);
        print_inventory();
    } else {
        cerr << "Unknown atom type!" << endl;
    }
}

/**
 * @brief Entry point of the TCP server program.
 * 
 * @param argc Number of arguments (expects 2).
 * @param argv Argument values (expects the port number).
 * @return int Exit status.
 */
int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);  // Catch Ctrl+C

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <PORT>" << endl;
        return 1;
    }

    int port = atoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }

    listen(server_fd, 5);

    fd_set master_set, read_fds;
    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    int fdmax = server_fd;

    vector<int> clients;

    cout << "Server is listening on port " << port << "...\n";

    while (true) {
        read_fds = master_set;
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select error");
            return 1;
        }

        for (int i = 0; i <= fdmax; ++i) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
                    // New client connection
                    int newfd = accept(server_fd, NULL, NULL);
                    if (newfd != -1) {
                        FD_SET(newfd, &master_set);
                        if (newfd > fdmax) fdmax = newfd;
                        clients.push_back(newfd);
                    }
                } else {
                    // Client sent data
                    char buf[1024] = {0};
                    int nbytes = recv(i, buf, sizeof(buf), 0);
                    if (nbytes <= 0) {
                        close(i);
                        FD_CLR(i, &master_set);
                    } else {
                        handle_command(string(buf));
                    }
                }
            }
        }
    }
    return 0;
}
