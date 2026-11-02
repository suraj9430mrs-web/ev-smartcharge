#include "charging_engine.h"
#include "../db/database.h"
#include <iostream>
#include <cmath>
#include <cstring>

// ============================================================
// UTILITY METHODS
// ============================================================

std::string ChargingEngine::getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* ltm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(ltm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string ChargingEngine::generateRequestID() {
    requestCounter++;
    std::ostringstream oss;
    oss << "REQ-" << std::setfill('0') << std::setw(4) << requestCounter;
    return oss.str();
}

// ============================================================
// CONSTRUCTOR
// ============================================================

ChargingEngine::ChargingEngine()
    : db(nullptr), vehicleCount(0), stationCount(0),
      totalAssignments(0), totalCompletions(0), requestCounter(0) {}

// ============================================================
// PRESERVED FUNCTIONS (refactored from original team code)
// ============================================================

// BUBBLE SORT - sorts stations by charging speed descending
// Preserved exactly from original: bubble sort with adjacent swaps
void ChargingEngine::sortStations() {
    for (int i = 0; i < stationCount - 1; i++) {
        for (int j = 0; j < stationCount - i - 1; j++) {
            if (stations[j].chargingSpeed < stations[j + 1].chargingSpeed) {
                Station temp = stations[j];
                stations[j] = stations[j + 1];
                stations[j + 1] = temp;
            }
        }
    }
}

// SEQUENTIAL SEARCH - finds available compatible station
// Preserved exactly from original: checks availability AND connector type match
int ChargingEngine::searchStation(const Vehicle& v) {
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].available == true && stations[i].connectorType == v.connectorType) {
            return i;
        }
    }
    return -1;
}

// ASSIGN SLOT - assigns a vehicle to a compatible station
// Preserved from original with added history tracking
bool ChargingEngine::assignSlot(const Vehicle& v) {
    int index = searchStation(v);
    if (index == -1) return false;

    stations[index].available = false;
    stations[index].currentVehicle = v.vehicleID;

    // Charging time = (requiredEnergy / chargingSpeed) * 60 minutes
    float chargingTime = (v.requiredEnergy / stations[index].chargingSpeed) * 60.0f;

    totalAssignments++;

    // Push assignment action to history stack
    std::ostringstream details;
    details << "Vehicle " << v.vehicleID << " assigned to Station "
            << stations[index].stationID << " (Speed: " << stations[index].chargingSpeed
            << "kW, Est. Time: " << std::fixed << std::setprecision(1)
            << chargingTime << " min)";

    ActionRecord record("ASSIGNMENT", v.vehicleID, stations[index].stationID,
                        getCurrentTimestamp(), details.str());
    actionHistory.push(record);

    // Log to request linked list
    RequestRecord reqRec(generateRequestID(), v.vehicleID, "ASSIGNED",
                         getCurrentTimestamp(), details.str());
    requestLog.insertFront(reqRec);

    return true;
}

// PROCESS QUEUE - iterates waiting queue and tries to assign each vehicle
// Preserved from original: iterate and assign, remove on success, skip on failure
void ChargingEngine::processQueue() {
    int i = 0;
    int qSize = waitingQueue.size();
    while (i < qSize) {
        Vehicle v = waitingQueue.at(i);
        if (assignSlot(v)) {
            waitingQueue.removeAt(i);
            qSize--;
            // Don't increment i since elements shifted
        } else {
            i++;
        }
    }
}

