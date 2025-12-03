#include "ClimateController.h"

ClimateController::ClimateController(ShellyManager* mgr, AppConfig* cfg) : shellyManager(mgr), config(cfg) {}

int ClimateController::parseTime(String timeStr) {
    int split = timeStr.indexOf(':');
    if (split == -1) return 0;
    int h = timeStr.substring(0, split).toInt();
    int m = timeStr.substring(split + 1).toInt();
    return h * 60 + m;
}

int ClimateController::getMinutesFromMidnight(struct tm* timeinfo) {
    return timeinfo->tm_hour * 60 + timeinfo->tm_min;
}

void ClimateController::update() {
    unsigned long now = millis();
    if (now - lastUpdate < 10000) return; // Check every 10s
    lastUpdate = now;
    
    if (!config->climate.enabled) return;
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return; // Time not set yet
    
    int currentMins = getMinutesFromMidnight(&timeinfo);
    
    // 1. Update Schedules
    for (auto& kv : config->devices) {
        DeviceConfig& devCfg = kv.second;
        if (devCfg.role == DeviceRole::TRV && devCfg.schedule_enabled && !devCfg.schedule.empty()) {
            // Find target temp
            float target = -1.0;
            int bestTime = -1;
            
            // Find latest point <= currentMins
            for (const auto& pt : devCfg.schedule) {
                int ptMins = parseTime(pt.time);
                if (ptMins <= currentMins) {
                    if (ptMins > bestTime) {
                        bestTime = ptMins;
                        target = pt.temp;
                    }
                }
            }
            
            // If no point found today, use the last one from the list (previous day)
            if (target < 0) {
                // Find the point with max time
                int maxTime = -1;
                for (const auto& pt : devCfg.schedule) {
                    int ptMins = parseTime(pt.time);
                    if (ptMins > maxTime) {
                        maxTime = ptMins;
                        target = pt.temp;
                    }
                }
            }
            
            if (target > 0) {
                ShellyDevice* dev = shellyManager->getDevice(devCfg.id);
                if (dev) {
                    // Only update if significantly different to avoid spamming
                    if (abs(dev->getTargetTemp() - target) > 0.1) {
                        dev->setTargetTemperature(target);
                    }
                }
            }
        }
    }
    
    // 2. Boiler Logic
    ShellyDevice* boiler = shellyManager->getDevice(config->climate.boiler_relay_id);
    if (!boiler) return;
    
    if (config->climate.summer_mode) {
        if (boiler->getIsOn()) boiler->turnOff();
        return;
    }
    
    bool needHeat = false;
    auto devices = shellyManager->getAllDevices();
    for (auto* dev : devices) {
        if (dev->getRole() == DeviceRole::TRV) {
            // Check valve pos or temp diff
            if (dev->getValvePos() > 10.0f) {
                needHeat = true;
                break;
            }
            if (dev->getCurrentTemp() < (dev->getTargetTemp() - config->climate.hysteresis)) {
                needHeat = true;
                break;
            }
        }
    }
    
    if (needHeat) {
        if (!boiler->getIsOn()) boiler->turnOn();
    } else {
        if (boiler->getIsOn()) boiler->turnOff();
    }
}
