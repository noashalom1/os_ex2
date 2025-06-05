#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#define MAX_VALUE 1000000000000000000
using namespace std;

// Global atom inventory, initialized to 0 for each atom type
map<string, unsigned long long> atom_inventory = {
    {"CARBON", 0}, {"OXYGEN", 0}, {"HYDROGEN", 0}
};

// Recipe book for building molecules from atoms
map<string, map<string, int>> molecule_recipes = {
    {"WATER", {{"HYDROGEN", 2}, {"OXYGEN", 1}}},
    {"CARBON DIOXIDE", {{"CARBON", 1}, {"OXYGEN", 2}}},
    {"ALCOHOL", {{"CARBON", 2}, {"HYDROGEN", 6}, {"OXYGEN", 1}}},
    {"GLUCOSE", {{"CARBON", 6}, {"HYDROGEN", 12}, {"OXYGEN", 6}}}
};

/** 
 * Prints the current atom inventory to the console.
 */
void print_inventory() {
    cout << "CARBON: " << atom_inventory["CARBON"]
         << ", OXYGEN: " << atom_inventory["OXYGEN"]
         << ", HYDROGEN: " << atom_inventory["HYDROGEN"] << endl;
}

/**
 * Adds a specified amount of atoms to the inventory.
 * Validates the input and ensures inventory won't overflow.
 */
void add_atoms(const string& atom_type, const string& amount_string) {
    if (!all_of(amount_string.begin(), amount_string.end(), ::isdigit)) {
        cerr << "Invalid command: amount must be a positive number!" << endl;
        return;
    }

    try {
        unsigned long long amount = stoull(amount_string);  // Convert string to unsigned long long
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
 * Handles TCP commands from clients (e.g., "ADD OXYGEN 50").
 */
void handle_tcp_command(const string& command) {
    istringstream iss(command);
    string action, atom_type, amount_string;
  
    iss >> action >> atom_type >> amount_string;
    
    if (action != "ADD" || iss.fail()) {
        cerr << "Invalid command!" << endl;
        return;
    }

    transform(atom_type.begin(), atom_type.end(), atom_type.begin(), ::toupper); // Convert atom name to uppercase

    if (atom_inventory.find(atom_type) != atom_inventory.end()) {
        add_atoms(atom_type, amount_string);
        print_inventory();
    } else {
        cerr << "Unknown atom type!" << endl;
    }
}

/**
 * Handles UDP commands for delivering molecules.
 * Validates input and updates inventory if enough atoms are available.
 */
string handle_udp_command(const string& command) {
    istringstream iss(command);
    string action;
    iss >> action;

    if (action != "DELIVER") {
        cerr << "Invalid UDP command!" << endl;
        return "ERROR: Invalid command";
    }

    vector<string> tokens;
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() < 2) {
        cerr << "Invalid UDP command format!" << endl;
        return "ERROR: Invalid command format";
    }

    string count_str = tokens.back();

    if (!all_of(count_str.begin(), count_str.end(), ::isdigit)) {
        return "ERROR: Not a positive number";
    }

    unsigned long long count;
    try {
        count = stoull(count_str);
    } catch (const exception& e) {
        return "ERROR: Conversion failed";
    }

    // Reconstruct molecule name (excluding atom if provided)
    string molecule_name;
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        if (!molecule_name.empty()) molecule_name += " ";
        molecule_name += tokens[i];
    }

    string atom_name = "";
    string real_molecule = molecule_name;
    if (tokens.size() > 3) {
        atom_name = tokens[0];
        real_molecule = molecule_name.substr(atom_name.size() + 1);  // Skip atom name
    }

    if (molecule_recipes.find(real_molecule) == molecule_recipes.end()) {
        return "ERROR: Unknown molecule '" + real_molecule + "'";
    }

    const auto& recipe = molecule_recipes[real_molecule];

    map<string, unsigned long long> needed;
    for (const auto& [atom, per_mol] : recipe) {
        needed[atom] += per_mol * count;
    }

    if (!atom_name.empty()) {
        string upper_atom = atom_name;
        transform(upper_atom.begin(), upper_atom.end(), upper_atom.begin(), ::toupper);
        if (atom_inventory.find(upper_atom) == atom_inventory.end()) {
            return "ERROR: Invalid atom '" + upper_atom + "'";
        }
        needed[upper_atom] += count;
    }

    // Check for missing atoms
    vector<string> missing_atoms;
    for (const auto& [atom, need_count] : needed) {
        if (atom_inventory[atom] < need_count) {
            unsigned long long missing = need_count - atom_inventory[atom];
            missing_atoms.push_back(to_string(missing) + " " + atom);
        }
    }

    if (!missing_atoms.empty()) {
        string error = "ERROR: Not enough atoms – missing ";
        for (size_t i = 0; i < missing_atoms.size(); ++i) {
            if (i > 0) error += ", ";
            error += missing_atoms[i];
        }
        return error;
    }

    // Deduct required atoms
    for (const auto& [atom, need_count] : needed) {
        atom_inventory[atom] -= need_count;
    }

    print_inventory();
    return "OK: Delivered " + to_string(count) + " " + molecule_name + " molecules";
}

/**
 * Main server loop: listens for TCP and UDP connections,
 * handles incoming commands from clients.
 */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <TCP_PORT> <UDP_PORT>" << endl;
        return 1;
    }

    int tcp_port = atoi(argv[1]);
    int udp_port = atoi(argv[2]);

    // Create TCP socket
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_addr {};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);
    bind(tcp_sock, (sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(tcp_sock, 5);

    // Create UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_addr {};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);
    bind(udp_sock, (sockaddr*)&udp_addr, sizeof(udp_addr));

    cout << "Server listening on TCP port " << tcp_port
         << " and UDP port " << udp_port << "..." << endl;

    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(tcp_sock, &master);
    FD_SET(udp_sock, &master);
    int fdmax = max(tcp_sock, udp_sock);

    vector<int> clients;

    while (true) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, nullptr, nullptr, nullptr) < 0) {
            perror("select");
            break;
        }

        for (int i = 0; i <= fdmax; ++i) {
            if (!FD_ISSET(i, &read_fds)) continue;

            if (i == tcp_sock) {
                // New TCP connection
                int newfd = accept(tcp_sock, nullptr, nullptr);
                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;
                clients.push_back(newfd);
            } else if (i == udp_sock) {
                // Incoming UDP message
                char buf[1024] = {0};
                sockaddr_in client_addr {};
                socklen_t len = sizeof(client_addr);
                int n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&client_addr, &len);
                if (n > 0) {
                    string response = handle_udp_command(string(buf));
                    if (response.rfind("ERROR", 0) == 0) {
                        cerr << response << endl;
                    } else {
                        cout << response << endl;  // Success output
                    }

                    sendto(udp_sock, response.c_str(), response.size(), 0, (sockaddr*)&client_addr, len);
                }

            } else {
                // Incoming TCP data
                char buf[1024] = {0};
                int n = recv(i, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    close(i);
                    FD_CLR(i, &master);
                } else {
                    handle_tcp_command(string(buf));
                }
            }
        }
    }

    return 0;
}