// ADD VEHICLE with validation and duplicate checking
json ChargingEngine::addVehicle(const Vehicle& v) {
    json result;

    // Validate required fields
    if (v.vehicleID.empty()) {
        result["success"] = false;
        result["message"] = "Vehicle ID is required.";
        return result;
    }
    if (v.connectorType.empty()) {
        result["success"] = false;
        result["message"] = "Connector type is required.";
        return result;
    }
    if (v.requiredEnergy <= 0) {
        result["success"] = false;
        result["message"] = "Required energy must be greater than 0.";
        return result;
    }
    if (v.batteryLevel < 0 || v.batteryLevel > 100) {
        result["success"] = false;
        result["message"] = "Battery level must be between 0 and 100.";
        return result;
    }

    // Check for duplicate vehicle ID
    for (int i = 0; i < vehicleCount; i++) {
        if (vehicles[i].vehicleID == v.vehicleID) {
            result["success"] = false;
            result["message"] = "Vehicle with ID '" + v.vehicleID + "' already exists.";
            return result;
        }
    }

    // Check array capacity
    if (vehicleCount >= MAX) {
        result["success"] = false;
        result["message"] = "Maximum vehicle capacity reached.";
        return result;
    }

    // Add to vehicles array
    vehicles[vehicleCount] = v;
    vehicleCount++;

    // Log registration to linked list
    RequestRecord reqRec(generateRequestID(), v.vehicleID, "REGISTERED",
                         getCurrentTimestamp(),
                         "Vehicle registered: " + v.connectorType + ", " +
                         std::to_string(v.requiredEnergy) + "kWh needed");
    requestLog.insertFront(reqRec);

    // Try to assign a slot
    // If queue has waiting vehicles, enqueue this one too (FIFO fairness)
    if (!waitingQueue.isEmpty()) {
        waitingQueue.enqueue(v);
        RequestRecord queueRec(generateRequestID(), v.vehicleID, "QUEUED",
                               getCurrentTimestamp(),
                               "Added to waiting queue (position " +
                               std::to_string(waitingQueue.size()) + ")");
        requestLog.insertFront(queueRec);

        result["success"] = true;
        result["message"] = "Vehicle registered. Added to waiting queue (position " +
                            std::to_string(waitingQueue.size()) + ") - other vehicles waiting.";
        result["assigned"] = false;
        result["queuePosition"] = waitingQueue.size();
    } else if (assignSlot(v)) {
        result["success"] = true;
        result["message"] = "Vehicle registered and assigned to a charging station.";
        result["assigned"] = true;
    } else {
        // No compatible station available, add to waiting queue
        waitingQueue.enqueue(v);
        RequestRecord queueRec(generateRequestID(), v.vehicleID, "QUEUED",
                               getCurrentTimestamp(),
                               "No compatible station available. Added to waiting queue.");
        requestLog.insertFront(queueRec);

        result["success"] = true;
        result["message"] = "Vehicle registered. No compatible station available - added to waiting queue.";
        result["assigned"] = false;
        result["queuePosition"] = waitingQueue.size();
    }

    result["vehicle"] = v;
    return result;
}

// ADD STATION with validation
json ChargingEngine::addStation(const Station& s) {
    json result;

    // Validate
    if (s.stationID <= 0) {
        result["success"] = false;
        result["message"] = "Station ID must be a positive number.";
        return result;
    }
    if (s.chargingSpeed <= 0) {
        result["success"] = false;
        result["message"] = "Charging speed must be greater than 0.";
        return result;
    }
    if (s.connectorType.empty()) {
        result["success"] = false;
        result["message"] = "Connector type is required.";
        return result;
    }

    // Check for duplicate station ID
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].stationID == s.stationID) {
            result["success"] = false;
            result["message"] = "Station with ID " + std::to_string(s.stationID) + " already exists.";
            return result;
        }
    }

    if (stationCount >= MAX) {
        result["success"] = false;
        result["message"] = "Maximum station capacity reached.";
        return result;
    }

    stations[stationCount] = s;
    stationCount++;

    // Re-sort stations by charging speed descending
    sortStations();

    result["success"] = true;
    result["message"] = "Station " + std::to_string(s.stationID) + " added successfully.";
    result["station"] = s;
    return result;
}

// COMPLETE CHARGING - mark station available and process queue
json ChargingEngine::completeCharging(int stationID) {
    json result;

    // Find station
    int stationIndex = -1;
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].stationID == stationID) {
            stationIndex = i;
            break;
        }
    }

    if (stationIndex == -1) {
        result["success"] = false;
        result["message"] = "Station " + std::to_string(stationID) + " not found.";
        return result;
    }

    if (stations[stationIndex].available) {
        result["success"] = false;
        result["message"] = "Station " + std::to_string(stationID) + " is already available (no vehicle charging).";
        return result;
    }

    std::string completedVehicle = stations[stationIndex].currentVehicle;

    // Mark station as available
    stations[stationIndex].available = true;
    stations[stationIndex].currentVehicle = "";

    totalCompletions++;

    // Push completion action to history stack
    std::ostringstream details;
    details << "Vehicle " << completedVehicle << " completed charging at Station " << stationID;
    ActionRecord record("COMPLETION", completedVehicle, stationID,
                        getCurrentTimestamp(), details.str());
    actionHistory.push(record);

    // Log to linked list
    RequestRecord reqRec(generateRequestID(), completedVehicle, "COMPLETED",
                         getCurrentTimestamp(), details.str());
    requestLog.insertFront(reqRec);

    // Process waiting queue for compatible vehicles
    int queueSizeBefore = waitingQueue.size();
    processQueue();
    int assigned = queueSizeBefore - waitingQueue.size();

    if (db) {
        db->updateStationStatus(stationID, "Operational", true, "");
        for (int i = 0; i < stationCount; i++) {
            if (!stations[i].available && !stations[i].currentVehicle.empty()) {
                db->updateStationStatus(stations[i].stationID, "Operational", false, stations[i].currentVehicle);
            }
        }
    }

    result["success"] = true;
    result["message"] = "Charging completed for vehicle " + completedVehicle +
                        " at Station " + std::to_string(stationID) + ".";
    result["completedVehicle"] = completedVehicle;
    result["stationID"] = stationID;
    result["queueVehiclesAssigned"] = assigned;
    result["queueRemaining"] = waitingQueue.size();
    return result;
}

