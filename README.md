# 📁 Network File Sharing Server–Client (Multi-Client, C++)

A high-performance **client–server file sharing system** built in **C++** using sockets, featuring:

- ✔ Multi-client support (via `fork()`)
- ✔ User authentication system (admin + clients)
- ✔ File uploads & downloads
- ✔ Central shared directory
- ✔ Per-client private upload folders
- ✔ Admin-based publishing workflow
- ✔ Real-time server logging
- ✔ Clean directory structure

This project runs on **Linux (Ubuntu)** and demonstrates core system programming concepts:
Networking • OS • Socket Programming • Inter-process Communication • File I/O • Concurrency.


---

# 🚀 Features

## 🟦 1. Multi-Client Capability
- The server handles **multiple clients simultaneously**
- Achieved using `fork()` to create separate child processes
- Logs include:
  - Client IP address  
  - Child process ID  
  - Authentication success  
  - Clean disconnects  

---

## 🟩 2. Authentication System
The system includes:
- Admin authentication (required to start server)
- Client authentication for connecting


---

## 🟨 3. File Operations (Supported Commands)

| Command          | Description                                              |
|------------------|----------------------------------------------------------|
| `LIST`           | Shows files from shared/main/ directory                  |
| `GET <file>`     | Downloads file into client's downloads/ folder           |
| `PUT <file>`     | Uploads file from client's uploads/ folder               |
| `REQUEST <file>` | Requests admin approval to publish file                  |
| `EXIT`           | Disconnects client gracefully                            |

---

## 🟥 4. Admin Approval Workflow
Uploaded files go into:
shared/uploads/<client_name>/
Admin decides whether to move them into:
shared/main/
Clients can only publish a file after admin approval.

---

# 🟪 Folder Structure

### Server Side:
shared/
main/ # Public shared files
uploads/
<client1>/
<client2>/
<client3>/
server.cpp

### Client Side:
downloads/ # Downloaded files
uploads/ # Files to upload
client.cpp



---

# 🧩 System Architecture

+-----------------------------+
| SERVER |
+-----------------------------+
| Handle multiple clients |
| Fork child per connection |
| Authentication |
| LIST / GET / PUT commands |
| Accept publish requests |
| Log client activity |
+-----------------------------+
|
| TCP Socket
|
+-----------------------------+
| CLIENT |
+-----------------------------+
| Login |
| Send commands |
| Upload / Download |
| Local file storage |
+-----------------------------+

---

# 🛠️ Build & Run Instructions

## Compile Server
```bash
g++ server/server.cpp -o server/server
Run:
./server/server
Compile Client
bash
g++ client/client.cpp -o client/client
Run client:
./client/client
🧪 How to Test
Start server in one terminal
Start one or more clients in separate terminals
Perform operations:
LIST
PUT (upload)
GET (download)
REQUEST publishing
Admin approves/denies requests on the server side
Verify shared folder contents

🏗️ Technologies Used
C++
Linux TCP sockets
fork()
File I/O
Directory APIs (mkdir, opendir, rename)
Process management


