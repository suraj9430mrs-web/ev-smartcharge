#ifndef QUEUE_H
#define QUEUE_H

#include <string>
#include <functional>
#include "../lib/json.hpp"

using json = nlohmann::json;

#ifndef MAX
#define MAX 100
#endif

// Custom array-based Queue implementation (NOT std::queue)
// Preserves the original compaction logic from the team's code:
// - Array-based with front/rear tracking
// - Compaction: when rear hits MAX-1, elements are shifted so front=0
// - removeAt preserves the original removeVehicle shift logic
template <typename T>
class Queue {
private:
    T data[MAX];
    int frontIndex;
    int rearIndex;
    int count;

    // Compact the queue: shift all elements so frontIndex = 0
    // This preserves the original enqueue compaction logic
    void compact() {
        if (frontIndex == 0) return;
        for (int i = 0; i < count; i++) {
            data[i] = data[frontIndex + i];
        }
        frontIndex = 0;
        rearIndex = count - 1;
    }

public:
    Queue() : frontIndex(0), rearIndex(-1), count(0) {}

    // Enqueue: add to rear, compact if needed
    // Preserves original enqueue logic
    bool enqueue(const T& item) {
        if (count >= MAX) return false;  // Queue full
        if (rearIndex >= MAX - 1) {
            compact();
        }
        rearIndex++;
        data[rearIndex] = item;
        count++;
        return true;
    }

    // Dequeue: remove from front
    bool dequeue(T& item) {
        if (count == 0) return false;
        item = data[frontIndex];
        frontIndex++;
        count--;
        if (count == 0) {
            frontIndex = 0;
            rearIndex = -1;
        }
        return true;
    }

    // Peek at front element
    T& front() {
        return data[frontIndex];
    }

    const T& front() const {
        return data[frontIndex];
    }

    // Remove element at a specific position (0-based relative to front)
    // Preserves original removeVehicle logic: shift elements left
    bool removeAt(int position) {
        if (position < 0 || position >= count) return false;

        // Compact first so we work with 0-based indices
        compact();

        // Shift elements left from position
        for (int i = position; i < rearIndex; i++) {
            data[i] = data[i + 1];
        }
        rearIndex--;
        count--;
        if (count == 0) {
            frontIndex = 0;
            rearIndex = -1;
        }
        return true;
    }

    // Get element at position (0-based relative to front)
    T& at(int position) {
        compact();
        return data[position];
    }

    const T& at(int position) const {
        return data[frontIndex + position];
    }

    bool isEmpty() const { return count == 0; }

    bool isFull() const { return count >= MAX; }

    int size() const { return count; }

    void clear() {
        frontIndex = 0;
        rearIndex = -1;
        count = 0;
    }

    // Search with predicate, returns position (0-based) or -1
    int search(std::function<bool(const T&)> predicate) const {
        for (int i = 0; i < count; i++) {
            if (predicate(data[frontIndex + i])) return i;
        }
        return -1;
    }

    // Convert to JSON array
    json toJSON() const {
        json arr = json::array();
        for (int i = 0; i < count; i++) {
            arr.push_back(data[frontIndex + i]);
        }
        return arr;
    }

    // Get queue state info for visualization
    json stateJSON() const {
        json state;
        state["size"] = size();
        state["isEmpty"] = isEmpty();
        state["isFull"] = isFull();
        state["capacity"] = MAX;
        state["frontIndex"] = frontIndex;
        state["rearIndex"] = rearIndex;
        state["items"] = toJSON();
        if (!isEmpty()) {
            state["front"] = data[frontIndex];
        } else {
            state["front"] = nullptr;
        }
        return state;
    }
};

#endif // QUEUE_H