// ============================================================
// NEW FUNCTIONS
// ============================================================

// Binary search on station array by stationID
// Requires stations sorted by ID - we create a temporary sorted copy
int ChargingEngine::binarySearchStation(int stationID) {
    // Since stations are sorted by chargingSpeed, we do a linear index mapping
    // Create sorted index array by stationID
    int indices[MAX];
    for (int i = 0; i < stationCount; i++) indices[i] = i;

    // Sort indices by stationID
    for (int i = 0; i < stationCount - 1; i++) {
        for (int j = 0; j < stationCount - i - 1; j++) {
            if (stations[indices[j]].stationID > stations[indices[j + 1]].stationID) {
                int temp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp;
            }
        }
    }

    // Binary search on sorted indices
    int low = 0, high = stationCount - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (stations[indices[mid]].stationID == stationID) {
            return indices[mid]; // Return actual index in stations array
        } else if (stations[indices[mid]].stationID < stationID) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

// Dashboard statistics
json ChargingEngine::getDashboard() {
    json dashboard;

    int availableStations = 0;
    int occupiedStations = 0;
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].available) availableStations++;
        else occupiedStations++;
    }

    // Count connector types
    int ccsCount = 0, chademoCount = 0, type2Count = 0;
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].connectorType == "CCS") ccsCount++;
        else if (stations[i].connectorType == "CHAdeMO") chademoCount++;
        else if (stations[i].connectorType == "Type2") type2Count++;
    }

    dashboard["totalVehicles"] = vehicleCount;
    dashboard["totalStations"] = stationCount;
    dashboard["availableStations"] = availableStations;
    dashboard["occupiedStations"] = occupiedStations;
    dashboard["queueLength"] = waitingQueue.size();
    dashboard["totalAssignments"] = totalAssignments;
    dashboard["totalCompletions"] = totalCompletions;
    dashboard["historySize"] = actionHistory.size();
    dashboard["requestLogSize"] = requestLog.size();

    dashboard["connectorBreakdown"] = {
        {"CCS", ccsCount},
        {"CHAdeMO", chademoCount},
        {"Type2", type2Count}
    };

    // Utilization rate
    if (stationCount > 0) {
        dashboard["utilizationRate"] = (float)occupiedStations / stationCount * 100.0f;
    } else {
        dashboard["utilizationRate"] = 0.0f;
    }

    return dashboard;
}

json ChargingEngine::getVehicles() {
    json arr = json::array();
    for (int i = 0; i < vehicleCount; i++) {
        json v = vehicles[i];
        // Add assignment info
        bool isAssigned = false;
        int assignedStation = -1;
        for (int j = 0; j < stationCount; j++) {
            if (!stations[j].available && stations[j].currentVehicle == vehicles[i].vehicleID) {
                isAssigned = true;
                assignedStation = stations[j].stationID;
                break;
            }
        }
        v["isAssigned"] = isAssigned;
        v["assignedStation"] = assignedStation;

        // Check if in queue
        bool inQueue = false;
        int queuePos = -1;
        for (int q = 0; q < waitingQueue.size(); q++) {
            if (waitingQueue.at(q).vehicleID == vehicles[i].vehicleID) {
                inQueue = true;
                queuePos = q + 1;
                break;
            }
        }
        v["inQueue"] = inQueue;
        v["queuePosition"] = queuePos;

        arr.push_back(v);
    }
    return arr;
}

