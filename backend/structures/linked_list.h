#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <string>
#include <functional>
#include "../lib/json.hpp"

using json = nlohmann::json;

// Record for vehicle request history / active session tracking
struct RequestRecord {
    std::string requestID;
    std::string vehicleID;
    std::string action;       // "REGISTERED", "QUEUED", "ASSIGNED", "COMPLETED", "DELETED"
    std::string timestamp;
    std::string details;

    RequestRecord()
        : requestID(""), vehicleID(""), action(""),
          timestamp(""), details("") {}

    RequestRecord(const std::string& rid, const std::string& vid,
                  const std::string& act, const std::string& ts,
                  const std::string& det)
        : requestID(rid), vehicleID(vid), action(act),
          timestamp(ts), details(det) {}
};

inline void to_json(json& j, const RequestRecord& r) {
    j = json{
        {"requestID", r.requestID},
        {"vehicleID", r.vehicleID},
        {"action", r.action},
        {"timestamp", r.timestamp},
        {"details", r.details}
    };
}

inline void from_json(const json& j, RequestRecord& r) {
    if (j.contains("requestID")) j.at("requestID").get_to(r.requestID);
    if (j.contains("vehicleID")) j.at("vehicleID").get_to(r.vehicleID);
    if (j.contains("action")) j.at("action").get_to(r.action);
    if (j.contains("timestamp")) j.at("timestamp").get_to(r.timestamp);
    if (j.contains("details")) j.at("details").get_to(r.details);
}

// Custom singly linked list implementation (NOT std::list)
template <typename T>
class LinkedList {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& d) : data(d), next(nullptr) {}
    };

    Node* head;
    int count;

public:
    LinkedList() : head(nullptr), count(0) {}

    ~LinkedList() {
        clear();
    }

    // Insert at the front of the list
    void insertFront(const T& item) {
        Node* newNode = new Node(item);
        newNode->next = head;
        head = newNode;
        count++;
    }

    // Insert at the end of the list
    void insertEnd(const T& item) {
        Node* newNode = new Node(item);
        if (head == nullptr) {
            head = newNode;
        } else {
            Node* current = head;
            while (current->next != nullptr) {
                current = current->next;
            }
            current->next = newNode;
        }
        count++;
    }

    // Delete node by key (using a predicate)
    bool deleteNode(std::function<bool(const T&)> predicate) {
        if (head == nullptr) return false;

        // Check head
        if (predicate(head->data)) {
            Node* temp = head;
            head = head->next;
            delete temp;
            count--;
            return true;
        }

        // Check rest
        Node* current = head;
        while (current->next != nullptr) {
            if (predicate(current->next->data)) {
                Node* temp = current->next;
                current->next = temp->next;
                delete temp;
                count--;
                return true;
            }
            current = current->next;
        }
        return false;
    }

    // Delete node by string key field
    bool deleteByKey(const std::string& key, std::function<std::string(const T&)> getKey) {
        return deleteNode([&](const T& item) {
            return getKey(item) == key;
        });
    }

    // Search by predicate, returns pointer to data or nullptr
    T* search(std::function<bool(const T&)> predicate) {
        Node* current = head;
        while (current != nullptr) {
            if (predicate(current->data)) {
                return &(current->data);
            }
            current = current->next;
        }
        return nullptr;
    }

    // Search by string key
    T* searchByKey(const std::string& key, std::function<std::string(const T&)> getKey) {
        return search([&](const T& item) {
            return getKey(item) == key;
        });
    }

    // Traverse: apply function to each element
    void traverse(std::function<void(const T&)> func) const {
        Node* current = head;
        while (current != nullptr) {
            func(current->data);
            current = current->next;
        }
    }

    int size() const { return count; }

    bool isEmpty() const { return head == nullptr; }

    void clear() {
        while (head != nullptr) {
            Node* temp = head;
            head = head->next;
            delete temp;
        }
        count = 0;
    }

    // Convert to JSON array (front to end order)
    json toJSON() const {
        json arr = json::array();
        Node* current = head;
        while (current != nullptr) {
            arr.push_back(current->data);
            current = current->next;
        }
        return arr;
    }

    // Get list state info for visualization
    json stateJSON() const {
        json state;
        state["size"] = size();
        state["isEmpty"] = isEmpty();
        state["items"] = toJSON();
        if (head != nullptr) {
            state["head"] = head->data;
        } else {
            state["head"] = nullptr;
        }

        // Build node chain visualization
        json nodes = json::array();
        Node* current = head;
        int idx = 0;
        while (current != nullptr) {
            json nodeInfo;
            nodeInfo["index"] = idx;
            nodeInfo["data"] = current->data;
            nodeInfo["hasNext"] = (current->next != nullptr);
            nodes.push_back(nodeInfo);
            current = current->next;
            idx++;
        }
        state["nodes"] = nodes;
        return state;
    }
};

#endif // LINKED_LIST_H
