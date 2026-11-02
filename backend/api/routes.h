#ifndef ROUTES_H
#define ROUTES_H

#include "../lib/httplib.h"
#include "../services/charging_engine.h"
#include "../db/database.h"

void registerRoutes(httplib::Server& svr, ChargingEngine& engine, Database& db);

#endif // ROUTES_H

