#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

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
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(sock, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool sendFull(int sock, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sock, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

int main() {
    ensureDir("downloads");
    ensureDir("uploads");

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
        return 1;
    }

    // AUTH
    string msg;

    recvLine(sock, msg);
    cout << msg;
    string user;
    getline(cin, user);
    sendLine(sock, user);

    recvLine(sock, msg);
    cout << msg;
    string pass;
    getline(cin, pass);
    sendLine(sock, pass);

    recvLine(sock, msg);
    if (msg == "AUTH_FAIL") {
        cout << "Login failed.\n";
        return 0;
    }
    cout << "Login success!\n";

    // MENU
    while (true) {
        cout << "\n1) LIST\n2) DOWNLOAD\n3) UPLOAD\n4) REQUEST PUBLISH\n5) EXIT\n> ";

        string ch;
        getline(cin, ch);

        // LIST
        if (ch == "1") {
            sendLine(sock, "LIST");
            cout << "\nMain Folder Files:\n";

            while (true) {
                recvLine(sock, msg);
                if (msg == "END") break;
                cout << " - " << msg << endl;
            }
        }

        // DOWNLOAD
        else if (ch == "2") {
            cout << "File to download: ";
            string fname;
            getline(cin, fname);

            sendLine(sock, "GET " + fname);
            recvLine(sock, msg);

            if (msg.rfind("OK ", 0) != 0) {
                cout << "File not found.\n";
                continue;
            }

            uint64_t sz = stoull(msg.substr(3));
            cout << "Downloading " << sz << " bytes...\n";

            ofstream out("downloads/" + fname, ios::binary);
            char buf[4096];
            uint64_t rem = sz;

            while (rem > 0) {
                size_t chunk = rem < sizeof(buf) ? rem : sizeof(buf);
                if (!recvFull(sock, buf, chunk)) break;
                out.write(buf, chunk);
                rem -= chunk;
            }
            out.close();
            cout << "Saved to downloads/" << fname << endl;
        }

        // UPLOAD
        else if (ch == "3") {
            cout << "Local file in uploads/ to upload: ";
            string fname;
            getline(cin, fname);

            string path = "uploads/" + fname;
            ifstream f(path, ios::binary | ios::ate);
            if (!f.is_open()) {
                cout << "File not found in uploads.\n";
                continue;
            }
            uint64_t sz = f.tellg();
            f.seekg(0);

            sendLine(sock, "PUT " + fname);
            recvLine(sock, msg);

            if (msg != "READY") {
                cout << "Server error.\n";
                continue;
            }

            sendLine(sock, "SIZE " + to_string(sz));

            char buf[4096];
            while (!f.eof()) {
                f.read(buf, sizeof(buf));
                size_t n = f.gcount();
                if (n > 0) sendFull(sock, buf, n);
            }
            f.close();

            recvLine(sock, msg);
            cout << msg << endl;
        }

        // REQUEST PUBLISH
        else if (ch == "4") {
            cout << "File in uploads/ to publish: ";
            string fname;
            getline(cin, fname);

            sendLine(sock, "REQUEST " + fname);
            recvLine(sock, msg);

            if (msg == "APPROVED") cout << "Approved & published!\n";
            else if (msg == "DENIED") cout << "Admin denied.\n";
            else cout << "Server: " << msg << endl;
        }

        // EXIT
        else if (ch == "5") {
            sendLine(sock, "EXIT");
            break;
        }

        else {
            cout << "Invalid choice.\n";
        }
    }

    close(sock);
    return 0;
}

