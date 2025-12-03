#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include <ESPmDNS.h>
#include "ShellyDevice.h"
#include "ConfigTypes.h"

class ShellyManager {
private:
    std::map<String, ShellyDevice*> devices; // Key: ID (MAC_Channel)
    AppConfig* config; // Reference to global config
    
    unsigned long lastUpdate = 0;
    unsigned long lastDiscovery = 0;
    
    void discoverDevices();
    void processDiscoveredService(String ip, String hostname);
    String getMacFromIp(String ip, bool isGen2);
    int getChannelCount(String ip, bool isGen2);

public:
    ShellyManager(AppConfig* config);
    void begin();
    void update(); // Called in loop
    
    ShellyDevice* getDevice(String id);
    std::vector<ShellyDevice*> getAllDevices();
    
    float getTotalPower(String mainMeterId);
    
    // Helper to sync config with discovered devices
    void syncConfig();
};
