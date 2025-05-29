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

using namespace std;

map<string, unsigned long long> atom_inventory = {
    {"CARBON", 0}, {"OXYGEN", 0}, {"HYDROGEN", 0}
};

map<string, map<string, int>> molecule_recipes = {
    {"WATER", {{"HYDROGEN", 2}, {"OXYGEN", 1}}},
    {"CARBON DIOXIDE", {{"CARBON", 1}, {"OXYGEN", 2}}},
    {"ALCOHOL", {{"CARBON", 2}, {"HYDROGEN", 6}, {"OXYGEN", 1}}},
    {"GLUCOSE", {{"CARBON", 6}, {"HYDROGEN", 12}, {"OXYGEN", 6}}}
};

map<string, vector<string>> drink_recipes = {
    {"SOFT DRINK", {"WATER", "CARBON DIOXIDE", "GLUCOSE"}},
    {"VODKA", {"WATER", "ALCOHOL", "GLUCOSE"}},
    {"CHAMPAGNE", {"WATER", "CARBON DIOXIDE", "ALCOHOL"}}
};

void print_inventory() {
    cout << "CARBON: " << atom_inventory["CARBON"]
         << ", OXYGEN: " << atom_inventory["OXYGEN"]
         << ", HYDROGEN: " << atom_inventory["HYDROGEN"] << endl;
}

map<string, unsigned long long> molecule_inventory;

void add_molecules_to_inventory(const string& molecule_name, unsigned long long count) {
    molecule_inventory[molecule_name] += count;
}

void handle_tcp_command(const string& command) {
    istringstream iss(command);
    string action, atom;
    unsigned long long amount;

    iss >> action >> atom >> amount;
    transform(atom.begin(), atom.end(), atom.begin(), ::toupper);

    if (action != "ADD" || atom_inventory.find(atom) == atom_inventory.end() || iss.fail()) {
        cerr << "Invalid TCP command!" << endl;
        return;
    }

    atom_inventory[atom] += amount;
    print_inventory();
}

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

    unsigned long long count;
    try {
        count = stoull(tokens.back());
    } catch (...) {
        return "ERROR: Invalid molecule count";
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

    // ✅ הדפסת ההצלחה לשרת
    cout << "DELIVERED: " << count << " " << molecule_name << " molecules" << endl;
    add_molecules_to_inventory(molecule_name, count);
    print_inventory();
    return "OK: Delivered " + to_string(count) + " " + molecule_name + " molecules";
}

int compute_drink_count(const string& drink_name) {
    if (drink_recipes.find(drink_name) == drink_recipes.end()) return 0;

    int min_count = INT_MAX;
    for (const string& mol : drink_recipes[drink_name]) {
        if (molecule_inventory.find(mol) == molecule_inventory.end()) return 0;
        min_count = min(min_count, static_cast<int>(molecule_inventory[mol]));
    }
    return min_count;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <TCP_PORT> <UDP_PORT>" << endl;
        return 1;
    }

    int tcp_port = atoi(argv[1]);
    int udp_port = atoi(argv[2]);

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

    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(tcp_sock, &master);
    FD_SET(udp_sock, &master);
    FD_SET(STDIN_FILENO, &master);

    int fdmax = max({tcp_sock, udp_sock, STDIN_FILENO});
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
                int newfd = accept(tcp_sock, nullptr, nullptr);
                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;
                clients.push_back(newfd);
            } else if (i == udp_sock) {
                char buf[1024] = {0};
                sockaddr_in client_addr{};
                socklen_t len = sizeof(client_addr);
                int n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&client_addr, &len);
                if (n > 0) {
                    string response = handle_udp_command(string(buf));
                    if (response.rfind("ERROR", 0) == 0) cerr << response << endl;
                    sendto(udp_sock, response.c_str(), response.size(), 0, (sockaddr*)&client_addr, len);
                }
            } else if (i == STDIN_FILENO) {
                string line;
                getline(cin, line);
                transform(line.begin(), line.end(), line.begin(), ::toupper);
                istringstream iss(line);
                string command, drink;
                iss >> command;
                getline(iss, drink);
                drink.erase(0, drink.find_first_not_of(" ")); // להסיר רווחים מיותרים

                transform(command.begin(), command.end(), command.begin(), ::toupper);
                transform(drink.begin(), drink.end(), drink.begin(), ::toupper);

                if (command == "GEN" && drink_recipes.find(drink) != drink_recipes.end()) {
                    int count = compute_drink_count(drink);
                    cout << "Can prepare " << count << " " << drink << " drinks" << endl;
                } else {
                    cout << "Unknown drink command: " << line << endl;
                }

            } else {
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
