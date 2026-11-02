#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>
#include "../lib/json.hpp"

using json = nlohmann::json;

class Database {
private:
    sqlite3* db;
    std::mutex dbMutex;
    std::string currentDbPath;

    bool execute(const std::string& sql);
    void migrate();
    void seedInitialData();

public:
    Database();
    ~Database();

    bool init(const std::string& dbPath = "ev_smartcharge.db");
    void close();

    // --- User Operations ---
    int createUser(const std::string& fullName, const std::string& email, 
                   const std::string& mobile, const std::string& passwordHash, 
                   const std::string& role = "customer");
    json getUserByEmail(const std::string& email);
    json getUserById(int userId);
    json getAllUsers();
    bool updateUserProfile(int userId, const std::string& fullName, const std::string& mobile);
    bool updateUserPassword(int userId, const std::string& passwordHash);
    bool setUserRole(int userId, const std::string& role);

    // --- Session / Token Operations ---
    bool createSession(const std::string& token, int userId, int expirySeconds = 604800); // 7 days
    json getSession(const std::string& token);
    bool deleteSession(const std::string& token);
    void cleanExpiredSessions();

    // --- Vehicle Operations ---
    int createVehicle(int userId, const std::string& regNum, const std::string& model, 
                      const std::string& type, const std::string& connector, 
                      double batteryCap, const std::string& ownerName = "");
    json getVehiclesByUser(int userId);
    json getAllVehicles();
    json getVehicleById(int vehicleId);
    json getVehicleByReg(const std::string& regNum);
    bool updateVehicle(int vehicleId, int userId, const std::string& model, 
                       const std::string& type, const std::string& connector, 
                       double batteryCap);
    bool deleteVehicle(int vehicleId, int userId, bool isAdmin = false);

    // --- Station Operations ---
    bool syncStation(int id, const std::string& name, const std::string& loc, 
                     double speed, const std::string& conn, const std::string& status, 
                     bool isAvail, const std::string& curVeh);
    json getStationById(int stationId);
    json getAllStations();
    bool updateStationStatus(int stationId, const std::string& status, bool isAvail, const std::string& curVeh);

    // --- Booking Operations ---
    std::string generateBookingReference();
    bool hasBookingConflict(int stationId, const std::string& date, 
                            const std::string& startTime, const std::string& endTime, 
                            int excludeBookingId = 0);
    json createBooking(int userId, int vehicleId, int stationId, 
                       const std::string& date, const std::string& startTime, 
                       const std::string& endTime, int battery, double energy, 
                       double estDuration, const std::string& status, 
                       const std::string& notes = "");
    json getBookingById(int bookingId);
    json getBookingByRef(const std::string& ref);
    json getBookingsByUser(int userId);
    json getAllBookings(const std::string& statusFilter = "", const std::string& dateFilter = "");
    bool updateBookingStatus(int bookingId, const std::string& newStatus);
    bool cancelBooking(int bookingId, int userId, bool isAdmin = false);

    // --- Contact Messages ---
    bool saveContactMessage(const std::string& name, const std::string& email, const std::string& message);
    json getContactMessages();

    // --- Admin Dashboard Statistics ---
    json getAdminMetrics();
};

#endif // DATABASE_H

