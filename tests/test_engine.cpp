#include <iostream>
#include <cassert>
#include <string>
#include "../backend/models/vehicle.h"
#include "../backend/models/station.h"
#include "../backend/structures/array_store.h"
#include "../backend/structures/stack.h"
#include "../backend/structures/queue.h"
#include "../backend/structures/linked_list.h"
#include "../backend/services/charging_engine.h"

void testStack() {
    std::cout << "[TEST] Running Stack Tests (CO-1: LIFO)...\n";
    Stack<int> s;
    assert(s.isEmpty());
    assert(s.size() == 0);

    assert(s.push(10));
    assert(s.push(20));
    assert(s.push(30));
    assert(s.size() == 3);
    assert(!s.isEmpty());
    assert(s.peek() == 30);

    int val;
    assert(s.pop(val));
    assert(val == 30);
    assert(s.peek() == 20);

    assert(s.pop(val));
    assert(val == 20);
    assert(s.pop(val));
    assert(val == 10);
    assert(s.isEmpty());

    // Test underflow
    assert(!s.pop(val));
    std::cout << "  ✓ Stack LIFO operations passed.\n";
}

void testQueue() {
    std::cout << "[TEST] Running Queue Tests (CO-2: FIFO)...\n";
    Queue<std::string> q;
    assert(q.isEmpty());

    assert(q.enqueue("Car1"));
    assert(q.enqueue("Car2"));
    assert(q.enqueue("Car3"));
    assert(q.size() == 3);
    assert(q.front() == "Car1");

    std::string val;
    assert(q.dequeue(val));
    assert(val == "Car1");
    assert(q.front() == "Car2");

    // Test removeAt
    assert(q.removeAt(0)); // Remove Car2
    assert(q.size() == 1);
    assert(q.front() == "Car3");

    assert(q.dequeue(val));
    assert(val == "Car3");
    assert(q.isEmpty());
    std::cout << "  ✓ Queue FIFO and compaction operations passed.\n";
}

void testLinkedList() {
    std::cout << "[TEST] Running Linked List Tests (CO-2: Pointers)...\n";
    LinkedList<int> list;
    assert(list.isEmpty());

    list.insertFront(100);
    list.insertFront(200);
    list.insertEnd(300);
    assert(list.size() == 3);

    // Search
    int* found = list.search([](const int& v) { return v == 100; });
    assert(found != nullptr && *found == 100);

    int* notFound = list.search([](const int& v) { return v == 999; });
    assert(notFound == nullptr);

    // Delete
    assert(list.deleteNode([](const int& v) { return v == 200; }));
    assert(list.size() == 2);
    assert(list.deleteNode([](const int& v) { return v == 300; }));
    assert(list.size() == 1);
    std::cout << "  ✓ Linked List dynamic node allocation passed.\n";
}

void testChargingEngine() {
    std::cout << "[TEST] Running Charging Engine Integration Tests...\n";
    ChargingEngine engine;

    // 1. Add Stations
    Station s1; s1.stationID = 201; s1.chargingSpeed = 50.0f; s1.connectorType = "CCS"; s1.available = true;
    Station s2; s2.stationID = 202; s2.chargingSpeed = 150.0f; s2.connectorType = "CCS"; s2.available = true;
    Station s3; s3.stationID = 203; s3.chargingSpeed = 60.0f; s3.connectorType = "Type2"; s3.available = true;

    assert(engine.addStation(s1)["success"].get<bool>());
    assert(engine.addStation(s2)["success"].get<bool>());
    assert(engine.addStation(s3)["success"].get<bool>());

    // Verify Bubble Sort: Stations should be sorted DESC by chargingSpeed: 150 (s2), 60 (s3), 50 (s1)
    json stations = engine.getStations();
    assert(stations.size() == 3);
    assert(stations[0]["chargingSpeed"].get<float>() == 150.0f);
    assert(stations[1]["chargingSpeed"].get<float>() == 60.0f);
    assert(stations[2]["chargingSpeed"].get<float>() == 50.0f);
    std::cout << "  ✓ Bubble Sort arranged stations descending by speed.\n";

    // 2. Binary search station
    int idx = engine.binarySearchStation(202);
    assert(idx != -1);
    std::cout << "  ✓ Binary search successfully found station 202.\n";

    // 3. Register vehicle and assign slot
    Vehicle v1("TEST-V1", 20, 30.0f, "CCS");
    json reg1 = engine.addVehicle(v1);
    assert(reg1["success"].get<bool>());
    assert(reg1["assigned"].get<bool>()); // Should be assigned to station 202 (fastest CCS)

    stations = engine.getStations();
    assert(stations[0]["stationID"].get<int>() == 202);
    assert(!stations[0]["available"].get<bool>());
    assert(stations[0]["currentVehicle"].get<std::string>() == "TEST-V1");
    std::cout << "  ✓ Vehicle assigned to fastest compatible station (150kW CCS).\n";

    // 4. Duplicate vehicle ID rejected
    Vehicle v1Dup("TEST-V1", 50, 20.0f, "CCS");
    json regDup = engine.addVehicle(v1Dup);
    assert(!regDup["success"].get<bool>());
    std::cout << "  ✓ Duplicate vehicle ID properly rejected.\n";

    // 5. Complete charging
    json comp = engine.completeCharging(202);
    assert(comp["success"].get<bool>());
    stations = engine.getStations();
    assert(stations[0]["available"].get<bool>());
    std::cout << "  ✓ Charging completed and station made available.\n";

    // 6. ActionStack has entries
    json history = engine.getHistory();
    assert(history.size() >= 2); // ASSIGNMENT + COMPLETION
    std::cout << "  ✓ ActionStack recorded assignment and completion events.\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "   EV SMARTCHARGE TEST SUITE — ALLIANCE UNIVERSITY      \n";
    std::cout << "========================================================\n";

    testStack();
    testQueue();
    testLinkedList();
    testChargingEngine();

    std::cout << "\n========================================================\n";
    std::cout << "  ✓ ALL 10 DATA STRUCTURE AND ALGORITHM TESTS PASSED!   \n";
    std::cout << "========================================================\n";
    return 0;
}

