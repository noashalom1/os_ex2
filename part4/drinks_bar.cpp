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
#include <climits>
#include <cstdlib>
#define MAX_VALUE 1000000000000000000
using namespace std;

// Global inventory for atoms (Carbon, Oxygen, Hydrogen)
map<string, unsigned long long> atom_inventory = {
    {"CARBON", 0}, {"OXYGEN", 0}, {"HYDROGEN", 0}
};

// Molecule recipes: how many atoms of each type needed per molecule
map<string, map<string, int>> molecule_recipes = {
    {"WATER", {{"HYDROGEN", 2}, {"OXYGEN", 1}}},
    {"CARBON DIOXIDE", {{"CARBON", 1}, {"OXYGEN", 2}}},
    {"ALCOHOL", {{"CARBON", 2}, {"HYDROGEN", 6}, {"OXYGEN", 1}}},
    {"GLUCOSE", {{"CARBON", 6}, {"HYDROGEN", 12}, {"OXYGEN", 6}}}
};

// Drink recipes: drinks consist of multiple molecules
map<string, vector<string>> drink_recipes = {
    {"SOFT DRINK", {"WATER", "CARBON DIOXIDE", "GLUCOSE"}},
    {"VODKA", {"WATER", "ALCOHOL", "GLUCOSE"}},
    {"CHAMPAGNE", {"WATER", "CARBON DIOXIDE", "ALCOHOL"}}
};

map<string, unsigned long long> molecule_inventory; // Inventory of created molecules

/**
 * @brief Prints the current atom inventory.
 */
void print_inventory() {
    cout << "CARBON: " << atom_inventory["CARBON"]
         << ", OXYGEN: " << atom_inventory["OXYGEN"]
         << ", HYDROGEN: " << atom_inventory["HYDROGEN"] << endl;
}

/**
 * @brief Adds a specified count of molecules to the molecule inventory.
 */
void add_molecules_to_inventory(const string& molecule_name, unsigned long long count) {
    molecule_inventory[molecule_name] += count;
}

/**
 * @brief Adds a given amount of atoms to the inventory if valid.
 * @param atom_type The type of atom to add (CARBON, OXYGEN, HYDROGEN).
 * @param amount_string The amount as a string (validated and converted).
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
 * @brief Parses and validates a timeout value from string to integer.
 */
int set_timeout(const string& timeout) {
    if (!all_of(timeout.begin(), timeout.end(), ::isdigit)) {
        cerr << "Invalid command: amount must be a positive number!" << endl;
        return 0;
    }

    try {
        int time = stoull(timeout); 
        return time;
    } catch (const exception& e) {
        cerr << "Error converting number" << endl;
        return 0;
    }
}

/**
 * @brief Processes a TCP command for adding atoms.
 */
void handle_tcp_command(const string& command) {
    istringstream iss(command);
    string action, atom, amount_string;

    iss >> action >> atom >> amount_string;
    transform(atom.begin(), atom.end(), atom.begin(), ::toupper);

    if (action != "ADD" || atom_inventory.find(atom) == atom_inventory.end() || iss.fail()) {
        cerr << "Invalid TCP command!" << endl;
        return;
    }

    add_atoms(atom, amount_string);
    print_inventory();
}

/**
 * @brief Handles a UDP command for delivering molecules.
 * @return A status message indicating success or error.
 */
string handle_udp_command(const string& command) {
    istringstream iss(command);
    string action;
    iss >> action;

    if (action != "DELIVER") {
        return "ERROR: Invalid command";
    }

    vector<string> tokens;
    string token;
    while (iss >> token) tokens.push_back(token);

    if (tokens.size() < 2) return "ERROR: Invalid command format";

  
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

    string molecule_name;
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        if (!molecule_name.empty()) molecule_name += " ";
        molecule_name += tokens[i];
    }

    if (molecule_recipes.find(molecule_name) == molecule_recipes.end()) {
        return "ERROR: Unknown molecule '" + molecule_name + "'";
    }

    const auto& recipe = molecule_recipes[molecule_name];
    map<string, unsigned long long> needed;
    for (const auto& [atom, per_mol] : recipe) {
        needed[atom] += per_mol * count;
    }

    for (const auto& [atom, need_count] : needed) {
        if (atom_inventory[atom] < need_count) {
            return "ERROR: Not enough atoms – missing " + to_string(need_count - atom_inventory[atom]) + " " + atom;
        }
    }

    for (const auto& [atom, need_count] : needed) {
        atom_inventory[atom] -= need_count;
    }

    cout << "DELIVERED: " << count << " " << molecule_name << " molecules" << endl;
    add_molecules_to_inventory(molecule_name, count);
    print_inventory();
    return "OK: Delivered " + to_string(count) + " " + molecule_name + " molecules";
}

/**
 * @brief Computes how many drinks of the specified type can be made.
 * @return The number of drinks that can be prepared.
 */
