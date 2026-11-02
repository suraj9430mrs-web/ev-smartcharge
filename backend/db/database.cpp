#include "database.h"
#include "../utils/crypto.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

Database::Database() : db(nullptr) {}

Database::~Database() {
    close();
}

void Database::close() {
    std::lock_guard<std::mutex> lock(dbMutex);
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB ERROR] " << (errMsg ? errMsg : "Unknown error") << " in SQL: " << sql << "\n";
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

static std::string getCurrentIsoTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool Database::init(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(dbMutex);
    currentDbPath = dbPath;

    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB ERROR] Cannot open database " << dbPath << ": " << sqlite3_errmsg(db) << "\n";
        return false;
    }

    // Enable foreign keys and WAL mode for maximum performance & concurrent reads
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");

    migrate();
    seedInitialData();

    std::cout << "[DB] SQLite persistent database initialized at: " << dbPath << "\n";
    return true;
}

void Database::migrate() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            full_name TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL COLLATE NOCASE,
            mobile TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL DEFAULT 'customer',
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            expires_at INTEGER NOT NULL,
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS vehicles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            registration_number TEXT UNIQUE NOT NULL COLLATE NOCASE,
            model TEXT NOT NULL,
            vehicle_type TEXT NOT NULL DEFAULT '4-Wheeler (Car)',
            connector_type TEXT NOT NULL,
            battery_capacity REAL NOT NULL,
            owner_name TEXT,
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS stations (
            id INTEGER PRIMARY KEY,
            station_name TEXT NOT NULL,
            location TEXT NOT NULL,
            charging_speed REAL NOT NULL,
            connector_type TEXT NOT NULL,
            operational_status TEXT NOT NULL DEFAULT 'Operational',
            is_available INTEGER NOT NULL DEFAULT 1,
            current_vehicle TEXT DEFAULT '',
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS bookings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            booking_reference TEXT UNIQUE NOT NULL,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
            station_id INTEGER NOT NULL REFERENCES stations(id) ON DELETE RESTRICT,
            booking_date TEXT NOT NULL,
            start_time TEXT NOT NULL,
            end_time TEXT NOT NULL,
            current_battery INTEGER NOT NULL DEFAULT 20,
            required_energy REAL NOT NULL,
            estimated_duration REAL NOT NULL,
            status TEXT NOT NULL DEFAULT 'Confirmed',
            notes TEXT DEFAULT '',
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS charging_sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            booking_id INTEGER NOT NULL REFERENCES bookings(id) ON DELETE CASCADE,
            started_at TEXT,
            completed_at TEXT,
            energy_delivered REAL DEFAULT 0.0,
            status TEXT NOT NULL DEFAULT 'Active'
        );

        CREATE TABLE IF NOT EXISTS contact_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT NOT NULL,
            message TEXT NOT NULL,
            created_at TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
        CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);
        CREATE INDEX IF NOT EXISTS idx_vehicles_user ON vehicles(user_id);
        CREATE INDEX IF NOT EXISTS idx_bookings_user ON bookings(user_id);
        CREATE INDEX IF NOT EXISTS idx_bookings_station_date ON bookings(station_id, booking_date);
    )";

    execute(schema);
}

