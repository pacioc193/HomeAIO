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
                String typeStr = (cfg.type == DeviceType::SHELLY_GEN2) ? "GEN2" : (cfg.type == DeviceType::SHELLY_BLU_TRV) ? "BLU_TRV" : "GEN1";
                SysLog.log(String("ShellyManager: added static device Shelly/") + typeStr + " [" + cfg.id + "]");
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
    
    // 1. Search for _http._tcp (Gen1 and some Gen2)
    int n = MDNS.queryService("http", "tcp");
    SysLog.log(String("ShellyManager/GEN1: mDNS found ") + String(n) + " _http._tcp services");
    for (int i = 0; i < n; ++i) {
        String hostname = MDNS.hostname(i);
        String ip = MDNS.address(i).toString();
        
        if (hostname.indexOf("shelly") >= 0) {
            processDiscoveredService(ip, hostname);
        }
    }

    // 2. Search for _shelly._tcp (Gen2 specific)
    n = MDNS.queryService("shelly", "tcp");
    SysLog.log(String("ShellyManager/GEN2: mDNS found ") + String(n) + " _shelly._tcp services");
    for (int i = 0; i < n; ++i) {
        String hostname = MDNS.hostname(i);
        String ip = MDNS.address(i).toString();
        
        // Gen2 devices usually have hostnames starting with shelly...
        // But we process them if found via _shelly._tcp service
        if (hostname.indexOf("shelly") >= 0) {
            processDiscoveredService(ip, hostname);
        }
    }
}

void ShellyManager::processDiscoveredService(String ip, String hostname) {
    // 1. Device identification (Gen1 vs Gen2) and MAC address
    // Use the /shelly endpoint which is common to Gen2 and supported by many Gen1 devices (returns type/mac).
    // If it's Gen2, the JSON contains a "gen" field (2 or greater).
    
    String url = "http://" + ip + "/shelly";
    HttpResult r = httpGet(url);
    if (r.code <= 0) return;

    JsonDocument doc;
    if (deserializeJson(doc, r.payload)) return;

    String mac = "";
    int generation = 1; // Default Gen1

    // Detect MAC and generation
    if (!doc["gen"].isNull()) {
        // It's a Gen2 or newer device (Plus, Pro)
        generation = doc["gen"].as<int>();
        if (!doc["mac"].isNull()) mac = doc["mac"].as<String>();
    } else {
        // Probably Gen1
        // Gen1 on /shelly responds with { "type": "...", "mac": "...", ... }
        if (!doc["mac"].isNull()) mac = doc["mac"].as<String>();
    }

    if (mac.length() == 0) {
        // Fallback for old Gen1 that may not expose a full /shelly endpoint or use /status
        // Try calling getMacFromIp (legacy /status path)
        mac = getMacFromIp(ip);
    }

    if (mac.length() == 0) return;

    // 2. Determine channel count
    int channels = 1;
    if (generation >= 2) {
        // Gen2 logic: query RPC to see how many switches are present
        channels = getChannelCountGen2(ip);
    } else {
        // Gen1 logic: query /status
        channels = getChannelCountGen1(ip);
    }

    // 3. Creazione e Aggiunta Dispositivi
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
            
            DeviceType dtype = (generation >= 2) ? DeviceType::SHELLY_GEN2 : DeviceType::SHELLY_GEN1;

            ShellyDevice* dev = ShellyDevice::create(dtype, ip, id, ch, role, priority);
            
            if (dev) {
                dev->fetchMetadata(); 
                devices[id] = dev;
                
                String typeStr = (generation >= 2) ? "GEN2" : "GEN1";
                String logMsg = String("ShellyManager: added Shelly/") + typeStr + " [" + id + "] name=\"" + dev->getName() + "\"";
                SysLog.log(logMsg);
                
                if (config->devices[id].name.length() == 0) {
                    config->devices[id].name = dev->getName();
                }
            }
        }
    }
}

// Legacy helper: get MAC from /status for Gen1 devices
String ShellyManager::getMacFromIp(String ip) {
    String url = "http://" + ip + "/status";
    HttpResult r = httpGet(url);
    if (r.code <= 0) return "";
    JsonDocument doc;
    if (deserializeJson(doc, r.payload)) return "";
    if (!doc["mac"].isNull()) return doc["mac"].as<String>();
    if (!doc["device"].isNull() && !doc["device"]["mac"].isNull()) return doc["device"]["mac"].as<String>();
    if (!doc["wifi_sta"].isNull() && !doc["wifi_sta"]["mac"].isNull()) return doc["wifi_sta"]["mac"].as<String>();
    return "";
}

// Helper: count channels on Gen1 (using /status)
int ShellyManager::getChannelCountGen1(String ip) {
    String url = "http://" + ip + "/status";
    HttpResult r = httpGet(url);
    if (r.code <= 0) return 1; 
    JsonDocument doc;
    if (deserializeJson(doc, r.payload)) return 1;

    // Check roller mode
    if (!doc["rollers"].isNull()) return 1;
    // Check emeters (3EM)
    if (!doc["emeters"].isNull()) {
        int c = doc["emeters"].size();
        if (c > 0) return c; 
    }
    // Check Relays/Meters
    int relays = 0;
    if (!doc["relays"].isNull()) relays = doc["relays"].size();
    int meters = 0;
    if (!doc["meters"].isNull()) meters = doc["meters"].size();
    int maxc = (relays > meters) ? relays : meters;
    return (maxc > 0) ? maxc : 1;
}

// Helper: count channels on Gen2 (using Shelly.GetConfig RPC)
int ShellyManager::getChannelCountGen2(String ip) {
    // Use Shelly.GetConfig to see how many 'switch' components are configured.
    // Alternative: call Switch.GetConfig for id=0,1,2,... until failure, but Shelly.GetConfig is a single call.
    String body = "{\"id\":1, \"method\":\"Shelly.GetConfig\"}";
    String url = "http://" + ip + "/rpc";
    HttpResult r = httpPost(url, body);
    
    if (r.code != 200) return 1; // Fallback

    JsonDocument doc;
    // Increase capacity if the config JSON is very large (Shelly Pro 4PM)
    if (deserializeJson(doc, r.payload)) return 1;

    if (doc["result"].isNull()) return 1;

    int switchCount = 0;
    JsonObject result = doc["result"].as<JsonObject>();
    
    // Iterate keys to find "switch:0", "switch:1", etc.
    for (JsonPair kv : result) {
        String key = String(kv.key().c_str());
        if (key.startsWith("switch:")) {
            switchCount++;
        }
    }

    // If no switches found, it might be a cover device (e.g. Shelly Plus 2PM in cover mode).
    // To support covers, look for keys like "cover:0". For now return switchCount.
    return (switchCount > 0) ? switchCount : 1;
}

// Keep these methods for interface compatibility, but internally we now use differentiated logic
int ShellyManager::getChannelCount(String ip) {
    return getChannelCountGen1(ip); 
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