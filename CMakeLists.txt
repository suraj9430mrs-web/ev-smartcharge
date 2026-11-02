cmake_minimum_required(VERSION 3.14)
project(EV_SmartCharge VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(backend/lib)

find_package(Threads REQUIRED)

# Backend Executable
add_executable(ev_smartcharge
    backend/main.cpp
    backend/db/database.cpp
    backend/services/charging_engine.cpp
    backend/api/routes.cpp
)

target_link_libraries(ev_smartcharge PRIVATE Threads::Threads sqlite3)

# Test Suite Executable
add_executable(test_runner
    tests/test_engine.cpp
    backend/db/database.cpp
    backend/services/charging_engine.cpp
)
target_link_libraries(test_runner PRIVATE Threads::Threads sqlite3)
