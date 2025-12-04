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
    
    // Metodi interni di discovery
    void discoverDevices();
    void processDiscoveredService(String ip, String hostname);
    
    // Metodi Helper
    String getMacFromIp(String ip);
    
    // Gestione conteggio canali (Gen1 vs Gen2)
    int getChannelCount(String ip);      // Wrapper generico (mantenuto per compatibilità)
    int getChannelCountGen1(String ip);  // Logica specifica Gen1 (/status)
    int getChannelCountGen2(String ip);  // Logica specifica Gen2 (RPC)

public:
    ShellyManager(AppConfig* config);
    void begin();
    void update(); // Called in loop
    
    ShellyDevice* getDevice(String id);
    std::vector<ShellyDevice*> getAllDevices();
    
    float getTotalPower(String mainMeterId);
    
    // Helper to sync config with discovered devices
    void syncConfig();
    
    // Save discovered devices to SD as JSON so user can edit config manually.
    // Returns true on success.
    bool saveDiscoveredDevices(const String& path = "/shelly_discovered.json");
};