json ChargingEngine::getStations() {
    json arr = json::array();
    for (int i = 0; i < stationCount; i++) {
        arr.push_back(stations[i]);
    }
    return arr;
}

json ChargingEngine::getQueue() {
    return waitingQueue.toJSON();
}

json ChargingEngine::getHistory() {
    return actionHistory.toJSON();
}

json ChargingEngine::getDataStructuresDemo() {
    json ds;

    // Array info
    ds["array"] = {
        {"type", "Fixed-Size Array"},
        {"description", "Stores vehicles and stations in contiguous memory. O(1) access by index, O(n) search."},
        {"vehicleCount", vehicleCount},
        {"stationCount", stationCount},
        {"maxCapacity", MAX},
        {"vehicles", getVehicles()},
        {"stations", getStations()}
    };

    // Stack info
    ds["stack"] = actionHistory.stateJSON();
    ds["stack"]["type"] = "Array-Based Stack (LIFO)";
    ds["stack"]["description"] = "Tracks assignment/completion history. Last action is always on top. O(1) push/pop.";

    // Queue info
    ds["queue"] = waitingQueue.stateJSON();
    ds["queue"]["type"] = "Array-Based Queue with Compaction (FIFO)";
    ds["queue"]["description"] = "Manages waiting vehicles. First vehicle to wait is first to be assigned. Uses compaction to reuse array space.";

    // Linked List info
    ds["linkedList"] = requestLog.stateJSON();
    ds["linkedList"]["type"] = "Singly Linked List";
    ds["linkedList"]["description"] = "Dynamic request log. New entries added at front. O(1) insertion at front, O(n) search.";

    // Algorithms info
    ds["algorithms"] = {
        {"bubbleSort", {
            {"name", "Bubble Sort"},
            {"description", "Sorts stations by charging speed in descending order. Compares adjacent elements and swaps if needed."},
            {"timeComplexity", "O(n²)"},
            {"spaceComplexity", "O(1)"},
            {"currentOrder", "Descending by charging speed"}
        }},
        {"sequentialSearch", {
            {"name", "Sequential/Linear Search"},
            {"description", "Finds the first available station matching a vehicle's connector type. Scans from index 0 to n-1."},
            {"timeComplexity", "O(n)"},
            {"spaceComplexity", "O(1)"}
        }},
        {"binarySearch", {
            {"name", "Binary Search"},
            {"description", "Searches for a station by ID using divide-and-conquer. Requires sorted data."},
            {"timeComplexity", "O(log n)"},
            {"spaceComplexity", "O(1)"}
        }}
    };

    return ds;
}

Vehicle* ChargingEngine::searchVehicle(const std::string& id) {
    for (int i = 0; i < vehicleCount; i++) {
        if (vehicles[i].vehicleID == id) {
            return &vehicles[i];
        }
    }
    return nullptr;
}

json ChargingEngine::deleteVehicle(const std::string& id) {
    json result;

    int index = -1;
    for (int i = 0; i < vehicleCount; i++) {
        if (vehicles[i].vehicleID == id) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        result["success"] = false;
        result["message"] = "Vehicle '" + id + "' not found.";
        return result;
    }

    // Check if vehicle is currently charging
    for (int i = 0; i < stationCount; i++) {
        if (!stations[i].available && stations[i].currentVehicle == id) {
            // Free the station
            stations[i].available = true;
            stations[i].currentVehicle = "";
            break;
        }
    }

    // Remove from waiting queue if present
    for (int i = 0; i < waitingQueue.size(); i++) {
        if (waitingQueue.at(i).vehicleID == id) {
            waitingQueue.removeAt(i);
            break;
        }
    }

    // Remove from vehicles array (shift left)
    for (int i = index; i < vehicleCount - 1; i++) {
        vehicles[i] = vehicles[i + 1];
    }
    vehicleCount--;

    // Log deletion
    RequestRecord reqRec(generateRequestID(), id, "DELETED",
                         getCurrentTimestamp(), "Vehicle " + id + " removed from system");
    requestLog.insertFront(reqRec);

    result["success"] = true;
    result["message"] = "Vehicle '" + id + "' deleted successfully.";
    return result;
}

