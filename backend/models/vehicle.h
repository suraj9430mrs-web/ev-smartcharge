#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>
#include "../lib/json.hpp"

using json = nlohmann::json;

#define MAX 100

struct Vehicle {
    std::string vehicleID;
    int batteryLevel;        // percentage 0-100
    float requiredEnergy;    // kWh
    std::string connectorType; // CCS, CHAdeMO, Type2

    // Optional extended fields
    std::string ownerName;
    std::string departureTime;
    std::string membershipCategory; // Regular, Premium, VIP

    Vehicle()
        : vehicleID(""), batteryLevel(0), requiredEnergy(0.0f),
          connectorType(""), ownerName(""), departureTime(""),
          membershipCategory("Regular") {}

    Vehicle(const std::string& id, int battery, float energy, const std::string& connector)
        : vehicleID(id), batteryLevel(battery), requiredEnergy(energy),
          connectorType(connector), ownerName(""), departureTime(""),
          membershipCategory("Regular") {}
};

inline void to_json(json& j, const Vehicle& v) {
    j = json{
        {"vehicleID", v.vehicleID},
        {"batteryLevel", v.batteryLevel},
        {"requiredEnergy", v.requiredEnergy},
        {"connectorType", v.connectorType},
        {"ownerName", v.ownerName},
        {"departureTime", v.departureTime},
        {"membershipCategory", v.membershipCategory}
    };
}

inline void from_json(const json& j, Vehicle& v) {
    if (j.contains("vehicleID")) j.at("vehicleID").get_to(v.vehicleID);
    if (j.contains("batteryLevel")) j.at("batteryLevel").get_to(v.batteryLevel);
    if (j.contains("requiredEnergy")) j.at("requiredEnergy").get_to(v.requiredEnergy);
    if (j.contains("connectorType")) j.at("connectorType").get_to(v.connectorType);
    if (j.contains("ownerName")) j.at("ownerName").get_to(v.ownerName);
    if (j.contains("departureTime")) j.at("departureTime").get_to(v.departureTime);
    if (j.contains("membershipCategory")) j.at("membershipCategory").get_to(v.membershipCategory);
}

#endif // VEHICLE_H
