#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include "ConfigTypes.h"
#include "ShellyManager.h"

class LoadManager {
private:
    ShellyManager* shellyManager;
    EnergyConfig* config;
    
    unsigned long overloadStartTime = 0;
    bool isOverloaded = false;
    
    struct ShedDevice {
        String id;
        unsigned long shedTime;
    };
    std::vector<ShedDevice> shedDevices;
    
    unsigned long lastCheck = 0;

public:
    LoadManager(ShellyManager* mgr, EnergyConfig* cfg);
    void update();
};