json ChargingEngine::updateVehicle(const std::string& id, const Vehicle& v) {
    json result;

    Vehicle* existing = searchVehicle(id);
    if (existing == nullptr) {
        result["success"] = false;
        result["message"] = "Vehicle '" + id + "' not found.";
        return result;
    }

    // Update fields (preserve vehicleID)
    if (!v.connectorType.empty()) existing->connectorType = v.connectorType;
    if (v.batteryLevel >= 0 && v.batteryLevel <= 100) existing->batteryLevel = v.batteryLevel;
    if (v.requiredEnergy > 0) existing->requiredEnergy = v.requiredEnergy;
    if (!v.ownerName.empty()) existing->ownerName = v.ownerName;
    if (!v.departureTime.empty()) existing->departureTime = v.departureTime;
    if (!v.membershipCategory.empty()) existing->membershipCategory = v.membershipCategory;

    result["success"] = true;
    result["message"] = "Vehicle '" + id + "' updated successfully.";
    result["vehicle"] = *existing;
    return result;
}

json ChargingEngine::updateStation(int id, const Station& s) {
    json result;

    int index = -1;
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].stationID == id) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        result["success"] = false;
        result["message"] = "Station " + std::to_string(id) + " not found.";
        return result;
    }

    // Update fields (preserve stationID)
    if (s.chargingSpeed > 0) stations[index].chargingSpeed = s.chargingSpeed;
    if (!s.connectorType.empty()) stations[index].connectorType = s.connectorType;
    if (!s.location.empty()) stations[index].location = s.location;
    if (!s.maintenanceStatus.empty()) stations[index].maintenanceStatus = s.maintenanceStatus;

    // Re-sort after speed change
    sortStations();

    result["success"] = true;
    result["message"] = "Station " + std::to_string(id) + " updated successfully.";
    result["station"] = stations[index];
    return result;
}

// Sort demonstration - shows the sorting process
json ChargingEngine::sortDemo(const std::string& field, const std::string& order) {
    json result;

    // Create a copy of stations for demonstration
    json beforeSort = json::array();
    for (int i = 0; i < stationCount; i++) {
        beforeSort.push_back(stations[i]);
    }

    // Track sorting steps
    json steps = json::array();
    Station tempStations[MAX];
    for (int i = 0; i < stationCount; i++) {
        tempStations[i] = stations[i];
    }

    bool ascending = (order == "asc");
    int comparisons = 0;
    int swaps = 0;

    // Bubble sort with step tracking
    for (int i = 0; i < stationCount - 1; i++) {
        for (int j = 0; j < stationCount - i - 1; j++) {
            comparisons++;
            bool shouldSwap = false;

            if (field == "chargingSpeed") {
                shouldSwap = ascending
                    ? (tempStations[j].chargingSpeed > tempStations[j + 1].chargingSpeed)
                    : (tempStations[j].chargingSpeed < tempStations[j + 1].chargingSpeed);
            } else if (field == "stationID") {
                shouldSwap = ascending
                    ? (tempStations[j].stationID > tempStations[j + 1].stationID)
                    : (tempStations[j].stationID < tempStations[j + 1].stationID);
            }

            if (shouldSwap) {
                Station temp = tempStations[j];
                tempStations[j] = tempStations[j + 1];
                tempStations[j + 1] = temp;
                swaps++;

                json step;
                step["pass"] = i + 1;
                step["comparison"] = j + 1;
                step["swapped"] = true;
                step["indices"] = {j, j + 1};
                step["description"] = "Swapped Station " +
                    std::to_string(tempStations[j + 1].stationID) + " and Station " +
                    std::to_string(tempStations[j].stationID);
                steps.push_back(step);
            }
        }
    }

    json afterSort = json::array();
    for (int i = 0; i < stationCount; i++) {
        afterSort.push_back(tempStations[i]);
    }

    result["algorithm"] = "Bubble Sort";
    result["field"] = field;
    result["order"] = order;
    result["totalComparisons"] = comparisons;
    result["totalSwaps"] = swaps;
    result["timeComplexity"] = "O(n²)";
    result["beforeSort"] = beforeSort;
    result["afterSort"] = afterSort;
    result["steps"] = steps;
    result["itemCount"] = stationCount;

    return result;
}

