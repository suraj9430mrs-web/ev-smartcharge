#ifndef CHARGING_ENGINE_H
#define CHARGING_ENGINE_H

#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "../lib/json.hpp"
#include "../models/vehicle.h"
#include "../models/station.h"
#include "../structures/array_store.h"
#include "../structures/queue.h"
#include "../structures/stack.h"
#include "../structures/linked_list.h"

using json = nlohmann::json;

class Database;

class ChargingEngine {
private:
    Database* db;

    // Arrays for vehicles and stations (preserving original approach)
    Vehicle vehicles[MAX];
    Station stations[MAX];
    int vehicleCount;
    int stationCount;

    // Custom Queue for waiting vehicles
    Queue<Vehicle> waitingQueue;

    // Custom Stack for action history
    ActionStack actionHistory;

    // Custom LinkedList for request log
    LinkedList<RequestRecord> requestLog;

    // Statistics
    int totalAssignments;
    int totalCompletions;
    int requestCounter;

    // Get current timestamp string
    std::string getCurrentTimestamp();

    // Generate request ID
    std::string generateRequestID();

public:
    ChargingEngine();

    // ========== PRESERVED FUNCTIONS (refactored from original) ==========

    // Bubble sort stations by charging speed descending
    void sortStations();

    // Sequential search for available compatible station
    int searchStation(const Vehicle& v);

    // Assign vehicle to a station
    bool assignSlot(const Vehicle& v);

    // Process waiting queue - try to assign queued vehicles
    void processQueue();

    // Add vehicle with validation and duplicate checking
    json addVehicle(const Vehicle& v);

    // Add single station with validation
    json addStation(const Station& s);

    // Complete charging at a station and process queue
    json completeCharging(int stationID);

    // ========== NEW FUNCTIONS ==========

    // Binary search on sorted station IDs
    int binarySearchStation(int stationID);

    // Dashboard statistics
    json getDashboard();

    // Get all vehicles
    json getVehicles();

    // Get all stations
    json getStations();

    // Get waiting queue
    json getQueue();

    // Get action history (from stack)
    json getHistory();

    // Get data structures demo info
    json getDataStructuresDemo();

    // Search for a vehicle by ID
    Vehicle* searchVehicle(const std::string& id);

    // Delete a vehicle by ID
    json deleteVehicle(const std::string& id);

    // Update a vehicle by ID
    json updateVehicle(const std::string& id, const Vehicle& v);

    // Update a station by ID
    json updateStation(int id, const Station& s);

    // Sort demonstration
    json sortDemo(const std::string& field, const std::string& order);

    // Search demonstration
    json searchDemo(const std::string& type, const std::string& query);

    // Get stack state for DS explorer
    json getStackState();

    // Push to demo stack
    json pushToStack(const ActionRecord& record);

    // Pop from demo stack
    json popFromStack();

    // Get queue state for DS explorer
    json getQueueState();

    // Get linked list state for DS explorer
    json getLinkedListState();

    // Insert to linked list
    json insertToLinkedList(const RequestRecord& record);

    // Delete from linked list by requestID
    json deleteFromLinkedList(const std::string& requestID);

    // Load sample data
    void loadSampleData();

    // ========== DATABASE INTEGRATION METHODS ==========
    void attachDatabase(Database* dbPtr);
    void syncFromDatabase();
    int findCompatibleStationForBooking(const std::string& connectorType, int preferredStationId = 0);
    Station* getStationByID(int stationID);
    bool reserveStationSlot(int stationID, const std::string& vehicleReg);
    void markStationCompleted(int stationID);
};

#endif // CHARGING_ENGINE_H
