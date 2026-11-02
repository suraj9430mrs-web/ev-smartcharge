# EV SmartCharge — Deployment Guide

## Table of Contents
1. [Local Development](#local-development)
2. [Docker Deployment](#docker-deployment)
3. [Render Deployment](#render-deployment)
4. [Environment Variables](#environment-variables)
5. [Database Management](#database-management)

---

## Local Development

### Prerequisites
- **macOS/Linux:** Clang or GCC with C++17 support, SQLite3 development libraries
- **Windows:** See [WINDOWS_SETUP.md](WINDOWS_SETUP.md)

### Build & Run

```bash
cd backend

# Build the server
clang++ -std=c++17 -O2 -I lib \
    -o ev_smartcharge \
    main.cpp db/database.cpp services/charging_engine.cpp api/routes.cpp \
    -lsqlite3 -pthread

# Run the server
./ev_smartcharge 8080
```

Open `http://localhost:8080` in your browser.

### First Run
On first startup, the server:
1. Creates `ev_smartcharge.db` SQLite database
2. Initializes all tables (users, sessions, vehicles, stations, bookings, contact_messages)
3. Seeds the default admin account
4. Seeds 6 default charging stations

---

## Docker Deployment

### Build the Docker Image
```bash
docker build -t ev-smartcharge .
```

### Run the Container
```bash
docker run -d \
    --name ev-smartcharge \
    -p 8080:8080 \
    -v ev_data:/app/data \
    ev-smartcharge
```

The `-v ev_data:/app/data` flag creates a persistent volume for the SQLite database so data survives container restarts.

---

## Render Deployment

### Option 1: Docker on Render

1. Push your repository to GitHub
2. Go to [render.com](https://render.com) → New → Web Service
3. Connect your GitHub repository
4. Select **Docker** as the environment
5. Set environment variables:
   - `PORT` = `10000` (Render assigns this automatically)
   - `DATABASE_PATH` = `/app/data/ev_smartcharge.db`
   - `ADMIN_PASSWORD` = `<your-strong-password>`
6. Add a **Disk** under the service settings:
   - Name: `ev-data`
   - Mount Path: `/app/data`
   - Size: 1 GB (free tier)
7. Deploy

### Option 2: Native Build on Render

1. Push to GitHub
2. Create a new **Web Service** on Render
3. Set **Build Command:**
   ```bash
   apt-get update && apt-get install -y libsqlite3-dev && cd backend && clang++ -std=c++17 -O2 -I lib -o ev_smartcharge main.cpp db/database.cpp services/charging_engine.cpp api/routes.cpp -lsqlite3 -pthread
   ```
4. Set **Start Command:**
   ```bash
   cd backend && ./ev_smartcharge
   ```
5. Add a persistent disk for the database

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `8080` | Server listening port |
| `DATABASE_PATH` | `ev_smartcharge.db` | Path to SQLite database file |
| `ADMIN_EMAIL` | `admin@smartcharge.in` | Default admin email (first run only) |
| `ADMIN_PASSWORD` | `smartcharge2026` | Default admin password (first run only) |

---

## Database Management

### Backup
```bash
sqlite3 ev_smartcharge.db ".backup backup_$(date +%Y%m%d).db"
```

### Reset
```bash
rm ev_smartcharge.db
# Restart the server — a fresh database will be created
```

### Inspect
```bash
sqlite3 ev_smartcharge.db
.tables
SELECT * FROM users;
SELECT * FROM bookings;
.quit
```

