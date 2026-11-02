#ifndef STACK_H
#define STACK_H

#include <string>
#include "../lib/json.hpp"

using json = nlohmann::json;

#ifndef MAX
#define MAX 100
#endif

// Stack entry for recording assignment/completion actions
struct ActionRecord {
    std::string actionType;   // "ASSIGNMENT", "COMPLETION", "PUSH_TEST", etc.
    std::string vehicleID;
    int stationID;
    std::string timestamp;
    std::string details;

    ActionRecord()
        : actionType(""), vehicleID(""), stationID(0),
          timestamp(""), details("") {}

    ActionRecord(const std::string& type, const std::string& vid, int sid,
                 const std::string& ts, const std::string& det)
        : actionType(type), vehicleID(vid), stationID(sid),
          timestamp(ts), details(det) {}
};

inline void to_json(json& j, const ActionRecord& r) {
    j = json{
        {"actionType", r.actionType},
        {"vehicleID", r.vehicleID},
        {"stationID", r.stationID},
        {"timestamp", r.timestamp},
        {"details", r.details}
    };
}

inline void from_json(const json& j, ActionRecord& r) {
    if (j.contains("actionType")) j.at("actionType").get_to(r.actionType);
    if (j.contains("vehicleID")) j.at("vehicleID").get_to(r.vehicleID);
    if (j.contains("stationID")) j.at("stationID").get_to(r.stationID);
    if (j.contains("timestamp")) j.at("timestamp").get_to(r.timestamp);
    if (j.contains("details")) j.at("details").get_to(r.details);
}

// Custom array-based Stack implementation (NOT std::stack)
template <typename T>
class Stack {
private:
    T data[MAX];
    int topIndex;

public:
    Stack() : topIndex(-1) {}

    bool push(const T& item) {
        if (topIndex >= MAX - 1) return false;  // Stack overflow
        topIndex++;
        data[topIndex] = item;
        return true;
    }

    bool pop(T& item) {
        if (topIndex < 0) return false;  // Stack underflow
        item = data[topIndex];
        topIndex--;
        return true;
    }

    bool pop() {
        if (topIndex < 0) return false;
        topIndex--;
        return true;
    }

    T& peek() {
        return data[topIndex];
    }

    const T& peek() const {
        return data[topIndex];
    }

    bool isEmpty() const {
        return topIndex < 0;
    }

    bool isFull() const {
        return topIndex >= MAX - 1;
    }

    int size() const {
        return topIndex + 1;
    }

    void clear() {
        topIndex = -1;
    }

    // Get element at position from bottom (0-indexed)
    const T& at(int index) const {
        return data[index];
    }

    // Convert entire stack to JSON array (bottom to top order)
    json toJSON() const {
        json arr = json::array();
        for (int i = 0; i <= topIndex; i++) {
            json item;
            to_json(item, data[i]);
            arr.push_back(item);
        }
        return arr;
    }

    // Get stack state info for visualization
    json stateJSON() const {
        json state;
        state["size"] = size();
        state["isEmpty"] = isEmpty();
        state["isFull"] = isFull();
        state["capacity"] = MAX;
        state["items"] = toJSON();
        if (!isEmpty()) {
            json topItem;
            to_json(topItem, data[topIndex]);
            state["top"] = topItem;
        } else {
            state["top"] = nullptr;
        }
        return state;
    }
};

// Type alias for the action history stack
using ActionStack = Stack<ActionRecord>;

#endif // STACK_H
