# EV SmartCharge — Windows Setup Guide

## Prerequisites

1. **MSYS2** — Download from https://www.msys2.org/
2. **SQLite3** — Installed via MSYS2

## Step-by-Step Setup

### 1. Install MSYS2

Download and install MSYS2 from https://www.msys2.org/

### 2. Install Build Tools

Open **MSYS2 UCRT64** terminal and run:

```bash
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-sqlite3
```

### 3. Clone the Repository

```bash
git clone https://github.com/<your-username>/ev-smartcharge.git
cd ev-smartcharge
```

### 4. Build the Server

```bash
cd backend
clang++ -std=c++17 -O2 -I lib \
    -o ev_smartcharge.exe \
    main.cpp db/database.cpp services/charging_engine.cpp api/routes.cpp \
    -lsqlite3 -lpthread -lws2_32
```

> **Note:** On Windows, you need `-lws2_32` for networking (Winsock).

### 5. Run the Server

```bash
./ev_smartcharge.exe 8080
```

Open `http://localhost:8080` in your browser.

## Alternative: Visual Studio

1. Create a new C++ Console project
2. Add all `.cpp` and `.h` files from `backend/`
3. Set C++ Language Standard to **C++17**
4. Add SQLite3 include and library paths
5. Link against `sqlite3.lib` and `ws2_32.lib`
6. Build and run

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `sqlite3.h not found` | Install `mingw-w64-ucrt-x86_64-sqlite3` via pacman |
| `ws2_32` linking error | Add `-lws2_32` to the compile command |
| Port already in use | Change the port: `./ev_smartcharge.exe 3000` |
| Database locked | Ensure only one instance is running |