// Search demonstration
json ChargingEngine::searchDemo(const std::string& type, const std::string& query) {
    json result;

    if (type == "linear" || type == "sequential") {
        // Sequential search through vehicles
        result["algorithm"] = "Sequential/Linear Search";
        result["timeComplexity"] = "O(n)";
        result["query"] = query;

        json steps = json::array();
        int foundIndex = -1;
        json foundItem = nullptr;

        for (int i = 0; i < vehicleCount; i++) {
            json step;
            step["index"] = i;
            step["comparing"] = vehicles[i].vehicleID;
            step["target"] = query;

            if (vehicles[i].vehicleID == query) {
                step["match"] = true;
                step["description"] = "Match found at index " + std::to_string(i);
                steps.push_back(step);
                foundIndex = i;
                foundItem = vehicles[i];
                break;
            } else {
                step["match"] = false;
                step["description"] = "No match, moving to next element";
                steps.push_back(step);
            }
        }

        // Also search in stations by connector type
        json stationMatches = json::array();
        for (int i = 0; i < stationCount; i++) {
            if (stations[i].connectorType == query) {
                stationMatches.push_back(stations[i]);
            }
        }

        result["found"] = (foundIndex != -1);
        result["foundIndex"] = foundIndex;
        result["foundItem"] = foundItem;
        result["stationMatches"] = stationMatches;
        result["steps"] = steps;
        result["totalComparisons"] = (int)steps.size();
        result["totalElements"] = vehicleCount;

    } else if (type == "binary") {
        // Binary search on stations by ID
        result["algorithm"] = "Binary Search";
        result["timeComplexity"] = "O(log n)";
        result["query"] = query;

        int targetID = 0;
        try {
            targetID = std::stoi(query);
        } catch (...) {
            result["error"] = "Binary search requires a numeric station ID";
            return result;
        }

        // Create sorted copy of station IDs for demonstration
        int sortedIDs[MAX];
        int sortedIndices[MAX];
        for (int i = 0; i < stationCount; i++) {
            sortedIDs[i] = stations[i].stationID;
            sortedIndices[i] = i;
        }
        // Sort by ID
        for (int i = 0; i < stationCount - 1; i++) {
            for (int j = 0; j < stationCount - i - 1; j++) {
                if (sortedIDs[j] > sortedIDs[j + 1]) {
                    int tmp = sortedIDs[j]; sortedIDs[j] = sortedIDs[j + 1]; sortedIDs[j + 1] = tmp;
                    int tmp2 = sortedIndices[j]; sortedIndices[j] = sortedIndices[j + 1]; sortedIndices[j + 1] = tmp2;
                }
            }
        }

        json sortedArray = json::array();
        for (int i = 0; i < stationCount; i++) {
            sortedArray.push_back(sortedIDs[i]);
        }
        result["sortedArray"] = sortedArray;

        json steps = json::array();
        int low = 0, high = stationCount - 1;
        int foundIndex = -1;
        json foundItem = nullptr;

        while (low <= high) {
            int mid = (low + high) / 2;
            json step;
            step["low"] = low;
            step["high"] = high;
            step["mid"] = mid;
            step["midValue"] = sortedIDs[mid];
            step["target"] = targetID;

            if (sortedIDs[mid] == targetID) {
                step["action"] = "FOUND";
                step["description"] = "Target " + std::to_string(targetID) +
                                      " found at position " + std::to_string(mid);
                steps.push_back(step);
                foundIndex = sortedIndices[mid];
                foundItem = stations[foundIndex];
                break;
            } else if (sortedIDs[mid] < targetID) {
                step["action"] = "GO_RIGHT";
                step["description"] = std::to_string(sortedIDs[mid]) + " < " +
                                      std::to_string(targetID) + ", search right half";
                steps.push_back(step);
                low = mid + 1;
            } else {
                step["action"] = "GO_LEFT";
                step["description"] = std::to_string(sortedIDs[mid]) + " > " +
                                      std::to_string(targetID) + ", search left half";
                steps.push_back(step);
                high = mid - 1;
            }
        }

        result["found"] = (foundIndex != -1);
        result["foundIndex"] = foundIndex;
        result["foundItem"] = foundItem;
        result["steps"] = steps;
        result["totalComparisons"] = (int)steps.size();
        result["totalElements"] = stationCount;
    } else {
        result["error"] = "Unknown search type. Use 'linear' or 'binary'.";
    }

    return result;
}

// Stack state for DS explorer
json ChargingEngine::getStackState() {
    return actionHistory.stateJSON();
}

// Push to demo stack
json ChargingEngine::pushToStack(const ActionRecord& record) {
    json result;
    if (actionHistory.push(record)) {
        result["success"] = true;
        result["message"] = "Pushed to stack successfully.";
        result["stack"] = actionHistory.stateJSON();
    } else {
        result["success"] = false;
        result["message"] = "Stack is full (overflow).";
    }
    return result;
}

