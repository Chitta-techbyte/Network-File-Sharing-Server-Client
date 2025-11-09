#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

static const string ROOT_SHARED = "shared";
static const string MAIN_DIR    = "shared/main";
static const string UPLOAD_ROOT = "shared/uploads";

// ---------------------- Utility ---------------------- //
void ensureDir(const string &p) { mkdir(p.c_str(), 0777); }

void sendLine(int sock, const string &msg) {
    string out = msg + "\n";
    send(sock, out.c_str(), out.size(), 0);
}

bool recvLine(int sock, string &out) {
    out.clear();
    char c;
    while (true) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') break;
        out.push_back(c);
    }
    return true;
}

bool recvFull(int sock, char *buf, size_t len) {
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(sock, buf + recvd, len - recvd, 0);
        if (n <= 0) return false;
        recvd += n;
    }
    return true;
}

bool sendFull(int sock, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

bool validUser(const string &u, const string &p) {
    return (u=="cl1" && p=="cl1pass") ||
           (u=="cl2" && p=="cl2pass") ||
           (u=="cl3" && p=="cl3pass");
}

uint64_t getFileSize(const string &path) {
    ifstream f(path, ios::binary | ios::ate);
    if (!f.is_open()) return 0;
    return static_cast<uint64_t>(f.tellg());
}

// ---------------------- Commands ---------------------- //
void handleList(int client) {
    DIR *d = opendir(MAIN_DIR.c_str());
    if (!d) { sendLine(client, "ERR"); return; }
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != nullptr) {
            string name = ent->d_name;
            if (name=="." || name=="..") continue;
            sendLine(client, name);
        }
        closedir(d);
    }
    sendLine(client, "END");
}

void handleGet(int client, const string &filename) {
    string path = MAIN_DIR + "/" + filename;
    uint64_t sz = getFileSize(path);
    if (!sz) { sendLine(client, "ERR"); return; }

    sendLine(client, "OK " + to_string(sz));

    ifstream f(path, ios::binary);
    char buffer[4096];
    while (!f.eof()) {
        f.read(buffer, sizeof(buffer));
        streamsize bytes = f.gcount();
        if (bytes > 0) {
            if (!sendFull(client, buffer, static_cast<size_t>(bytes))) break;
        }
    }
}

void handlePut(int client, const string &user, const string &filename) {
    sendLine(client, "READY");

    string sizeLine;
    if (!recvLine(client, sizeLine) || sizeLine.rfind("SIZE ",0)!=0) {
        sendLine(client, "ERR protocol");
        return;
    }
    uint64_t sz = stoull(sizeLine.substr(5));

    string userDir = UPLOAD_ROOT + "/" + user;
    ensureDir(UPLOAD_ROOT);
    ensureDir(userDir);
    string outPath = userDir + "/" + filename;

    ofstream out(outPath, ios::binary);
    if (!out.is_open()) { sendLine(client, "ERR cannot_open"); return; }

    char buffer[4096];
    uint64_t remaining = sz;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (!recvFull(client, buffer, chunk)) { out.close(); return; }
        out.write(buffer, chunk);
        remaining -= chunk;
    }
    out.close();
    sendLine(client, "OK uploaded (pending admin approval)");
}

void handleRequestApproval(int client, const string &user, const string &filename) {
    string src = UPLOAD_ROOT + "/" + user + "/" + filename;
    string dst = MAIN_DIR + "/" + filename;

    ifstream f(src, ios::binary);
    if (!f.is_open()) { sendLine(client, "ERR no such file"); return; }
    f.close();

    cout << "\n====== Admin Approval Request ======\n";
    cout << "User: " << user << "\n";
    cout << "File: " << filename << "\n";
    cout << "Approve moving to MAIN folder? (y/n): ";

    char ans;
    cin >> ans;

    if (ans=='y' || ans=='Y') {
        if (rename(src.c_str(), dst.c_str()) == 0) {
            sendLine(client, "APPROVED");
            cout << "File approved and moved.\n";
        } else {
            sendLine(client, "ERR move_failed");
        }
    } else {
        sendLine(client, "DENIED");
        cout << "Admin denied.\n";
    }
}

// ---------------------- Child handler ---------------------- //
void handleClient(int client) {
    sendLine(client, "USERNAME:");
    string user; if (!recvLine(client, user)) { close(client); return; }

    sendLine(client, "PASSWORD:");
    string pass; if (!recvLine(client, pass)) { close(client); return; }

    if (!validUser(user, pass)) {
        sendLine(client, "AUTH_FAIL");
        close(client);
        return;
    }
    sendLine(client, "AUTH_OK");
    cout << "[AUTH SUCCESS] User logged in: " << user << endl;

    while (true) {
        string cmd;
        if (!recvLine(client, cmd)) {
            cout << "[Child PID " << getpid() << "] Client disconnected unexpectedly.\n";
            break;
        }

        if (cmd == "EXIT") {
            cout << "[Child PID " << getpid() << "] Client requested EXIT.\n";
            break;
        } else if (cmd == "LIST") {
            handleList(client);
        } else if (cmd.rfind("GET ", 0) == 0) {
            handleGet(client, cmd.substr(4));
        } else if (cmd.rfind("PUT ", 0) == 0) {
            handlePut(client, user, cmd.substr(4));
        } else if (cmd.rfind("REQUEST ", 0) == 0) {
            handleRequestApproval(client, user, cmd.substr(8));
        } else {
            sendLine(client, "ERR invalid");
        }
    }

    cout << "[Child PID " << getpid() << "] Closing connection.\n";
    close(client);
}

// ---------------------- Main server ---------------------- //
int main() {
    string adminPass;
    cout << "Admin password: ";
    cin >> adminPass;
    if (adminPass != "admin123") {
        cout << "Wrong admin password.\n";
        return 0;
    }

    ensureDir(ROOT_SHARED);
    ensureDir(MAIN_DIR);
    ensureDir(UPLOAD_ROOT);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, 10) < 0) { perror("listen"); return 1; }

    cout << "=== Multi-Client File Server Running on Port 8080 ===\n";

    while (true) {
        sockaddr_in cli{};
        socklen_t clen = sizeof(cli);
        int client = accept(srv, (sockaddr*)&cli, &clen);
        if (client < 0) continue;

        char ipbuf[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &cli.sin_addr, ipbuf, sizeof(ipbuf));
        cout << "\n[NEW CONNECTION] Client IP: " << ipbuf << endl;

        pid_t pid = fork();
        if (pid == 0) {
            // Child
            close(srv);
            cout << "[Child PID " << getpid() << "] Handling this client...\n";
            handleClient(client);
            cout << "[Child PID " << getpid() << "] Process exiting.\n";
            exit(0);
        } else {
            // Parent
            close(client);
        }
    }

    close(srv);
    return 0;
}