int compute_drink_count(const string& drink_name) {
    if (drink_recipes.find(drink_name) == drink_recipes.end()) return 0;

    int min_count = INT_MAX;
    for (const string& mol : drink_recipes[drink_name]) {
        if (molecule_inventory.find(mol) == molecule_inventory.end()) return 0;
        min_count = min(min_count, static_cast<int>(molecule_inventory[mol]));
    }
    return min_count;
}

/**
 * @brief Main server function: initializes sockets, parses flags, and runs event loop.
 */
int main(int argc, char* argv[]) {
    int tcp_port = -1, udp_port = -1;
    int timeout = -1;
    int opt;
    while ((opt = getopt(argc, argv, "T:U:o:c:h:t:")) != -1) {
        switch (opt) {
            case 'T': tcp_port = atoi(optarg); break;
            case 'U': udp_port = atoi(optarg); break;
            case 'o': add_atoms("OXYGEN", string(optarg)); break;
            case 'c': add_atoms("CARBON", string(optarg)); break;
            case 'h': add_atoms("HYDROGEN", string(optarg)); break;
            case 't': timeout = set_timeout(string(optarg)); break;
            default:
                cerr << "Usage: ./drinks_bar -T <tcp_port> -U <udp_port> [-o num] [-c num] [-h num] [-t timeout]" << endl;
                return 1;
        }
    }

    if (tcp_port == -1 || udp_port == -1) {
        cerr << "ERROR: TCP and UDP ports are required (use -T and -U)" << endl;
        return 1;
    }

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);
    bind(tcp_sock, (sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(tcp_sock, 5);

    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);
    bind(udp_sock, (sockaddr*)&udp_addr, sizeof(udp_addr));

    cout << "bar_drinks running on TCP port " << tcp_port
         << " and UDP port " << udp_port << "..." << endl;
    
    print_inventory();

    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(tcp_sock, &master);
    FD_SET(udp_sock, &master);
    FD_SET(STDIN_FILENO, &master);
    int fdmax = max({tcp_sock, udp_sock, STDIN_FILENO});

    timeval* timeout_ptr = nullptr;
    timeval timeout_val{};
    if (timeout > 0) {
        timeout_val.tv_sec = timeout;
        timeout_val.tv_usec = 0;
        timeout_ptr = &timeout_val;
    }

    vector<int> clients;

    while (true) {
        read_fds = master;
        int activity = select(fdmax + 1, &read_fds, nullptr, nullptr, timeout_ptr);
        if (activity < 0) {
            perror("select");
            break;
        } else if (activity == 0) {
            cout << "Timeout reached with no activity. Server exiting." << endl;
            break;
        }

        for (int i = 0; i <= fdmax; ++i) {
            if (!FD_ISSET(i, &read_fds)) continue;

            if (i == tcp_sock) {
                int newfd = accept(tcp_sock, nullptr, nullptr);
                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;
                clients.push_back(newfd);
                if (timeout > 0) {
                timeout_val.tv_sec = timeout;
                timeout_val.tv_usec = 0;
                timeout_ptr = &timeout_val;
                }
            } else if (i == udp_sock) {
                char buf[1024] = {0};
                sockaddr_in client_addr{};
                socklen_t len = sizeof(client_addr);
                int n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&client_addr, &len);
                if (n > 0) {
                    string response = handle_udp_command(string(buf));
                    if (response.rfind("ERROR", 0) == 0) cerr << response << endl;
                    sendto(udp_sock, response.c_str(), response.size(), 0, (sockaddr*)&client_addr, len);
                    if (timeout > 0) {
                    timeout_val.tv_sec = timeout;
                    timeout_val.tv_usec = 0;
                    timeout_ptr = &timeout_val;
                    }
                }
            } else if (i == STDIN_FILENO) {
                string line;
                getline(cin, line);
                transform(line.begin(), line.end(), line.begin(), ::toupper);
                istringstream iss(line);
                string command, drink;
                iss >> command;
                getline(iss, drink);
                drink.erase(0, drink.find_first_not_of(" "));
                transform(command.begin(), command.end(), command.begin(), ::toupper);
                transform(drink.begin(), drink.end(), drink.begin(), ::toupper);
                if (command == "GEN" && drink_recipes.find(drink) != drink_recipes.end()) {
                    int count = compute_drink_count(drink);
                    cout << "Can prepare " << count << " " << drink << " drinks" << endl;
                } else {
                    cout << "Unknown drink command: " << line << endl;
                }
                if (timeout > 0) {
                timeout_val.tv_sec = timeout;
                timeout_val.tv_usec = 0;
                timeout_ptr = &timeout_val;
                }
            } else {
                char buf[1024] = {0};
                int n = recv(i, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    close(i);
                    FD_CLR(i, &master);
                } else {
                    handle_tcp_command(string(buf));
                    if (timeout > 0) {
                    timeout_val.tv_sec = timeout;
                    timeout_val.tv_usec = 0;
                    timeout_ptr = &timeout_val;
                    }
                }
            }
        }
    }

    return 0;
}
