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
#include <termios.h>
#include <csignal>  // for the signals 
#include <cstdlib>  // exit()
#define MAX_VALUE 1000000000000000000
using namespace std;

/**
 * @brief Handles SIGINT (Ctrl+C) signal and exits cleanly.
 */
void handle_sigint([[maybe_unused]] int sig) {
    cout << "\n📤 Caught SIGINT (Ctrl+C). Exiting cleanly...\n";
    exit(0);  // makes the .gcda 
}

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

// Inventory of created molecules
map<string, unsigned long long> molecule_inventory;

/**
 * @brief Prints the current inventory of atoms.
 */
void print_inventory() {
    cout << "CARBON: " << atom_inventory["CARBON"]
         << ", OXYGEN: " << atom_inventory["OXYGEN"]
         << ", HYDROGEN: " << atom_inventory["HYDROGEN"] << endl;
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
    } catch (...) {
        cerr << "Error converting number" << endl;
    }
}

/**
 * @brief Handles a TCP command from the client (currently supports ADD).
 * @param command The full command line string received.
 */
void handle_tcp_command(const string& command) {
    istringstream iss(command);
    string action, atom_type, amount_string;
    iss >> action >> atom_type >> amount_string;
    transform(atom_type.begin(), atom_type.end(), atom_type.begin(), ::toupper);
    if (action != "ADD" || atom_inventory.find(atom_type) == atom_inventory.end()) {
        cerr << "Invalid command!" << endl;
        return;
    }
    // detect extra arguments
    string extra; 
    if (iss >> extra) {
        cerr << "Invalid command: too many arguments!" << endl;
        return;
    }
    add_atoms(atom_type, amount_string);
    print_inventory();
}


/**
 * @brief Handles a UDP DELIVER command and updates inventories accordingly.
 * @param command The full UDP command string.
 * @return A status string indicating success or the specific error.
 */
string handle_udp_command(const string& command) {
    istringstream iss(command);
    string action;
    iss >> action;
    if (action != "DELIVER") return "ERROR: Invalid command";

    vector<string> tokens;
    string token;
    while (iss >> token) tokens.push_back(token);
    if (tokens.size() < 2) return "ERROR: Invalid command format";

    string count_str = tokens.back();
    if (!all_of(count_str.begin(), count_str.end(), ::isdigit)) return "ERROR: Not a positive number";

    unsigned long long count = stoull(count_str);
    string molecule_name;
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        if (!molecule_name.empty()) molecule_name += " ";
        molecule_name += tokens[i];
    }

    string atom_name = "";
    string real_molecule = molecule_name;
    if (tokens.size() > 3) {
        atom_name = tokens[0];
        real_molecule = molecule_name.substr(atom_name.size() + 1); // Remove atom prefix if exists
    }

    if (molecule_recipes.find(real_molecule) == molecule_recipes.end()) {
        return "ERROR: Unknown molecule '" + real_molecule + "'";
    }

    const auto& recipe = molecule_recipes[real_molecule];
    map<string, unsigned long long> needed;
    for (const auto& [atom, per_mol] : recipe) needed[atom] += per_mol * count;

    if (!atom_name.empty()) {
        string upper_atom = atom_name;
        transform(upper_atom.begin(), upper_atom.end(), upper_atom.begin(), ::toupper);
        if (atom_inventory.find(upper_atom) == atom_inventory.end()) {
            return "ERROR: Invalid atom '" + upper_atom + "'";
        }
        needed[upper_atom] += count;
    }

    // Check if any atoms are missing for the delivery
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

    for (const auto& [atom, need_count] : needed)
        atom_inventory[atom] -= need_count;

    molecule_inventory[real_molecule] += count;
    print_inventory();
    return "OK: Delivered " + to_string(count) + " " + molecule_name + " molecules";
}
/**
 * @brief Computes how many full drinks of a type can be prepared with available atoms.
 * @param drink_name The name of the drink (e.g., "VODKA").
 * @return Number of drinks that can be prepared.
 */
