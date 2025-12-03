#include "ShellyManager.h"
#include "NetworkUtils.h"
#include "LogManager.h"
#include <SD.h>
#include <ArduinoJson.h>

ShellyManager::ShellyManager(AppConfig* config) : config(config) {}

void ShellyManager::begin() {
    for (auto& kv : config->devices) {
        DeviceConfig& cfg = kv.second;
        if (cfg.ip.length() > 0) {
            if (cfg.type == DeviceType::SHELLY_BLU_TRV) {
                 ShellyDevice* dev = ShellyDevice::create(cfg.type, cfg.ip, cfg.id, 200, cfg.role, cfg.priority); 
                 if (dev) devices[cfg.id] = dev;
                SysLog.log(String("ShellyManager: added static device: ") + cfg.id);
            }
        }
    }
    SysLog.log("ShellyManager: begin completed");
}

void ShellyManager::update() {
    unsigned long now = millis();
    
    if (now - lastDiscovery > 60000 || lastDiscovery == 0) {
        discoverDevices();
        lastDiscovery = now;
    }
    
    if (now - lastUpdate > 1000) {
        for (auto& kv : devices) {
            kv.second->update();
        }
        lastUpdate = now;
    }
}

void ShellyManager::discoverDevices() {
    SysLog.log("ShellyManager: starting mDNS discovery");
    int n = MDNS.queryService("http", "tcp");
    
    for (int i = 0; i < n; ++i) {
        String hostname = MDNS.hostname(i);
        String ip = MDNS.address(i).toString();
        
        if (hostname.indexOf("shelly") >= 0) {
            processDiscoveredService(ip, hostname);
        }
    }
}

void ShellyManager::processDiscoveredService(String ip, String hostname) {
    String mac = getMacFromIp(ip);
    if (mac.length() == 0) return;

    int channels = getChannelCount(ip);
    
    // Loop over available channels
    for (int ch = 0; ch < channels; ch++) {
        String id = mac + "_" + String(ch);
        
        if (devices.find(id) == devices.end()) {
            DeviceRole role = DeviceRole::UNKNOWN;
            int priority = 0;
            
            if (config->devices.find(id) != config->devices.end()) {
                role = config->devices[id].role;
                priority = config->devices[id].priority;
                config->devices[id].ip = ip; 
            }
            
            // Create Gen1 device (covers 1, 1PM, 2.5, 3EM)
            ShellyDevice* dev = ShellyDevice::create(
                DeviceType::SHELLY_GEN1,
                ip, id, ch, role, priority
            );
            
            if (dev) {
                dev->fetchMetadata(); // It will detect if it's in Roller mode (unsupported)
                devices[id] = dev;
                
                String logMsg = String("ShellyManager: added ") + id + " [" + dev->getName() + "]";
                SysLog.log(logMsg);
                
                if (config->devices[id].name.length() == 0) {
                    config->devices[id].name = dev->getName();
                }
            }
        }
    }
}

String ShellyManager::getMacFromIp(String ip) {
    String url = "http://" + ip + "/status";
    HttpResult r = httpGet(url);
    if (r.code <= 0) return "";

    JsonDocument doc;
    if (deserializeJson(doc, r.payload)) return "";

    if (!doc["mac"].isNull()) return doc["mac"].as<String>();
    if (!doc["device"].isNull() && !doc["device"]["mac"].isNull()) return doc["device"]["mac"].as<String>();
    // Shelly 3EM fallback
    if (!doc["wifi_sta"].isNull() && !doc["wifi_sta"]["mac"].isNull()) return doc["wifi_sta"]["mac"].as<String>();

    return "";
}

int ShellyManager::getChannelCount(String ip) {
    String url = "http://" + ip + "/status";
    HttpResult r = httpGet(url);
    if (r.code <= 0) return 1; 

    JsonDocument doc;
    if (deserializeJson(doc, r.payload)) return 1;

    // 1. Check Shelly 2.5 in Roller Mode
    // If there's the "rollers" array, it's logically a single entity
    if (!doc["rollers"].isNull()) {
        // Return 1 channel. ShellyGen1::update() will detect Roller and disable controls.
        return 1;
    }

    // 2. Shelly 3EM (o EM)
    if (!doc["emeters"].isNull()) {
        int c = doc["emeters"].size();
        if (c > 0) return c; 
    }

    // 3. Shelly Standard (1, 1PM, 2.5 Relay)
    int relays = 0;
    if (!doc["relays"].isNull()) relays = doc["relays"].size();
    
    int meters = 0;
    if (!doc["meters"].isNull()) meters = doc["meters"].size();

    int maxc = (relays > meters) ? relays : meters;
    return (maxc > 0) ? maxc : 1;
}

ShellyDevice* ShellyManager::getDevice(String id) {
    if (devices.find(id) != devices.end()) return devices[id];
    return nullptr;
}

std::vector<ShellyDevice*> ShellyManager::getAllDevices() {
    std::vector<ShellyDevice*> list;
    for (auto& kv : devices) {
        list.push_back(kv.second);
    }
    return list;
}

float ShellyManager::getTotalPower(String mainMeterId) {
    if (devices.find(mainMeterId) != devices.end()) {
        return devices[mainMeterId]->getPower();
    }
    return 0.0f;
}

void ShellyManager::syncConfig() {
    for (auto& kv : devices) {
        config->devices[kv.first].name = kv.second->getName();
    }
}

bool ShellyManager::saveDiscoveredDevices(const String& path) {
    if (!SysLog.isSdMounted()) SysLog.tryRemount();
    if (!SysLog.isSdMounted()) return false;

    File f = SD.open(path.c_str(), FILE_WRITE);
    if (!f) return false;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (auto &kv : devices) {
        ShellyDevice* dev = kv.second;
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = dev->getId();
        obj["ip"] = dev->getIp();
        
        DeviceType dt = dev->getType();
        if (dt == DeviceType::SHELLY_GEN2) obj["type"] = "GEN2";
        else if (dt == DeviceType::SHELLY_BLU_TRV) obj["type"] = "BLU_TRV";
        else obj["type"] = "GEN1";

        obj["channel"] = dev->getChannelIndex();
        obj["name"] = dev->getName();
        obj["role"] = (int)dev->getRole(); 
    }

    if (serializeJsonPretty(doc, f) == 0) {
        f.close();
        return false;
    }
    
    f.close();
    return true;
}