// Pop from demo stack
json ChargingEngine::popFromStack() {
    json result;
    ActionRecord record;
    if (actionHistory.pop(record)) {
        result["success"] = true;
        result["message"] = "Popped from stack successfully.";
        result["poppedItem"] = record;
        result["stack"] = actionHistory.stateJSON();
    } else {
        result["success"] = false;
        result["message"] = "Stack is empty (underflow).";
    }
    return result;
}

// Queue state for DS explorer
json ChargingEngine::getQueueState() {
    return waitingQueue.stateJSON();
}

// Linked list state for DS explorer
json ChargingEngine::getLinkedListState() {
    return requestLog.stateJSON();
}

// Insert to linked list
json ChargingEngine::insertToLinkedList(const RequestRecord& record) {
    json result;
    requestLog.insertFront(record);
    result["success"] = true;
    result["message"] = "Inserted to linked list (front).";
    result["linkedList"] = requestLog.stateJSON();
    return result;
}

// Delete from linked list by requestID
json ChargingEngine::deleteFromLinkedList(const std::string& requestID) {
    json result;
    bool deleted = requestLog.deleteNode([&](const RequestRecord& r) {
        return r.requestID == requestID;
    });

    if (deleted) {
        result["success"] = true;
        result["message"] = "Deleted request '" + requestID + "' from linked list.";
    } else {
        result["success"] = false;
        result["message"] = "Request '" + requestID + "' not found in linked list.";
    }
    result["linkedList"] = requestLog.stateJSON();
    return result;
}

// ============================================================
// SAMPLE DATA
// ============================================================

void ChargingEngine::loadSampleData() {
    std::cout << "[INIT] Loading operational stations and vehicle fleet...\n";

    // Add stations
    Station s1(101, 150.0f, "CCS", true);
    s1.location = "Bay A1";
    s1.maintenanceStatus = "Operational";
    stations[stationCount++] = s1;

    Station s2(102, 120.0f, "CHAdeMO", true);
    s2.location = "Bay A2";
    s2.maintenanceStatus = "Operational";
    stations[stationCount++] = s2;

    Station s3(103, 50.0f, "Type2", true);
    s3.location = "Bay B1";
    s3.maintenanceStatus = "Operational";
    stations[stationCount++] = s3;

    Station s4(104, 100.0f, "CCS", true);
    s4.location = "Bay B2";
    s4.maintenanceStatus = "Operational";
    stations[stationCount++] = s4;

    Station s5(105, 75.0f, "CHAdeMO", false, "KA-01-AB-1234");
    s5.location = "Bay C1";
    s5.maintenanceStatus = "Operational";
    stations[stationCount++] = s5;

    Station s6(106, 60.0f, "Type2", true);
    s6.location = "Bay C2";
    s6.maintenanceStatus = "Operational";
    stations[stationCount++] = s6;

    // Sort stations by charging speed descending
    sortStations();

    // Add vehicles
    Vehicle v1("KA-01-AB-1234", 20, 45.0f, "CHAdeMO");
    v1.ownerName = "Rahul Kumar";
    v1.membershipCategory = "Premium";
    vehicles[vehicleCount++] = v1;

    Vehicle v2("MH-02-CD-5678", 35, 30.0f, "CCS");
    v2.ownerName = "Priya Sharma";
    v2.membershipCategory = "Regular";
    vehicles[vehicleCount++] = v2;

    Vehicle v3("DL-03-EF-9012", 55, 20.0f, "Type2");
    v3.ownerName = "Amit Patel";
    v3.membershipCategory = "VIP";
    vehicles[vehicleCount++] = v3;

    // Record initial assignment for v1 at station 105
    totalAssignments = 1;
    ActionRecord initialAssignment("ASSIGNMENT", "KA-01-AB-1234", 105,
                                   getCurrentTimestamp(),
                                   "Vehicle KA-01-AB-1234 assigned to Station 105 (Speed: 75kW, Est. Time: 36.0 min)");
    actionHistory.push(initialAssignment);

    // Log sample requests
    RequestRecord req1(generateRequestID(), "KA-01-AB-1234", "REGISTERED",
                       getCurrentTimestamp(), "Vehicle registered: CHAdeMO, 45kWh needed");
    requestLog.insertFront(req1);

    RequestRecord req2(generateRequestID(), "KA-01-AB-1234", "ASSIGNED",
                       getCurrentTimestamp(), "Assigned to Station 105");
    requestLog.insertFront(req2);

    RequestRecord req3(generateRequestID(), "MH-02-CD-5678", "REGISTERED",
                       getCurrentTimestamp(), "Vehicle registered: CCS, 30kWh needed");
    requestLog.insertFront(req3);

    RequestRecord req4(generateRequestID(), "DL-03-EF-9012", "REGISTERED",
                       getCurrentTimestamp(), "Vehicle registered: Type2, 20kWh needed");
    requestLog.insertFront(req4);

    std::cout << "[INFO] Loaded " << stationCount << " operational charging stations and "
              << vehicleCount << " registered vehicles.\n";
}

