#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include "lib/httplib.h"
#include "db/database.h"
#include "services/charging_engine.h"
#include "api/routes.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    int port = 8080;
    const char* envPort = std::getenv("PORT");
    if (envPort) {
        port = std::stoi(envPort);
    } else if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    std::cout << "========================================================\n";
    std::cout << "        EV SMARTCHARGE - INTELLIGENT C++ ENGINE         \n";
    std::cout << "  Commercial Electric Vehicle Charging Operations Engine\n";
    std::cout << "========================================================\n";

    // 1. Initialize SQLite persistent database
    Database db;
    std::string dbPath = "ev_smartcharge.db";
    const char* envDb = std::getenv("DATABASE_PATH");
    if (envDb) {
        dbPath = envDb;
    }
    if (fs::path(dbPath).has_parent_path()) {
        try {
            fs::create_directories(fs::path(dbPath).parent_path());
        } catch (...) {}
    }
    if (!db.init(dbPath)) {
        std::cerr << "[FATAL] Failed to initialize persistent database!\n";
        return 1;
    }

    // 2. Initialize C++ data structures and engine
    ChargingEngine engine;
    std::cout << "[INIT] Initializing C++ data structures (Array, Stack, Queue, LinkedList)...\n";

    // 3. Attach DB and sync data
    engine.attachDatabase(&db);
    engine.loadSampleData();

    // 4. Initialize HTTP Server
    httplib::Server svr;

    // 5. Register all REST API routes
    registerRoutes(svr, engine, db);
    std::cout << "[API] Registered REST API routes under /api/*\n";

    // 6. Locate and mount frontend directory for static file serving
    std::string frontendDir = "";
    std::vector<std::string> searchPaths = {
        "./frontend",
        "../frontend",
        "../../frontend",
        "/Users/surajkumar/Downloads/EV CHarging/frontend"
    };

    for (const auto& path : searchPaths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            frontendDir = fs::canonical(path).string();
            break;
        }
    }

    if (!frontendDir.empty()) {
        std::cout << "[HTTP] Mounting static frontend directory: " << frontendDir << "\n";
        svr.set_mount_point("/", frontendDir);
    } else {
        std::cerr << "[WARN] Frontend directory not found in candidate paths!\n";
    }

    // Default route redirects or serves index
    svr.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        if (!frontendDir.empty()) {
            std::string indexPath = frontendDir + "/index.html";
            if (fs::exists(indexPath)) {
                std::ifstream f(indexPath);
                std::string content((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                res.set_content(content, "text/html");
                return;
            }
        }
        res.set_content("<h1>EV SmartCharge C++ Backend is running!</h1>", "text/html");
    });

    std::cout << "--------------------------------------------------------\n";
    std::cout << " Server is ready and listening on http://0.0.0.0:" << port << "\n";
    std::cout << " Public Web URL: http://localhost:" << port << "/\n";
    std::cout << " Health Check:   http://localhost:" << port << "/health\n";
    std::cout << " Admin Console:  http://localhost:" << port << "/admin.html\n";
    std::cout << " Dashboard:      http://localhost:" << port << "/pages/dashboard.html\n";
    std::cout << " Press Ctrl+C to terminate.\n";
    std::cout << "--------------------------------------------------------\n";

    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "[ERROR] Could not start server on port " << port << "!\n";
        return 1;
    }

    return 0;
}