unsigned long long compute_drink_count(const string& drink_name) {
    // Required atom counts for one drink
    unsigned long long needed_carbon = 0;
    unsigned long long needed_hydrogen = 0;
    unsigned long long needed_oxygen = 0;

    if (drink_name == "SOFT DRINK") {
        // Contains: WATER (H2O), CARBON DIOXIDE (CO2), GLUCOSE (C6H12O6)
        needed_carbon = 1 + 6;           // 1 from CO2, 6 from GLUCOSE
        needed_hydrogen = 2 + 12;        // 2 from WATER, 12 from GLUCOSE
        needed_oxygen = 1 + 2 + 6;       // 1 from WATER, 2 from CO2, 6 from GLUCOSE
    } else if (drink_name == "VODKA") {
        // Contains: WATER, ALCOHOL (C2H6O), GLUCOSE
        needed_carbon = 2 + 6;           // 2 from ALCOHOL, 6 from GLUCOSE
        needed_hydrogen = 2 + 6 + 12;    // 2 from WATER, 6 from ALCOHOL, 12 from GLUCOSE
        needed_oxygen = 1 + 1 + 6;       // 1 from WATER, 1 from ALCOHOL, 6 from GLUCOSE
    } else if (drink_name == "CHAMPAGNE") {
        // Contains: WATER, CO2, ALCOHOL
        needed_carbon = 1 + 2;           // 1 from CO2, 2 from ALCOHOL
        needed_hydrogen = 2 + 6;         // 2 from WATER, 6 from ALCOHOL
        needed_oxygen = 1 + 2 + 1;       // 1 from WATER, 2 from CO2, 1 from ALCOHOL
    } else {
        // Drink not recognized
        return 0;
    }

    // Calculate how many full drinks can be made based on current atom inventory
    unsigned long long from_carbon = atom_inventory["CARBON"] / needed_carbon;
    unsigned long long from_hydrogen = atom_inventory["HYDROGEN"] / needed_hydrogen;
    unsigned long long from_oxygen = atom_inventory["OXYGEN"] / needed_oxygen;

    // The limiting atom determines the maximum number of drinks possible
    return std::min({from_carbon, from_hydrogen, from_oxygen});
}

/**
 * @brief Main server loop. Listens on TCP and UDP ports and handles incoming commands.
 */
int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);  // Catch Ctrl+C
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <TCP_PORT> <UDP_PORT>" << endl;
        return 1;
    }
    int tcp_port = atoi(argv[1]);
    int udp_port = atoi(argv[2]);

    // Create and bind TCP socket
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_addr {};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);
    bind(tcp_sock, (sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(tcp_sock, 5);

    // Create and bind UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_addr {};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);
    bind(udp_sock, (sockaddr*)&udp_addr, sizeof(udp_addr));

    cout << "bar_drinks running on TCP port " << tcp_port << " and UDP port " << udp_port << "..." << endl;

    tcflush(STDIN_FILENO, TCIFLUSH); // Clear input buffer before select
    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(tcp_sock, &master);
    FD_SET(udp_sock, &master);
    FD_SET(STDIN_FILENO, &master);
    int fdmax = max({tcp_sock, udp_sock, STDIN_FILENO});

    vector<int> clients;

    while (true) {
        read_fds = master;
        FD_SET(STDIN_FILENO, &read_fds);
        if (select(fdmax + 1, &read_fds, nullptr, nullptr, nullptr) < 0) {
            perror("select");
            break;
        }

        for (int i = 0; i <= fdmax; ++i) {
            if (!FD_ISSET(i, &read_fds)) continue;

            if (i == tcp_sock) {
                // Handle new TCP connection
                int newfd = accept(tcp_sock, nullptr, nullptr);
                if (newfd < 0) {
                    perror("accept failed");
                    continue;
                }
                FD_SET(newfd, &master);
                fdmax = max({fdmax, newfd}); 
                clients.push_back(newfd);

            } else if (i == udp_sock) {
                // Handle UDP request
                char buf[1024] = {0};
                sockaddr_in client_addr {};
                socklen_t len = sizeof(client_addr);
                int n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&client_addr, &len);
                if (n > 0) {
                    string response = handle_udp_command(string(buf));
                    cout << response << endl;
                    sendto(udp_sock, response.c_str(), response.size(), 0, (sockaddr*)&client_addr, len);
                }

            } else if (i == STDIN_FILENO) {
                // Handle local user input from keyboard
                string line;
                getline(cin, line);
                transform(line.begin(), line.end(), line.begin(), ::toupper);
                istringstream iss(line);
                string command, drink;
                iss >> command;
                getline(iss, drink);
                drink.erase(0, drink.find_first_not_of(" "));
                transform(drink.begin(), drink.end(), drink.begin(), ::toupper);

                if (command == "GEN" && drink_recipes.find(drink) != drink_recipes.end()) {
                   unsigned long long count = compute_drink_count(drink);
                    cout << "Can prepare " << count << " " << drink << " drinks" << endl;
                } else {
                    cout << "Unknown drink command: " << line << endl;
                }

            } else {
                // Handle incoming TCP message from existing client
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