// ============================================================
// DATABASE INTEGRATION & RESERVATION ENGINE
// ============================================================

void ChargingEngine::attachDatabase(Database* dbPtr) {
    db = dbPtr;
    syncFromDatabase();
}

void ChargingEngine::syncFromDatabase() {
    if (!db) return;

    // Load stations from DB
    json dbStations = db->getAllStations();
    if (dbStations.is_array() && !dbStations.empty()) {
        stationCount = 0;
        for (const auto& sJson : dbStations) {
            if (stationCount >= MAX) break;
            Station s;
            s.stationID = sJson["id"].get<int>();
            s.chargingSpeed = (float)sJson["chargingSpeed"].get<double>();
            s.connectorType = sJson["connectorType"].get<std::string>();
            s.available = sJson["isAvailable"].get<bool>();
            s.currentVehicle = sJson["currentVehicle"].get<std::string>();
            s.location = sJson["location"].get<std::string>();
            s.maintenanceStatus = sJson["operationalStatus"].get<std::string>();
            stations[stationCount++] = s;
        }
        // Bubble sort stations descending by charging speed
        sortStations();
    }

    // Load vehicles from DB
    json dbVehicles = db->getAllVehicles();
    if (dbVehicles.is_array() && !dbVehicles.empty()) {
        vehicleCount = 0;
        for (const auto& vJson : dbVehicles) {
            if (vehicleCount >= MAX) break;
            Vehicle v;
            v.vehicleID = vJson["registrationNumber"].get<std::string>();
            v.connectorType = vJson["connectorType"].get<std::string>();
            v.requiredEnergy = 30.0f;
            v.batteryLevel = 25;
            v.ownerName = vJson["ownerName"].is_string() ? vJson["ownerName"].get<std::string>() : "";
            v.membershipCategory = "Regular";
            vehicles[vehicleCount++] = v;
        }
    }
}

int ChargingEngine::findCompatibleStationForBooking(const std::string& connectorType, int preferredStationId) {
    // 1. If preferred station is specified, check compatibility and availability
    if (preferredStationId > 0) {
        for (int i = 0; i < stationCount; i++) {
            if (stations[i].stationID == preferredStationId) {
                if (stations[i].connectorType == connectorType && stations[i].available && stations[i].maintenanceStatus == "Operational") {
                    return stations[i].stationID;
                }
                break;
            }
        }
    }

    // 2. Otherwise run Sequential Search over Bubble-Sorted array to pick fastest compatible
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].connectorType == connectorType && stations[i].available && stations[i].maintenanceStatus == "Operational") {
            return stations[i].stationID;
        }
    }

    return -1; // None currently available
}

Station* ChargingEngine::getStationByID(int stationID) {
    for (int i = 0; i < stationCount; i++) {
        if (stations[i].stationID == stationID) {
            return &stations[i];
        }
    }
    return nullptr;
}

bool ChargingEngine::reserveStationSlot(int stationID, const std::string& vehicleReg) {
    Station* st = getStationByID(stationID);
    if (!st) return false;

    st->available = false;
    st->currentVehicle = vehicleReg;
    totalAssignments++;

    // Stack audit record
    std::ostringstream details;
    details << "Vehicle " << vehicleReg << " assigned to Station " << stationID << " (Speed: " << st->chargingSpeed << "kW)";
    ActionRecord act("ASSIGNMENT", vehicleReg, stationID, getCurrentTimestamp(), details.str());
    actionHistory.push(act);

    // Singly Linked List record
    RequestRecord req(generateRequestID(), vehicleReg, "ASSIGNED", getCurrentTimestamp(), details.str());
    requestLog.insertFront(req);

    if (db) {
        db->updateStationStatus(stationID, "Operational", false, vehicleReg);
    }
    return true;
}

void ChargingEngine::markStationCompleted(int stationID) {
    completeCharging(stationID);
}