void Database::seedInitialData() {
    // 1. Seed Administrator Account if not present
    sqlite3_stmt* checkUserStmt;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users WHERE email = 'admin@smartcharge.in' OR email = 'admin';", -1, &checkUserStmt, nullptr);
    int userCount = 0;
    if (sqlite3_step(checkUserStmt) == SQLITE_ROW) {
        userCount = sqlite3_column_int(checkUserStmt, 0);
    }
    sqlite3_finalize(checkUserStmt);

    if (userCount == 0) {
        std::string adminHash = Crypto::hashPassword("smartcharge2026");
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, "INSERT INTO users (full_name, email, mobile, password_hash, role, created_at) VALUES (?, ?, ?, ?, 'admin', ?);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, "System Administrator", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, "admin@smartcharge.in", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, "+91 98888 77777", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, adminHash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, getCurrentIsoTime().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        std::cout << "[DB] Seeded default administrator account: admin@smartcharge.in\n";
    }

    // 2. Seed Default Operational Stations (101 to 106)
    struct SeedStation {
        int id;
        const char* name;
        const char* loc;
        double speed;
        const char* conn;
    };

    SeedStation defaultStations[] = {
        {101, "Bay A1 - Ultra-Fast Hub", "Bengaluru Tech Corridor - Bay A1", 150.0, "CCS"},
        {102, "Bay A2 - Express DC", "Bengaluru Tech Corridor - Bay A2", 120.0, "CHAdeMO"},
        {103, "Bay B1 - Standard Fast", "Whitefield Metro Plaza - Bay B1", 50.0, "Type2"},
        {104, "Bay B2 - Highway Supercharger", "Electronic City Expressway - Bay B2", 100.0, "CCS"},
        {105, "Bay C1 - Rapid Hub", "Outer Ring Road Terminal - Bay C1", 75.0, "CHAdeMO"},
        {106, "Bay C2 - Urban AC Charger", "Indiranagar Central Hub - Bay C2", 60.0, "Type2"}
    };

    for (const auto& s : defaultStations) {
        sqlite3_stmt* sStmt;
        sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO stations (id, station_name, location, charging_speed, connector_type, operational_status, is_available, created_at) VALUES (?, ?, ?, ?, ?, 'Operational', 1, ?);", -1, &sStmt, nullptr);
        sqlite3_bind_int(sStmt, 1, s.id);
        sqlite3_bind_text(sStmt, 2, s.name, -1, SQLITE_STATIC);
        sqlite3_bind_text(sStmt, 3, s.loc, -1, SQLITE_STATIC);
        sqlite3_bind_double(sStmt, 4, s.speed);
        sqlite3_bind_text(sStmt, 5, s.conn, -1, SQLITE_STATIC);
        sqlite3_bind_text(sStmt, 6, getCurrentIsoTime().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(sStmt);
        sqlite3_finalize(sStmt);
    }
}

// ==================== USER OPERATIONS ====================

int Database::createUser(const std::string& fullName, const std::string& email, 
                         const std::string& mobile, const std::string& passwordHash, 
                         const std::string& role) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (full_name, email, mobile, password_hash, role, created_at) VALUES (?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;

    std::string now = getCurrentIsoTime();
    sqlite3_bind_text(stmt, 1, fullName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, mobile.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    int newId = -1;
    if (rc == SQLITE_DONE) {
        newId = (int)sqlite3_last_insert_rowid(db);
    }
    sqlite3_finalize(stmt);
    return newId;
}

json Database::getUserByEmail(const std::string& email) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, full_name, email, mobile, password_hash, role, created_at FROM users WHERE email = ? COLLATE NOCASE;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    json u = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        u = json::object();
        u["id"] = sqlite3_column_int(stmt, 0);
        u["fullName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        u["email"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u["mobile"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        u["passwordHash"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        u["role"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        u["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    }
    sqlite3_finalize(stmt);
    return u;
}

json Database::getUserById(int userId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, full_name, email, mobile, role, created_at FROM users WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_int(stmt, 1, userId);
    json u = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        u = json::object();
        u["id"] = sqlite3_column_int(stmt, 0);
        u["fullName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        u["email"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u["mobile"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        u["role"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        u["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    }
    sqlite3_finalize(stmt);
    return u;
}

json Database::getAllUsers() {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, full_name, email, mobile, role, created_at FROM users ORDER BY id DESC;";
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json u = json::object();
        u["id"] = sqlite3_column_int(stmt, 0);
        u["fullName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        u["email"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u["mobile"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        u["role"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        u["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        arr.push_back(u);
    }
    sqlite3_finalize(stmt);
    return arr;
}

bool Database::updateUserProfile(int userId, const std::string& fullName, const std::string& mobile) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET full_name = ?, mobile = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, fullName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, mobile.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, userId);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateUserPassword(int userId, const std::string& passwordHash) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET password_hash = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, userId);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::setUserRole(int userId, const std::string& role) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET role = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, userId);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ==================== SESSION OPERATIONS ====================

