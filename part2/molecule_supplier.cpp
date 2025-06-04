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

map<string, unsigned long long> atom_inventory = {
    {"CARBON", 0}, {"OXYGEN", 0}, {"HYDROGEN", 0}
};

// הגדרת צריכת אטומים לכל מולקולה
map<string, map<string, int>> molecule_recipes = {
    {"WATER", {{"HYDROGEN", 2}, {"OXYGEN", 1}}},
    {"CARBON DIOXIDE", {{"CARBON", 1}, {"OXYGEN", 2}}},
    {"ALCOHOL", {{"CARBON", 2}, {"HYDROGEN", 6}, {"OXYGEN", 1}}},
    {"GLUCOSE", {{"CARBON", 6}, {"HYDROGEN", 12}, {"OXYGEN", 6}}}
};

void print_inventory() {
    cout << "CARBON: " << atom_inventory["CARBON"]
         << ", OXYGEN: " << atom_inventory["OXYGEN"]
         << ", HYDROGEN: " << atom_inventory["HYDROGEN"] << endl;
}

void add_atoms(const string& atom_type, const string& amount_string) {
    // בדיקה שהקלט מכיל רק ספרות
    if (!all_of(amount_string.begin(), amount_string.end(), ::isdigit)) {
        cerr << "Invalid command: amount must be a positive number!" << endl;
        return;
    }

    try {
        unsigned long long amount = stoull(amount_string);  // משתמשים ב-ULL ולא UINT
        if (atom_inventory[atom_type] + amount > MAX_VALUE) {
            cerr << "Invalid command: not enough place for the atoms!" << endl;
            return;
        }

        atom_inventory[atom_type] += amount;
    } catch (const exception& e) {
        cerr << "Error converting number" << endl;
    }
}

void handle_tcp_command(const string& command) {
    istringstream iss(command);
    string action, atom_type, amount_string;
  
    iss >> action >> atom_type >> amount_string;
    
    // אם הפקודה לא חוקית
    if (action != "ADD" || iss.fail()) {
        cerr << "Invalid command!" << endl;
        return;
    }

    // ממירים את סוג האטום לאותיות גדולות (כדי שיהיה תואם ל-CARBON, OXYGEN וכו')
    transform(atom_type.begin(), atom_type.end(), atom_type.begin(), ::toupper);

    if (atom_inventory.find(atom_type) != atom_inventory.end()) {
        add_atoms(atom_type, amount_string);
        print_inventory();
    } else {
        cerr << "Unknown atom type!" << endl;
    }
}

string handle_udp_command(const string& command) {
    istringstream iss(command);
    string action;
    iss >> action;

    if (action != "DELIVER") {
        cerr << "Invalid UDP command!" << endl;
        return "ERROR: Invalid command";
    }

    // אסוף את כל המילים שנשארו
    vector<string> tokens;
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() < 2) {
        cerr << "Invalid UDP command format!" << endl;
        return "ERROR: Invalid command format";
    }

    // הניסיון לפענח את המספר האחרון
    string count_str = tokens.back();

// בדיקה שהמחרוזת מכילה רק ספרות
    if (!all_of(count_str.begin(), count_str.end(), ::isdigit)) {
        return "ERROR: Not a positive number";
    }

    unsigned long long count;
    try {
        count = stoull(count_str);
    } catch (const exception& e) {
        return "ERROR: Conversion failed";
    }


    // המילים שלפני המספר הן שם המולקולה
    string molecule_name;
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        if (!molecule_name.empty()) molecule_name += " ";
        molecule_name += tokens[i];
    }

    // תמיכה בפקודות כמו DELIVER OXYGEN WATER 2 → atom = OXYGEN, molecule = WATER
    string atom_name = "";
    string real_molecule = molecule_name;
    if (tokens.size() > 3) {
        atom_name = tokens[0];
        real_molecule = molecule_name.substr(atom_name.size() + 1); // הסרת האטום מהמולקולה
    }

    // בדיקה שהמולקולה קיימת
    if (molecule_recipes.find(real_molecule) == molecule_recipes.end()) {
        return "ERROR: Unknown molecule '" + real_molecule + "'";
    }

    const auto& recipe = molecule_recipes[real_molecule];

    // לחשב אילו אטומים דרושים
    map<string, unsigned long long> needed;
    for (const auto& [atom, per_mol] : recipe) {
        needed[atom] += per_mol * count;
    }

    // אם גם צוין atom בתחילת הפקודה – נוסיף אותו לרשימת ההפחתות
    if (!atom_name.empty()) {
        string upper_atom = atom_name;
        transform(upper_atom.begin(), upper_atom.end(), upper_atom.begin(), ::toupper);
        if (atom_inventory.find(upper_atom) == atom_inventory.end()) {
            return "ERROR: Invalid atom '" + upper_atom + "'";
        }
        needed[upper_atom] += count;
    }

    // לבדוק אילו אטומים חסרים
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


    // ניכוי בפועל
    for (const auto& [atom, need_count] : needed) {
        atom_inventory[atom] -= need_count;
    }

    print_inventory();
    return "OK: Delivered " + to_string(count) + " " + molecule_name + " molecules";
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <TCP_PORT> <UDP_PORT>" << endl;
        return 1;
    }

    int tcp_port = atoi(argv[1]);
    int udp_port = atoi(argv[2]);

    // יצירת סוקט TCP
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_addr {};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);
    bind(tcp_sock, (sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(tcp_sock, 5);

    // יצירת סוקט UDP
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
                int newfd = accept(tcp_sock, nullptr, nullptr);
                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;
                clients.push_back(newfd);
            } else if (i == udp_sock) {
                char buf[1024] = {0};
                sockaddr_in client_addr {};
                socklen_t len = sizeof(client_addr);
                int n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0,
                                 (sockaddr*)&client_addr, &len);
                if (n > 0) {
                    string response = handle_udp_command(string(buf));

                    // הדפסת השגיאה לשרת אם יש שגיאה
                    if (response.rfind("ERROR", 0) == 0) {
                        cerr << response << endl;
                    } else {
                        cout << response << endl;  // ✅ פלט מוצלח מוצג לשרת
                    }

                    sendto(udp_sock, response.c_str(), response.size(), 0,
                        (sockaddr*)&client_addr, len);
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
