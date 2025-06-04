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
#include <sys/un.h>
#define UNIX_STREAM_PATH "/tmp/drinks_bar_stream.sock"
#define UNIX_DGRAM_PATH "/tmp/drinks_bar_dgram.sock"
bool history_path = false;
const char* path = "default_data.txt";

typedef struct f{
    unsigned long long carbon;
    unsigned long long oxygen;
    unsigned long long hydrogen;

} file_storage;


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

map<string, unsigned long long> molecule_inventory;

void print_inventory() {
    cout << "CARBON: " << atom_inventory["CARBON"]
         << ", OXYGEN: " << atom_inventory["OXYGEN"]
         << ", HYDROGEN: " << atom_inventory["HYDROGEN"] << endl;
}

void add_molecules_to_inventory(const string& molecule_name, unsigned long long count) {
    molecule_inventory[molecule_name] += count;
}

void read_from_file(){
    file_storage fs;
    FILE* f = fopen(path, "rb");
    if (f != nullptr) {
        fread(&fs, sizeof(fs), 1, f);
        fclose(f);
    } else {
        perror("Error reading from file");
    }

    if(atom_inventory["CARBON"] != fs.carbon || atom_inventory["HYDROGEN"] != fs.hydrogen || atom_inventory["OXYGEN"] != fs.oxygen ){
        atom_inventory["CARBON"] = fs.carbon;
        atom_inventory["HYDROGEN"] = fs.hydrogen;
        atom_inventory["OXYGEN"] = fs.oxygen;
        cout << "Update from file: ";
        print_inventory();
    }
}

void update_file(file_storage fs){
    FILE* f = fopen(path, "wb");
    if (f != nullptr) {
        fwrite(&fs, sizeof(fs), 1, f);
        fclose(f);
    } else {
        perror("Error opening file for writing");
    }
}

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

    file_storage fs;
    fs.carbon = atom_inventory["CARBON"];
    fs.hydrogen = atom_inventory["HYDROGEN"];
    fs.oxygen = atom_inventory["OXYGEN"];

    update_file(fs);
}

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

int create_unix_stream_socket() {
    unlink(UNIX_STREAM_PATH); // לוודא שאין קובץ קודם
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

void handle_tcp_command(const string& command) {
    read_from_file();
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

string handle_udp_command(const string& command) {
    read_from_file();
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

    file_storage fs;
    fs.carbon = atom_inventory["CARBON"];
    fs.hydrogen = atom_inventory["HYDROGEN"];
    fs.oxygen = atom_inventory["OXYGEN"];
    update_file(fs);

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
    int tcp_port = -1, udp_port = -1;
    int timeout = -1;
    int opt;
    string stream_path, dgram_path;
    const char *oxygen;
    const char *carbon;
    const char *hydrogen;
    bool o = false, c = false, h = false;

    while ((opt = getopt(argc, argv, "T:U:o:c:h:t:s:d:f:")) != -1) {
        switch (opt) {
            case 'T': tcp_port = atoi(optarg); break;
            case 'U': udp_port = atoi(optarg); break;
            case 'o': oxygen = optarg; o = true; break;
            case 'c': carbon = optarg; c = true; break;
            case 'h': hydrogen = optarg; h = true; break;
            case 't': timeout = set_timeout(string(optarg)); break;
            case 's': stream_path = optarg; break;
            case 'd': dgram_path = optarg; break;
            case 'f': path = optarg; history_path = true; break;
            default:
                cerr << "Usage: ./drinks_bar -T <tcp_port> -U <udp_port> [-o num] [-c num] [-h num] [-t timeout]" << endl;
                return 1;
        }
    }
    
    if(history_path){
        read_from_file();
    }else{
        if(c){
            add_atoms("CARBON", carbon);
        }
        if(o){
            add_atoms("OXYGEN", oxygen);
        }
        if(h){
            add_atoms("HYDROGEN", hydrogen);
        }
        file_storage fs;
        fs.carbon = atom_inventory["CARBON"];
        fs.hydrogen = atom_inventory["HYDROGEN"];
        fs.oxygen = atom_inventory["OXYGEN"];
        update_file(fs);
    }

    if (tcp_port == -1 || udp_port == -1) {
        cerr << "ERROR: TCP and UDP ports are required (use -T and -U)" << endl;
        return 1;
    }
    
    fd_set master, read_fds;
    FD_ZERO(&master);
    int uds_stream_sock = -1, uds_dgram_sock = -1;
    int fdmax = STDIN_FILENO;

    if (!stream_path.empty()) {
        unlink(stream_path.c_str());
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
    
    print_inventory();

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
