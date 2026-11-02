# ⚡ EV SmartCharge

**Power Your Journey. Charge Smarter.**

[![Deploy to Render](https://render.com/images/deploy-to-render-button.svg)](https://render.com/deploy?repo=https://github.com/suraj9430mrs-web/ev-smartcharge)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/suraj9430mrs-web/ev-smartcharge)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-green.svg)](https://isocpp.org/)
[![Database](https://img.shields.io/badge/Database-SQLite3%20WAL-orange.svg)](https://sqlite.org/)

A full-stack EV charging management platform with a C++ backend, SQLite database, and responsive web frontend. Features real-time station discovery, intelligent slot assignment, user authentication, booking management, and an admin operations console.

---

## 🌐 1-Click Cloud Deployment (Render)

Click the button below to automatically deploy the complete C++ platform to Render's free cloud:

[![Deploy to Render](https://render.com/images/deploy-to-render-button.svg)](https://render.com/deploy?repo=https://github.com/suraj9430mrs-web/ev-smartcharge)

Render will:
1. Detect `render.yaml` and parse the service specifications
2. Build the multi-stage Docker container (`Ubuntu 22.04` + `Clang` + `SQLite3`)
3. Compile the C++ backend binary and mount the web frontend
4. Expose a secure public HTTPS URL with automatic health checking on `/health`

---

## 🚀 Features

### Customer Features
- **Station Discovery** — Browse and filter charging stations by connector type, availability, and speed
- **User Registration & Login** — Secure authentication with PBKDF2-SHA256 password hashing
- **Vehicle Management** — Register, edit, and manage multiple electric vehicles
- **Slot Booking** — 6-step booking wizard with date/time selection, conflict detection, and connector compatibility
- **Booking History** — View, filter, and print booking receipts
- **Customer Dashboard** — Personal dashboard with metrics, quick actions, and booking overview

### Admin Features
- **Operations Console** — Comprehensive admin panel for managing stations, vehicles, bookings, and users
- **Booking Lifecycle** — Confirm, start charging, complete, or cancel bookings
- **User Management** — View all registered customer accounts
- **Data Structures Explorer** — Interactive visualization of the C++ DSA engine

### Technical Features
- **C++ REST Backend** — High-performance HTTP server using cpp-httplib
- **SQLite Database** — Persistent storage with WAL mode for concurrent access
- **Intelligent Slot Assignment** — Bubble sort + sequential search for optimal station matching
- **Booking Conflict Prevention** — SQL-based overlap detection prevents double-booking
- **Responsive Design** — Mobile-first UI inspired by premium EV charging platforms

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Web Browser                          │
│  index.html │ find-charger │ dashboard │ admin console  │
└──────────────────────┬──────────────────────────────────┘
                       │ HTTP / REST API
┌──────────────────────┴──────────────────────────────────┐
│              C++ Backend Server (port 8080)              │
│  ┌─────────┐  ┌──────────────┐  ┌───────────────────┐  │
│  │ Routes  │  │  Charging    │  │  Database Layer   │  │
│  │ (REST)  │──│  Engine (DSA)│──│  (SQLite + CRUD)  │  │
│  └─────────┘  └──────────────┘  └───────────────────┘  │
│                       │                    │            │
│  ┌─────────┐  ┌──────┴──────┐  ┌──────────┴─────────┐  │
│  │ Crypto  │  │ Structures  │  │      Tables        │  │
│  │ (PBKDF2)│  │ Array|Stack │  │ users|sessions     │  │
│  │         │  │ Queue|LList │  │ vehicles|stations   │  │
│  └─────────┘  └─────────────┘  │ bookings|contacts   │  │
│                                └────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

---

## 📂 Project Structure

```
ev-smartcharge/
├── backend/
│   ├── api/routes.cpp          # REST API endpoints (~750 lines)
│   ├── db/database.cpp         # SQLite CRUD operations (~700 lines)
│   ├── services/
│   │   ├── charging_engine.h   # C++ DSA engine header
│   │   └── charging_engine.cpp # Engine implementation
│   ├── structures/
│   │   ├── array_store.h       # Fixed-size array wrapper
│   │   ├── stack.h             # Action audit trail stack (LIFO)
│   │   ├── queue.h             # Waiting vehicle queue (FIFO)
│   │   └── linked_list.h       # Request log linked list
│   ├── utils/crypto.h          # SHA-256, HMAC, PBKDF2 (portable)
│   ├── lib/httplib.h           # cpp-httplib HTTP server
│   └── main.cpp                # Server entry point
├── frontend/
│   ├── index.html              # Homepage
│   ├── admin.html              # Admin console
│   ├── pages/
│   │   ├── find-charger.html   # Station discovery
│   │   ├── dashboard.html      # Customer dashboard
│   │   ├── bookings.html       # Booking history
│   │   ├── login.html          # Login page
│   │   ├── register.html       # Registration page
│   │   ├── solutions.html      # Business solutions
│   │   ├── about.html          # About page
│   │   └── contact.html        # Contact page
│   ├── css/style.css           # Main stylesheet
│   ├── css/admin.css           # Admin console styles
│   └── js/
│       ├── api.js              # API client & auth manager
│       ├── main.js             # Frontend logic & booking wizard
│       └── admin.js            # Admin console logic
├── docs/                       # Documentation
├── tests/                      # Test suite
├── original_backup/            # Original team source code
├── Dockerfile                  # Container build
├── CMakeLists.txt              # CMake build config
└── README.md                   # This file
```

---

## 🛠️ Quick Start

### Prerequisites
- **macOS/Linux:** Clang (C++17), SQLite3
- **Windows:** See [docs/WINDOWS_SETUP.md](docs/WINDOWS_SETUP.md)

### Build & Run

```bash
# Clone the repository
git clone https://github.com/<your-username>/ev-smartcharge.git
cd ev-smartcharge

# Build
cd backend
clang++ -std=c++17 -O2 -I lib \
    -o ev_smartcharge \
    main.cpp db/database.cpp services/charging_engine.cpp api/routes.cpp \
    -lsqlite3 -pthread

# Run
./ev_smartcharge 8080
```

Open **http://localhost:8080** in your browser.

### Default Admin Account
- **Email:** `admin@smartcharge.in`
- **Password:** `smartcharge2026`
- **Access:** Navigate to the admin portal from the footer link

---

## 🔌 API Reference

### Authentication
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/auth/register` | Create customer account |
| POST | `/api/auth/login` | Login (returns Bearer token) |
| POST | `/api/auth/logout` | Invalidate session |
| GET | `/api/auth/me` | Get current user profile |
| PUT | `/api/auth/profile` | Update profile |

### Vehicles
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/vehicles` | List user's vehicles |
| POST | `/api/vehicles` | Register a vehicle |
| PUT | `/api/vehicles/:id` | Update vehicle |
| DELETE | `/api/vehicles/:id` | Delete vehicle |

### Stations
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/stations` | List all stations |
| GET | `/api/stations/:id` | Get station details |

### Bookings
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/bookings` | Create a booking |
| GET | `/api/bookings` | List user's bookings |
| GET | `/api/bookings/:id` | Get booking details |
| POST | `/api/bookings/:id/cancel` | Cancel a booking |

### Admin (requires admin role)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/admin/dashboard` | Admin metrics |
| GET | `/api/admin/bookings` | All bookings |
| PUT | `/api/admin/bookings/:id/status` | Update booking status |
| GET | `/api/admin/users` | All registered users |

---

## 📊 Data Structures (C++ Engine)

| Structure | Implementation | Use Case |
|-----------|---------------|----------|
| **Array** | Fixed-size `Vehicle[100]`, `Station[50]` | Core vehicle and station storage |
| **Stack** | `ActionStack` (LIFO, array-based) | Audit trail of assignments and operations |
| **Queue** | `WaitingQueue` (FIFO, circular array) | Vehicles waiting for compatible stations |
| **Linked List** | `RequestLinkedList` (singly linked) | Dynamic request session log |
| **Bubble Sort** | Descending by charging speed | Station priority ordering |
| **Sequential Search** | Linear scan with connector matching | Find compatible available station |
| **Binary Search** | On sorted station IDs | Fast station lookup by ID |

---

## 🔒 Security

- **Password Hashing:** PBKDF2-HMAC-SHA256 with 60,000 iterations and random salt
- **Session Tokens:** 64-character cryptographically random hex strings
- **Authorization:** Backend-enforced role checking on all protected endpoints
- **Rate Limiting:** Login attempts rate-limited (10/minute per IP)
- **Booking Integrity:** SQL transactions with overlap detection prevent double-booking
- **Input Validation:** Server-side validation on all user inputs

---

## 🚀 Deployment

See [docs/DEPLOYMENT_GUIDE.md](docs/DEPLOYMENT_GUIDE.md) for detailed instructions on:
- Docker containerized deployment
- Render.com cloud deployment
- Environment variable configuration

---

## 📄 License

This project is developed for educational and demonstration purposes.

© 2026 EV SmartCharge Technologies Pvt. Ltd.
