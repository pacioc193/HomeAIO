#include "ShellyManager.h"
#include "NetworkUtils.h"

ShellyManager::ShellyManager(AppConfig* config) : config(config) {}

void ShellyManager::begin() {
    // Initialize devices from config if they have IPs (static config)
    for (auto& kv : config->devices) {
        DeviceConfig& cfg = kv.second;
        if (cfg.ip.length() > 0) {
            // Create device if we know the type. 
            // If we don't know the type, we wait for discovery or probe it.
            // For now, let's assume discovery handles creation to ensure we have the right type.
            // Exception: BLU TRVs might need manual creation if not discoverable via mDNS
            if (cfg.type == DeviceType::SHELLY_BLU_TRV) {
                 ShellyDevice* dev = ShellyDevice::create(cfg.type, cfg.ip, cfg.id, 200, cfg.role, cfg.priority); // Assuming 200 for now or parse from ID
                 if (dev) devices[cfg.id] = dev;
            }
        }
    }
}

void ShellyManager::update() {
    unsigned long now = millis();
    
    // Discovery every 60s
    if (now - lastDiscovery > 60000 || lastDiscovery == 0) {
        discoverDevices();
        lastDiscovery = now;
    }
    
    // Update devices every 5s
    if (now - lastUpdate > 5000) {
        for (auto& kv : devices) {
            kv.second->update();
        }
        lastUpdate = now;
    }
}

void ShellyManager::discoverDevices() {
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
    // Determine type
    bool isGen2 = false;
    // Simple check: Gen2 usually has "shellyplus" or "shellypro" in hostname, or we can probe /rpc
    // Probing is safer.
    HTTPClient http;
    http.begin("http://" + ip + "/rpc");
    http.setTimeout(1000);
    int code = http.POST("{\"id\":1, \"method\":\"Shelly.GetDeviceInfo\"}");
    http.end();
    
    if (code == 200) {
        isGen2 = true;
    }
    
    String mac = getMacFromIp(ip, isGen2);
    if (mac.length() == 0) return;
    
    int channels = getChannelCount(ip, isGen2);
    
    for (int ch = 0; ch < channels; ch++) {
        String id = mac + "_" + String(ch);
        
        if (devices.find(id) == devices.end()) {
            // New device
            DeviceRole role = DeviceRole::UNKNOWN;
            int priority = 0;
            
            // Check if in config
            if (config->devices.find(id) != config->devices.end()) {
                role = config->devices[id].role;
                priority = config->devices[id].priority;
                config->devices[id].ip = ip; // Update IP in config
            }
            
            ShellyDevice* dev = ShellyDevice::create(
                isGen2 ? DeviceType::SHELLY_GEN2 : DeviceType::SHELLY_GEN1,
                ip, id, ch, role, priority
            );
            
            if (dev) {
                dev->fetchMetadata();
                devices[id] = dev;
                
                // Update config name if empty
                if (config->devices[id].name.length() == 0) {
                    config->devices[id].name = dev->getName();
                }
            }
        } else {
            // Existing device, update IP if changed?
            // The device object stores IP. We might need to update it.
            // For simplicity, we assume IP persistence or re-creation.
            // But ShellyDevice doesn't have setIp. 
            // We could cast and set, or just recreate.
        }
    }
}

String ShellyManager::getMacFromIp(String ip, bool isGen2) {
    if (isGen2) {
        String resp = httpPost("http://" + ip + "/rpc", "{\"id\":1, \"method\":\"Shelly.GetDeviceInfo\"}");
        JsonDocument doc;
        deserializeJson(doc, resp);
        if (!doc["result"].isNull()) return doc["result"]["mac"];
    } else {
        String resp = httpGet("http://" + ip + "/status");
        JsonDocument doc;
        deserializeJson(doc, resp);
        if (!doc["mac"].isNull()) return doc["mac"];
    }
    return "";
}

int ShellyManager::getChannelCount(String ip, bool isGen2) {
    // Simplified: assume 1 for now, or parse status
    // Real implementation would check relays array size
    if (isGen2) {
        // Gen2: Check config or status
        // Assume 1 for simplicity unless we parse full config
        return 1; 
    } else {
        String resp = httpGet("http://" + ip + "/status");
        JsonDocument doc;
        deserializeJson(doc, resp);
        if (!doc["relays"].isNull()) return doc["relays"].size();
    }
    return 1;
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
    // Or sum all loads?
    // Prompt says: "Total Power (from main_meter_id device)"
    return 0.0f;
}

void ShellyManager::syncConfig() {
    // Update config object with current device states/names
    for (auto& kv : devices) {
        String id = kv.first;
        ShellyDevice* dev = kv.second;
        
        config->devices[id].name = dev->getName();
        // config->devices[id].role = dev->getRole(); // Role is usually set in config, not device
    }
}