bool Database::createSession(const std::string& token, int userId, int expirySeconds) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO sessions (token, user_id, expires_at, created_at) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    auto nowSec = std::time(nullptr);
    auto expSec = nowSec + expirySeconds;
    std::string nowStr = getCurrentIsoTime();

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, userId);
    sqlite3_bind_int64(stmt, 3, expSec);
    sqlite3_bind_text(stmt, 4, nowStr.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

json Database::getSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = R"(
        SELECT s.token, s.user_id, s.expires_at, u.full_name, u.email, u.role 
        FROM sessions s 
        JOIN users u ON s.user_id = u.id 
        WHERE s.token = ?;
    )";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    json sess = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t exp = sqlite3_column_int64(stmt, 2);
        if (exp > std::time(nullptr)) {
            sess = json::object();
            sess["token"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            sess["userId"] = sqlite3_column_int(stmt, 1);
            sess["expiresAt"] = exp;
            sess["fullName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            sess["email"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            sess["role"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        }
    }
    sqlite3_finalize(stmt);
    return sess;
}

bool Database::deleteSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM sessions WHERE token = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

void Database::cleanExpiredSessions() {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM sessions WHERE expires_at < ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, std::time(nullptr));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// ==================== VEHICLE OPERATIONS ====================

int Database::createVehicle(int userId, const std::string& regNum, const std::string& model, 
                           const std::string& type, const std::string& connector, 
                           double batteryCap, const std::string& ownerName) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO vehicles (user_id, registration_number, model, vehicle_type, connector_type, battery_capacity, owner_name, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;

    std::string now = getCurrentIsoTime();
    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_text(stmt, 2, regNum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, connector.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, batteryCap);
    sqlite3_bind_text(stmt, 7, ownerName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    int newId = -1;
    if (rc == SQLITE_DONE) {
        newId = (int)sqlite3_last_insert_rowid(db);
    }
    sqlite3_finalize(stmt);
    return newId;
}

json Database::getVehiclesByUser(int userId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, user_id, registration_number, model, vehicle_type, connector_type, battery_capacity, owner_name, created_at FROM vehicles WHERE user_id = ? ORDER BY id DESC;";
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

    sqlite3_bind_int(stmt, 1, userId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json v = json::object();
        v["id"] = sqlite3_column_int(stmt, 0);
        v["userId"] = sqlite3_column_int(stmt, 1);
        v["registrationNumber"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        v["model"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        v["vehicleType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        v["connectorType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        v["batteryCapacity"] = sqlite3_column_double(stmt, 6);
        const char* owner = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        v["ownerName"] = owner ? owner : "";
        v["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        arr.push_back(v);
    }
    sqlite3_finalize(stmt);
    return arr;
}

json Database::getAllVehicles() {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = R"(
        SELECT v.id, v.user_id, v.registration_number, v.model, v.vehicle_type, 
               v.connector_type, v.battery_capacity, v.owner_name, v.created_at, u.full_name, u.email
        FROM vehicles v
        JOIN users u ON v.user_id = u.id
        ORDER BY v.id DESC;
    )";
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json v = json::object();
        v["id"] = sqlite3_column_int(stmt, 0);
        v["userId"] = sqlite3_column_int(stmt, 1);
        v["registrationNumber"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        v["model"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        v["vehicleType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        v["connectorType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        v["batteryCapacity"] = sqlite3_column_double(stmt, 6);
        const char* owner = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        v["ownerName"] = owner ? owner : "";
        v["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        v["userFullName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        v["userEmail"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        arr.push_back(v);
    }
    sqlite3_finalize(stmt);
    return arr;
}

json Database::getVehicleById(int vehicleId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, user_id, registration_number, model, vehicle_type, connector_type, battery_capacity, owner_name, created_at FROM vehicles WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_int(stmt, 1, vehicleId);
    json v = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        v = json::object();
        v["id"] = sqlite3_column_int(stmt, 0);
        v["userId"] = sqlite3_column_int(stmt, 1);
        v["registrationNumber"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        v["model"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        v["vehicleType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        v["connectorType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        v["batteryCapacity"] = sqlite3_column_double(stmt, 6);
        const char* owner = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        v["ownerName"] = owner ? owner : "";
        v["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    }
    sqlite3_finalize(stmt);
    return v;
}

json Database::getVehicleByReg(const std::string& regNum) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, user_id, registration_number, model, vehicle_type, connector_type, battery_capacity, owner_name, created_at FROM vehicles WHERE registration_number = ? COLLATE NOCASE;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_text(stmt, 1, regNum.c_str(), -1, SQLITE_TRANSIENT);
    json v = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        v = json::object();
        v["id"] = sqlite3_column_int(stmt, 0);
        v["userId"] = sqlite3_column_int(stmt, 1);
        v["registrationNumber"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        v["model"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        v["vehicleType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        v["connectorType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        v["batteryCapacity"] = sqlite3_column_double(stmt, 6);
        const char* owner = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        v["ownerName"] = owner ? owner : "";
        v["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    }
    sqlite3_finalize(stmt);
    return v;
}

bool Database::updateVehicle(int vehicleId, int userId, const std::string& model, 
                             const std::string& type, const std::string& connector, 
                             double batteryCap) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE vehicles SET model = ?, vehicle_type = ?, connector_type = ?, battery_capacity = ? WHERE id = ? AND user_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, connector.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, batteryCap);
    sqlite3_bind_int(stmt, 5, vehicleId);
    sqlite3_bind_int(stmt, 6, userId);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::deleteVehicle(int vehicleId, int userId, bool isAdmin) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = isAdmin ? "DELETE FROM vehicles WHERE id = ?;" : "DELETE FROM vehicles WHERE id = ? AND user_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, vehicleId);
    if (!isAdmin) {
        sqlite3_bind_int(stmt, 2, userId);
    }

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ==================== STATION OPERATIONS ====================

bool Database::syncStation(int id, const std::string& name, const std::string& loc, 
                           double speed, const std::string& conn, const std::string& status, 
                           bool isAvail, const std::string& curVeh) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO stations (id, station_name, location, charging_speed, connector_type, operational_status, is_available, current_vehicle, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            station_name=excluded.station_name,
            location=excluded.location,
            charging_speed=excluded.charging_speed,
            connector_type=excluded.connector_type,
            operational_status=excluded.operational_status,
            is_available=excluded.is_available,
            current_vehicle=excluded.current_vehicle;
    )";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string now = getCurrentIsoTime();
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, loc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, speed);
    sqlite3_bind_text(stmt, 5, conn.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, isAvail ? 1 : 0);
    sqlite3_bind_text(stmt, 8, curVeh.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

json Database::getStationById(int stationId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, station_name, location, charging_speed, connector_type, operational_status, is_available, current_vehicle, created_at FROM stations WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_int(stmt, 1, stationId);
    json s = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        s = json::object();
        s["id"] = sqlite3_column_int(stmt, 0);
        s["stationName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        s["location"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        s["chargingSpeed"] = sqlite3_column_double(stmt, 3);
        s["connectorType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        s["operationalStatus"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        s["isAvailable"] = (sqlite3_column_int(stmt, 6) == 1);
        const char* cur = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        s["currentVehicle"] = cur ? cur : "";
        s["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    }
    sqlite3_finalize(stmt);
    return s;
}

json Database::getAllStations() {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, station_name, location, charging_speed, connector_type, operational_status, is_available, current_vehicle, created_at FROM stations ORDER BY id ASC;";
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json s = json::object();
        s["id"] = sqlite3_column_int(stmt, 0);
        s["stationID"] = sqlite3_column_int(stmt, 0); // compat
        s["stationName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        s["location"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        s["chargingSpeed"] = sqlite3_column_double(stmt, 3);
        s["connectorType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        s["operationalStatus"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        s["maintenanceStatus"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)); // compat
        s["isAvailable"] = (sqlite3_column_int(stmt, 6) == 1);
        s["available"] = (sqlite3_column_int(stmt, 6) == 1); // compat
        const char* cur = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        s["currentVehicle"] = cur ? cur : "";
        s["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        arr.push_back(s);
    }
    sqlite3_finalize(stmt);
    return arr;
}

bool Database::updateStationStatus(int stationId, const std::string& status, bool isAvail, const std::string& curVeh) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE stations SET operational_status = ?, is_available = ?, current_vehicle = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, isAvail ? 1 : 0);
    sqlite3_bind_text(stmt, 3, curVeh.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, stationId);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ==================== BOOKING OPERATIONS ====================

std::string Database::generateBookingReference() {
    // Generate EVSC-YYYY-XXXXXX reference
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    sqlite3_stmt* stmt;
    int maxId = 0;
    if (sqlite3_prepare_v2(db, "SELECT MAX(id) FROM bookings;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            maxId = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    std::ostringstream ss;
    ss << "EVSC-" << (tm.tm_year + 1900) << "-" 
       << std::setw(6) << std::setfill('0') << (maxId + 1);
    return ss.str();
}

// Checks if another confirmed or charging booking on this station overlaps with [startTime, endTime] on bookingDate
bool Database::hasBookingConflict(int stationId, const std::string& date, 
                                  const std::string& startTime, const std::string& endTime, 
                                  int excludeBookingId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    // Overlap condition: (existing.start_time < new.end_time) AND (new.start_time < existing.end_time)
    const char* sql = R"(
        SELECT COUNT(*) FROM bookings 
        WHERE station_id = ? 
          AND booking_date = ? 
          AND status IN ('Confirmed', 'Charging', 'Pending') 
          AND id != ? 
          AND (start_time < ? AND ? < end_time);
    )";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, stationId);
    sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, excludeBookingId);
    sqlite3_bind_text(stmt, 4, endTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, startTime.c_str(), -1, SQLITE_TRANSIENT);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

json Database::createBooking(int userId, int vehicleId, int stationId, 
                             const std::string& date, const std::string& startTime, 
                             const std::string& endTime, int battery, double energy, 
                             double estDuration, const std::string& status, 
                             const std::string& notes) {
    std::lock_guard<std::mutex> lock(dbMutex);

    // Double check conflict inside mutex
    sqlite3_stmt* conflictStmt;
    const char* conflictSql = R"(
        SELECT COUNT(*) FROM bookings 
        WHERE station_id = ? 
          AND booking_date = ? 
          AND status IN ('Confirmed', 'Charging', 'Pending') 
          AND (start_time < ? AND ? < end_time);
    )";
    if (sqlite3_prepare_v2(db, conflictSql, -1, &conflictStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(conflictStmt, 1, stationId);
        sqlite3_bind_text(conflictStmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(conflictStmt, 3, endTime.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(conflictStmt, 4, startTime.c_str(), -1, SQLITE_TRANSIENT);
        int c = 0;
        if (sqlite3_step(conflictStmt) == SQLITE_ROW) {
            c = sqlite3_column_int(conflictStmt, 0);
        }
        sqlite3_finalize(conflictStmt);

        if (c > 0 && status != "Waiting") {
            json err;
            err["success"] = false;
            err["message"] = "Slot conflict: This station is already reserved for the selected date and time window.";
            return err;
        }
    }

    std::string ref = generateBookingReference();
    std::string now = getCurrentIsoTime();

    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO bookings (booking_reference, user_id, vehicle_id, station_id, 
                             booking_date, start_time, end_time, current_battery, 
                             required_energy, estimated_duration, status, notes, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        json err;
        err["success"] = false;
        err["message"] = "Database statement preparation failed.";
        return err;
    }

    sqlite3_bind_text(stmt, 1, ref.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, userId);
    sqlite3_bind_int(stmt, 3, vehicleId);
    sqlite3_bind_int(stmt, 4, stationId);
    sqlite3_bind_text(stmt, 5, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, startTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, endTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, battery);
    sqlite3_bind_double(stmt, 9, energy);
    sqlite3_bind_double(stmt, 10, estDuration);
    sqlite3_bind_text(stmt, 11, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, now.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        json err;
        err["success"] = false;
        err["message"] = "Failed to record booking in persistent storage.";
        return err;
    }
    int newId = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    json res;
    res["success"] = true;
    res["bookingId"] = newId;
    res["bookingReference"] = ref;
    res["status"] = status;
    res["estimatedDuration"] = estDuration;
    return res;
}

json Database::getBookingById(int bookingId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = R"(
        SELECT b.id, b.booking_reference, b.user_id, b.vehicle_id, b.station_id,
               b.booking_date, b.start_time, b.end_time, b.current_battery, 
               b.required_energy, b.estimated_duration, b.status, b.notes, b.created_at,
               u.full_name, u.email, u.mobile,
               v.registration_number, v.model, v.connector_type, v.battery_capacity,
               s.station_name, s.location, s.charging_speed, s.connector_type as station_connector
        FROM bookings b
        JOIN users u ON b.user_id = u.id
        JOIN vehicles v ON b.vehicle_id = v.id
        JOIN stations s ON b.station_id = s.id
        WHERE b.id = ?;
    )";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_int(stmt, 1, bookingId);
    json b = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        b = json::object();
        b["id"] = sqlite3_column_int(stmt, 0);
        b["bookingReference"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        b["userId"] = sqlite3_column_int(stmt, 2);
        b["vehicleId"] = sqlite3_column_int(stmt, 3);
        b["stationId"] = sqlite3_column_int(stmt, 4);
        b["bookingDate"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        b["startTime"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        b["endTime"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        b["currentBattery"] = sqlite3_column_int(stmt, 8);
        b["requiredEnergy"] = sqlite3_column_double(stmt, 9);
        b["estimatedDuration"] = sqlite3_column_double(stmt, 10);
        b["status"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        const char* notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        b["notes"] = notes ? notes : "";
        b["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));

        b["customerName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        b["customerEmail"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        b["customerMobile"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));

        b["vehicleRegistration"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 17));
        b["vehicleModel"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 18));
        b["vehicleConnector"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 19));
        b["vehicleBatteryCapacity"] = sqlite3_column_double(stmt, 20);

        b["stationName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 21));
        b["stationLocation"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 22));
        b["chargingSpeed"] = sqlite3_column_double(stmt, 23);
        b["stationConnector"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 24));
    }
    sqlite3_finalize(stmt);
    return b;
}

json Database::getBookingByRef(const std::string& ref) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM bookings WHERE booking_reference = ? COLLATE NOCASE;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;

    sqlite3_bind_text(stmt, 1, ref.c_str(), -1, SQLITE_TRANSIENT);
    int bId = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        bId = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (bId > 0) {
        // Unlock before calling getBookingById to avoid deadlock
        return getBookingById(bId);
    }
    return nullptr;
}

json Database::getBookingsByUser(int userId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = R"(
        SELECT b.id, b.booking_reference, b.vehicle_id, b.station_id,
               b.booking_date, b.start_time, b.end_time, b.current_battery, 
               b.required_energy, b.estimated_duration, b.status, b.created_at,
               v.registration_number, v.model, v.connector_type,
               s.station_name, s.location, s.charging_speed, s.connector_type as station_connector
        FROM bookings b
        JOIN vehicles v ON b.vehicle_id = v.id
        JOIN stations s ON b.station_id = s.id
        WHERE b.user_id = ?
        ORDER BY b.id DESC;
    )";
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

    sqlite3_bind_int(stmt, 1, userId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json b = json::object();
        b["id"] = sqlite3_column_int(stmt, 0);
        b["bookingReference"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        b["vehicleId"] = sqlite3_column_int(stmt, 2);
        b["stationId"] = sqlite3_column_int(stmt, 3);
        b["bookingDate"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        b["startTime"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        b["endTime"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        b["currentBattery"] = sqlite3_column_int(stmt, 7);
        b["requiredEnergy"] = sqlite3_column_double(stmt, 8);
        b["estimatedDuration"] = sqlite3_column_double(stmt, 9);
        b["status"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        b["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));

        b["vehicleRegistration"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        b["vehicleModel"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        b["vehicleConnector"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));

        b["stationName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        b["stationLocation"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));
        b["chargingSpeed"] = sqlite3_column_double(stmt, 17);
        b["stationConnector"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 18));
        arr.push_back(b);
    }
    sqlite3_finalize(stmt);
    return arr;
}

json Database::getAllBookings(const std::string& statusFilter, const std::string& dateFilter) {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::string sql = R"(
        SELECT b.id, b.booking_reference, b.user_id, b.vehicle_id, b.station_id,
               b.booking_date, b.start_time, b.end_time, b.current_battery, 
               b.required_energy, b.estimated_duration, b.status, b.created_at,
               u.full_name, u.email,
               v.registration_number, v.model,
               s.station_name, s.charging_speed, s.connector_type
        FROM bookings b
        JOIN users u ON b.user_id = u.id
        JOIN vehicles v ON b.vehicle_id = v.id
        JOIN stations s ON b.station_id = s.id
        WHERE 1=1
    )";

    if (!statusFilter.empty() && statusFilter != "ALL") {
        sql += " AND b.status = '" + statusFilter + "'";
    }
    if (!dateFilter.empty()) {
        sql += " AND b.booking_date = '" + dateFilter + "'";
    }
    sql += " ORDER BY b.id DESC;";

    sqlite3_stmt* stmt;
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return arr;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json b = json::object();
        b["id"] = sqlite3_column_int(stmt, 0);
        b["bookingReference"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        b["userId"] = sqlite3_column_int(stmt, 2);
        b["vehicleId"] = sqlite3_column_int(stmt, 3);
        b["stationId"] = sqlite3_column_int(stmt, 4);
        b["bookingDate"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        b["startTime"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        b["endTime"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        b["currentBattery"] = sqlite3_column_int(stmt, 8);
        b["requiredEnergy"] = sqlite3_column_double(stmt, 9);
        b["estimatedDuration"] = sqlite3_column_double(stmt, 10);
        b["status"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        b["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));

        b["customerName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        b["customerEmail"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));

        b["vehicleRegistration"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        b["vehicleModel"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));

        b["stationName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 17));
        b["chargingSpeed"] = sqlite3_column_double(stmt, 18);
        b["connectorType"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 19));
        arr.push_back(b);
    }
    sqlite3_finalize(stmt);
    return arr;
}

bool Database::updateBookingStatus(int bookingId, const std::string& newStatus) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE bookings SET status = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, newStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, bookingId);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::cancelBooking(int bookingId, int userId, bool isAdmin) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = isAdmin 
        ? "UPDATE bookings SET status = 'Cancelled' WHERE id = ? AND status IN ('Confirmed', 'Pending', 'Waiting');"
        : "UPDATE bookings SET status = 'Cancelled' WHERE id = ? AND user_id = ? AND status IN ('Confirmed', 'Pending', 'Waiting');";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, bookingId);
    if (!isAdmin) {
        sqlite3_bind_int(stmt, 2, userId);
    }

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ==================== CONTACT MESSAGES ====================

bool Database::saveContactMessage(const std::string& name, const std::string& email, const std::string& message) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO contact_messages (name, email, message, created_at) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string now = getCurrentIsoTime();
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

json Database::getContactMessages() {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, name, email, message, created_at FROM contact_messages ORDER BY id DESC;";
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json m = json::object();
        m["id"] = sqlite3_column_int(stmt, 0);
        m["name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m["email"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        m["message"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        m["createdAt"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        arr.push_back(m);
    }
    sqlite3_finalize(stmt);
    return arr;
}

// ==================== ADMIN METRICS ====================

json Database::getAdminMetrics() {
    std::lock_guard<std::mutex> lock(dbMutex);
    json m;

    auto runCount = [&](const std::string& q) -> int {
        sqlite3_stmt* s;
        int count = 0;
        if (sqlite3_prepare_v2(db, q.c_str(), -1, &s, nullptr) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) {
                count = sqlite3_column_int(s, 0);
            }
            sqlite3_finalize(s);
        }
        return count;
    };

    m["totalUsers"] = runCount("SELECT COUNT(*) FROM users;");
    m["totalVehicles"] = runCount("SELECT COUNT(*) FROM vehicles;");
    m["totalStations"] = runCount("SELECT COUNT(*) FROM stations;");
    m["availableStations"] = runCount("SELECT COUNT(*) FROM stations WHERE is_available = 1 AND operational_status = 'Operational';");
    m["occupiedStations"] = runCount("SELECT COUNT(*) FROM stations WHERE is_available = 0;");
    m["totalBookings"] = runCount("SELECT COUNT(*) FROM bookings;");
    m["confirmedBookings"] = runCount("SELECT COUNT(*) FROM bookings WHERE status = 'Confirmed';");
    m["chargingSessions"] = runCount("SELECT COUNT(*) FROM bookings WHERE status = 'Charging';");
    m["completedBookings"] = runCount("SELECT COUNT(*) FROM bookings WHERE status = 'Completed';");
    m["waitingBookings"] = runCount("SELECT COUNT(*) FROM bookings WHERE status = 'Waiting';");
    m["cancelledBookings"] = runCount("SELECT COUNT(*) FROM bookings WHERE status = 'Cancelled';");

    int tot = m["totalStations"].get<int>();
    int occ = m["occupiedStations"].get<int>();
    m["utilizationRate"] = (tot > 0) ? ((double)occ / tot * 100.0) : 0.0;

    return m;
}

