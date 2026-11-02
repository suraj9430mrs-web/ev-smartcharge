#ifndef ARRAY_STORE_H
#define ARRAY_STORE_H

#include <string>
#include <functional>
#include "../lib/json.hpp"

using json = nlohmann::json;

#ifndef MAX
#define MAX 100
#endif

// Template-based fixed-size array store
// Preserves the original array-based approach from the team's code
template <typename T>
class ArrayStore {
private:
    T data[MAX];
    int count;

public:
    ArrayStore() : count(0) {}

    // Add an element to the end
    bool add(const T& item) {
        if (count >= MAX) return false;
        data[count] = item;
        count++;
        return true;
    }

    // Remove element at index, shift remaining elements left
    bool remove(int index) {
        if (index < 0 || index >= count) return false;
        for (int i = index; i < count - 1; i++) {
            data[i] = data[i + 1];
        }
        count--;
        return true;
    }

    // Get element at index (mutable reference)
    T& get(int index) {
        return data[index];
    }

    // Get element at index (const reference)
    const T& get(int index) const {
        return data[index];
    }

    // Search with a predicate, returns index or -1
    int search(std::function<bool(const T&)> predicate) const {
        for (int i = 0; i < count; i++) {
            if (predicate(data[i])) return i;
        }
        return -1;
    }

    // Update element at index
    bool update(int index, const T& item) {
        if (index < 0 || index >= count) return false;
        data[index] = item;
        return true;
    }

    int size() const { return count; }

    bool isEmpty() const { return count == 0; }

    bool isFull() const { return count >= MAX; }

    void clear() { count = 0; }

    // Convert all elements to JSON array
    json toJSON() const {
        json arr = json::array();
        for (int i = 0; i < count; i++) {
            arr.push_back(data[i]);
        }
        return arr;
    }

    // Access underlying array pointer (for sort operations)
    T* rawArray() { return data; }
    const T* rawArray() const { return data; }
};

#endif // ARRAY_STORE_H
