#ifndef STATION_H
#define STATION_H

#include <string>
#include "../lib/json.hpp"

using json = nlohmann::json;

struct Station {
    int stationID;
    float chargingSpeed;       // kW
    std::string connectorType; // CCS, CHAdeMO, Type2
    bool available;
    std::string currentVehicle; // vehicleID if occupied, empty if available

    // Optional extended fields
    std::string location;
    std::string maintenanceStatus; // Operational, Maintenance, Offline

    Station()
        : stationID(0), chargingSpeed(0.0f), connectorType(""),
          available(true), currentVehicle(""), location(""),
          maintenanceStatus("Operational") {}

    Station(int id, float speed, const std::string& connector, bool avail,
            const std::string& vehicle = "")
        : stationID(id), chargingSpeed(speed), connectorType(connector),
          available(avail), currentVehicle(vehicle), location(""),
          maintenanceStatus("Operational") {}
};

inline void to_json(json& j, const Station& s) {
    j = json{
        {"stationID", s.stationID},
        {"chargingSpeed", s.chargingSpeed},
        {"connectorType", s.connectorType},
        {"available", s.available},
        {"currentVehicle", s.currentVehicle},
        {"location", s.location},
        {"maintenanceStatus", s.maintenanceStatus}
    };
}

inline void from_json(const json& j, Station& s) {
    if (j.contains("stationID")) j.at("stationID").get_to(s.stationID);
    if (j.contains("chargingSpeed")) j.at("chargingSpeed").get_to(s.chargingSpeed);
    if (j.contains("connectorType")) j.at("connectorType").get_to(s.connectorType);
    if (j.contains("available")) j.at("available").get_to(s.available);
    if (j.contains("currentVehicle")) j.at("currentVehicle").get_to(s.currentVehicle);
    if (j.contains("location")) j.at("location").get_to(s.location);
    if (j.contains("maintenanceStatus")) j.at("maintenanceStatus").get_to(s.maintenanceStatus);
}

#endif // STATION_H
