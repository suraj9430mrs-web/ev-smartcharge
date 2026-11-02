#!/bin/bash
cd "$(dirname "$0")"
echo "======================================================"
echo "  Building EV SmartCharge C++ REST Platform (v2.0)"
echo "======================================================"
clang++ -std=c++17 -O2 -I lib -o ev_smartcharge main.cpp db/database.cpp services/charging_engine.cpp api/routes.cpp -lsqlite3 -pthread
if [ $? -eq 0 ]; then
    echo "✓ Build successful! Binary: backend/ev_smartcharge"
    echo "  To run: ./ev_smartcharge [port]"
else
    echo "✗ Build failed!"
    exit 1
fi
