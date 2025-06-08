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
#include <sys/un.h>
#include <csignal>  // for the signals 
#include <cstdlib>  // exit()

#define MAX_VALUE 1000000000000000000
#define UNIX_STREAM_PATH "/tmp/drinks_bar_stream.sock"
#define UNIX_DGRAM_PATH "/tmp/drinks_bar_dgram.sock"

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
 * @brief Converts timeout string to integer seconds.
 * @param timeout Timeout as string.
 * @return Timeout in seconds.
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
 * @brief Creates and binds a UNIX stream socket.
 * @return File descriptor of the socket.
 */
int create_unix_stream_socket() {
    unlink(UNIX_STREAM_PATH); 
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("UDS STREAM socket");
        exit(1);
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_STREAM_PATH, sizeof(addr.sun_path) - 1);
    bind(sock, (sockaddr*)&addr, sizeof(addr));
    listen(sock, 5);
    return sock;
}

/**
 * @brief Creates and binds a UNIX datagram socket.
 * @return File descriptor of the socket.
 */
int create_unix_dgram_socket() {
    unlink(UNIX_DGRAM_PATH);
    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDS DGRAM socket");
        exit(1);
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_DGRAM_PATH, sizeof(addr.sun_path) - 1);
    bind(sock, (sockaddr*)&addr, sizeof(addr));
    return sock;
}

/**
 * @brief Handles TCP command (e.g., "ADD OXYGEN 5").
 * @param command The full command string.
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
    // detect extra arguments
    string extra; 
    if (iss >> extra) {
        cerr << "Invalid command: too many arguments!" << endl;
        return;
    }

    add_atoms(atom, amount_string);
    print_inventory();
}

/**
 * @brief Handles UDP command to deliver molecules.
 * @param command The full command string.
 * @return Response to be sent back to the client.
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
 * @brief Main entry point of the server. Handles TCP, UDP, UDS, and stdin.
 */
int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);  // Catch Ctrl+C
    int tcp_port = -1, udp_port = -1;
    int timeout = -1;
    int opt;
    string stream_path, dgram_path;

    while ((opt = getopt(argc, argv, "T:U:o:c:h:t:s:d:")) != -1) { // Parse command-line arguments
        switch (opt) {
            case 'T': tcp_port = atoi(optarg); break;
            case 'U': udp_port = atoi(optarg); break;
            case 'o': add_atoms("OXYGEN", string(optarg)); break;
            case 'c': add_atoms("CARBON", string(optarg)); break;
            case 'h': add_atoms("HYDROGEN", string(optarg)); break;
            case 't': timeout = set_timeout(string(optarg)); break;
            case 's': stream_path = optarg; break;
            case 'd': dgram_path = optarg; break;
            default:
                cerr << "Usage: ./drinks_bar -T <tcp_port> -U <udp_port> [-o num] [-c num] [-h num] [-t timeout]" << endl;
                return 1;
        }
    }

    if (tcp_port == -1 || udp_port == -1) {
        cerr << "ERROR: TCP and UDP ports are required (use -T and -U)" << endl;
        return 1;
    }
    
    fd_set master, read_fds;
    FD_ZERO(&master); // Clear fd set
    int uds_stream_sock = -1, uds_dgram_sock = -1;
    int fdmax = STDIN_FILENO;

    // Setup UDS stream socket if needed
    if (!stream_path.empty()) { 
        unlink(stream_path.c_str()); // Remove previous socket file
        uds_stream_sock = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, stream_path.c_str(), sizeof(addr.sun_path) - 1);
        bind(uds_stream_sock, (sockaddr*)&addr, sizeof(addr));
        listen(uds_stream_sock, 5);
        if (uds_stream_sock != -1) FD_SET(uds_stream_sock, &master);
        if (uds_dgram_sock != -1) FD_SET(uds_dgram_sock, &master);

        fdmax = max(fdmax, uds_stream_sock);
        cout << "Listening on UDS stream: " << stream_path << endl;
    }

    // Setup UDS datagram socket if needed
    if (!dgram_path.empty()) {
        unlink(dgram_path.c_str());
        uds_dgram_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, dgram_path.c_str(), sizeof(addr.sun_path) - 1);
        bind(uds_dgram_sock, (sockaddr*)&addr, sizeof(addr));
        if (uds_stream_sock != -1) fdmax = max(fdmax, uds_stream_sock);
        if (uds_dgram_sock != -1) fdmax = max(fdmax, uds_dgram_sock);

        fdmax = max(fdmax, uds_dgram_sock);
        cout << "Listening on UDS datagram: " << dgram_path << endl;
    }

    // Setup TCP socket
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);
    if (bind(tcp_sock, (sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
    perror("bind TCP");
    return 1;
    }
    listen(tcp_sock, 5);

    // Setup UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);
    if(bind(udp_sock, (sockaddr*)&udp_addr, sizeof(udp_addr)) < 0){
        perror("bind UDP");
        return 1;
    }

    cout << "bar_drinks running on TCP port " << tcp_port
         << " and UDP port " << udp_port << "..." << endl;
    
    print_inventory(); // Display starting inventory

    // Add all listening sockets to the master set
    FD_SET(tcp_sock, &master);
    FD_SET(udp_sock, &master);
    FD_SET(STDIN_FILENO, &master);
    fdmax = max({tcp_sock, udp_sock, STDIN_FILENO});
    FD_SET(uds_stream_sock, &master);
    FD_SET(uds_dgram_sock, &master);
    fdmax = max({fdmax, uds_stream_sock, uds_dgram_sock});


    timeval* timeout_ptr = nullptr;
    timeval timeout_val{};
    if (timeout > 0) {
        timeout_val.tv_sec = timeout;
        timeout_val.tv_usec = 0;
        timeout_ptr = &timeout_val;
    }
    vector<int> clients;

    // === Event Loop ===
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
                        else if (i == uds_stream_sock) {
                int newfd = accept(uds_stream_sock, nullptr, nullptr);
                if (newfd >= 0) {
                    FD_SET(newfd, &master);
                    if (newfd > fdmax) fdmax = newfd;
                    clients.push_back(newfd);
                    if (timeout > 0) {
                        timeout_val.tv_sec = timeout;
                        timeout_val.tv_usec = 0;
                        timeout_ptr = &timeout_val;
                    }
                }
            } else if (i == uds_dgram_sock) {
                char buf[1024] = {0};
                sockaddr_un client_addr{};
                socklen_t len = sizeof(client_addr);
                int n = recvfrom(uds_dgram_sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&client_addr, &len);
                if (n > 0) {
                    string response = handle_udp_command(string(buf));
                    if (response.rfind("ERROR", 0) == 0) cerr << response << endl;
                    sendto(uds_dgram_sock, response.c_str(), response.size(), 0, (sockaddr*)&client_addr, len);
                    if (timeout > 0) {
                        timeout_val.tv_sec = timeout;
                        timeout_val.tv_usec = 0;
                        timeout_ptr = &timeout_val;
                    }
                }
            }

            else if (i == tcp_sock) {
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
                    unsigned long long count = compute_drink_count(drink);
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
    // Cleanup
    if (uds_stream_sock != -1) {
    close(uds_stream_sock);
    unlink(stream_path.c_str());
    }
    if (uds_dgram_sock != -1) {
        close(uds_dgram_sock);
        unlink(dgram_path.c_str());
    }

    close(tcp_sock);
    close(udp_sock);


    return 0;
}
