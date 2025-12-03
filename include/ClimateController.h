#pragma once

#include <Arduino.h>
#include <time.h>
#include "ConfigTypes.h"
#include "ShellyManager.h"

class ClimateController {
private:
    ShellyManager* shellyManager;
    AppConfig* config;
    
    unsigned long lastUpdate = 0;
    
    int getMinutesFromMidnight(struct tm* timeinfo);
    int parseTime(String timeStr); // "HH:MM" -> minutes

public:
    ClimateController(ShellyManager* mgr, AppConfig* cfg);
    void update();
};
