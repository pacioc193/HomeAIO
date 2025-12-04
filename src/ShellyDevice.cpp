#include "ShellyDevice.h"
#include "NetworkUtils.h"
#include "LogManager.h"

// --- Base Class ---

ShellyDevice::ShellyDevice(String ip, String id, int channel, DeviceRole role, int priority)
    : ip(ip), id(id), channelIndex(channel), role(role), priority(priority) {}

ShellyDevice* ShellyDevice::create(DeviceType type, String ip, String id, int channel, DeviceRole role, int priority) {
    ShellyDevice* dev = nullptr;
    switch (type) {
        case DeviceType::SHELLY_GEN1:
            dev = new ShellyGen1(ip, id, channel, role, priority);
            break;
        case DeviceType::SHELLY_GEN2:
            dev = new ShellyGen2(ip, id, channel, role, priority);
            break;
        case DeviceType::SHELLY_BLU_TRV:
            dev = new ShellyBluTrv(ip, id, channel, role, priority);
            break;
        default:
            return nullptr;
    }

    if (dev) {
        dev->deviceType = type;
    }
    return dev;
}

// --- Shelly Gen 1 (Shelly 1, 1PM, 2.5, 3EM) ---

ShellyGen1::ShellyGen1(String ip, String id, int channel, DeviceRole role, int priority)
    : ShellyDevice(ip, id, channel, role, priority) {}

void ShellyGen1::turnOn() {
    if (!hasRelay) {
        SysLog.log(String("Shelly/GEN1 [") + id + "]: ERR turnOn ignored - Roller mode or meter-only channel not supported for control.");
        return;
    }

    String url = "http://" + ip + "/relay/" + String(channelIndex) + "?turn=on";
    SysLog.debug(String("Shelly/GEN1 [") + id + "]: sending turnOn -> " + url);
    HttpResult r = httpGet(url);
    SysLog.debug(String("Shelly/GEN1 [") + id + "]: turnOn HTTP code=" + String(r.code) + " payload=" + r.payload);
    
    if (r.code == 200) {
        isOn = true; 
    }
}

void ShellyGen1::turnOff() {
    if (!hasRelay) {
        SysLog.log(String("Shelly/GEN1 [") + id + "]: ERR turnOff ignored - Roller mode or meter-only channel not supported for control.");
        return;
    }

    String url = "http://" + ip + "/relay/" + String(channelIndex) + "?turn=off";
    SysLog.debug(String("Shelly/GEN1 [") + id + "]: sending turnOff -> " + url);
    HttpResult r = httpGet(url);
    SysLog.debug(String("Shelly/GEN1 [") + id + "]: turnOff HTTP code=" + String(r.code) + " payload=" + r.payload);
    
    if (r.code == 200) {
        isOn = false;
    }
}

void ShellyGen1::update() {
    String url = "http://" + ip + "/status";
    HttpResult r = httpGet(url);
    
    if (r.code <= 0 || r.payload.length() == 0) {
        SysLog.error(String("Shelly/GEN1 [") + id + "]: empty /status response from " + ip);
        isOnline = false;
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, r.payload);
    if (err) {
                SysLog.log(String("Shelly/GEN1 [") + id + "]: WARNING - detected 'roller' mode (unsupported for control).");
        isOnline = false;
        return;
    }

    isOnline = true;

    // --- ROLLER SHUTTER MODE CHECK (Shelly 2.5) ---
    if (!doc["rollers"].isNull()) {
        hasRelay = false; 
        isOn = false; 
        if (!doc["meters"].isNull() && doc["meters"].size() > 0) {
            power = doc["meters"][0]["power"];
        } else {
            power = 0.0f;
        }
        SysLog.debug(String("Shelly/GEN1 [") + id + "]: ROLLER SHUTTER mode (unsupported for control). Power=" + String(power));
        return; 
    }

    // --- STANDARD RELAY / EMETERS LOGIC ---
    if (!doc["relays"].isNull() && doc["relays"].size() > channelIndex) {
        isOn = doc["relays"][channelIndex]["ison"];
        hasRelay = true;
    } else {
        isOn = false;
        hasRelay = false;
    }

    bool powerFound = false;
    if (!doc["meters"].isNull() && doc["meters"].size() > channelIndex) {
        power = doc["meters"][channelIndex]["power"];
        powerFound = true;
    } 
    else if (!doc["emeters"].isNull() && doc["emeters"].size() > channelIndex) {
        power = doc["emeters"][channelIndex]["power"];
        powerFound = true;
    }

    if (!powerFound) power = 0.0f;

    SysLog.debug(String("Shelly/GEN1 [") + id + "]: On=" + String(isOn) + " Pwr=" + String(power) + " Rel=" + String(hasRelay));
}

void ShellyGen1::fetchMetadata() {
    String url = "http://" + ip + "/settings";
    HttpResult r = httpGet(url);
    
    if (r.code <= 0 || r.payload.length() == 0) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, r.payload);
    if (err) return;

    if (!doc["mode"].isNull()) {
        String mode = doc["mode"].as<String>();
        if (mode == "roller") {
            SysLog.log(String("Shelly/GEN1 [") + id + "]: WARNING - detected 'roller' mode (unsupported for control).");
        }
    }

    if (!doc["name"].isNull()) {
        String n = doc["name"].as<String>();
        if (n.length() > 0) friendlyName = n;
    }

    String channelName = "";
    if (!doc["relays"].isNull() && doc["relays"].size() > channelIndex) {
        if (!doc["relays"][channelIndex]["name"].isNull()) {
             channelName = doc["relays"][channelIndex]["name"].as<String>();
        }
    }
    if (channelName.length() == 0 && !doc["emeters"].isNull() && doc["emeters"].size() > channelIndex) {
        if (!doc["emeters"][channelIndex]["name"].isNull()) {
            channelName = doc["emeters"][channelIndex]["name"].as<String>();
        }
    }

    if (channelName.length() > 0) {
        friendlyName = channelName;
    }
    if (friendlyName.length() == 0) friendlyName = id;

    SysLog.log(String("Shelly/GEN1 [") + id + "]: fetched metadata name=\"" + friendlyName + "\"");
}

// --- Shelly Gen 2 ---

ShellyGen2::ShellyGen2(String ip, String id, int channel, DeviceRole role, int priority)
    : ShellyDevice(ip, id, channel, role, priority) {}

void ShellyGen2::turnOn() {
    // Gen2 uses RPC over HTTP
    String body = "{\"id\":1, \"method\":\"Switch.Set\", \"params\":{\"id\":" + String(channelIndex) + ", \"on\":true}}";
    String url = "http://" + ip + "/rpc";

    SysLog.debug(String("Shelly/GEN2 [") + id + "]: sending Switch.Set ON -> " + url + " body=" + body);
    HttpResult r = httpPost(url, body); // assume httpPost is available

    if (r.code == 200) {
        isOn = true;
    } else {
        SysLog.error(String("Shelly/GEN2 [") + id + "]: turnOn failed code=" + String(r.code) + " resp=" + r.payload);
    }
}

void ShellyGen2::turnOff() {
    String body = "{\"id\":1, \"method\":\"Switch.Set\", \"params\":{\"id\":" + String(channelIndex) + ", \"on\":false}}";
    String url = "http://" + ip + "/rpc";

    SysLog.debug(String("Shelly/GEN2 [") + id + "]: sending Switch.Set OFF -> " + url + " body=" + body);
    HttpResult r = httpPost(url, body);

    if (r.code == 200) {
        isOn = false;
    } else {
        SysLog.error(String("Shelly/GEN2 [") + id + "]: turnOff failed code=" + String(r.code) + " resp=" + r.payload);
    }
}

void ShellyGen2::update() {
    // Retrieve the specific Switch component status
    String body = "{\"id\":1, \"method\":\"Switch.GetStatus\", \"params\":{\"id\":" + String(channelIndex) + "}}";
    String url = "http://" + ip + "/rpc";
    HttpResult r = httpPost(url, body);

    if (r.code <= 0) {
        SysLog.error(String("Shelly/GEN2 [") + id + "]: empty /rpc response from " + ip);
        isOnline = false;
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, r.payload);
    if (err) {
        SysLog.error(String("Shelly/GEN2 [") + id + "]: JSON error: " + String(err.c_str()));
        isOnline = false; 
        return;
    }

    // Response structure: { "id": 1, "result": { "output": true, "apower": 12.5, ... } }
    if (!doc["result"].isNull()) {
        isOnline = true;
        isOn = doc["result"]["output"];
        // apower is the instantaneous active power
        if (!doc["result"]["apower"].isNull()) {
            power = doc["result"]["apower"];
        } else {
            power = 0.0f;
        }
        
        SysLog.debug(String("Shelly/GEN2 [") + id + "]: On=" + String(isOn) + " Pwr=" + String(power));
    } else {
        SysLog.error(String("Shelly/GEN2 [") + id + "]: API Error or component not found: " + r.payload);
        isOnline = false;
    }
}

void ShellyGen2::fetchMetadata() {
    // 1. Retrieve device configuration (global name)
    // Use Sys.GetConfig
    String sysBody = "{\"id\":1, \"method\":\"Sys.GetConfig\"}";
    String url = "http://" + ip + "/rpc";
    HttpResult rSys = httpPost(url, sysBody);
    
    String globalName = "";
    if (rSys.code == 200) {
        JsonDocument docSys;
        if (!deserializeJson(docSys, rSys.payload)) {
            if (!docSys["result"]["device"]["name"].isNull()) {
                globalName = docSys["result"]["device"]["name"].as<String>();
            }
        }
    }

    // 2. Retrieve channel configuration (switch name)
    String swBody = "{\"id\":2, \"method\":\"Switch.GetConfig\", \"params\":{\"id\":" + String(channelIndex) + "}}";
    HttpResult rSw = httpPost(url, swBody);
    
    String chName = "";
    if (rSw.code == 200) {
        JsonDocument docSw;
        if (!deserializeJson(docSw, rSw.payload)) {
            if (!docSw["result"]["name"].isNull()) {
                chName = docSw["result"]["name"].as<String>();
            }
        }
    }

    // Name combination logic
    if (chName.length() > 0) {
        friendlyName = chName;
    } else if (globalName.length() > 0) {
        // If only the global name exists, use that (optionally append index if multiple channels).
        // For now use the global name when the channel name is empty.
        friendlyName = globalName;
    }

    if (friendlyName.length() == 0) friendlyName = id;
    
    SysLog.log(String("Shelly/GEN2 [") + id + "]: fetched metadata name=\"" + friendlyName + "\"");
}

// --- Shelly Blu TRV ---

ShellyBluTrv::ShellyBluTrv(String gatewayIp, String id, int componentId, DeviceRole role, int priority)
    : ShellyDevice(gatewayIp, id, componentId, role, priority), componentId(componentId) {}

void ShellyBluTrv::turnOn() {} 
void ShellyBluTrv::turnOff() {} 

void ShellyBluTrv::setTargetTemperature(float temp) {
    String body = "{\"id\":1, \"method\":\"Thermostat.SetTargetTemp\", \"params\":{\"id\":" + String(componentId) + ", \"target_C\":" + String(temp) + "}}";
    String url = "http://" + ip + "/rpc";
    SysLog.debug(String("Shelly/BLU_TRV [") + id + "]: RPC set target temp -> " + url + " body=" + body);
    HttpResult r = httpPost(url, body);
    if (r.code <= 0) {
        SysLog.error(String("Shelly/BLU_TRV [") + id + "]: RPC set target temp failed");
    }
    targetTemp = temp;
}

void ShellyBluTrv::update() {
    String body = "{\"id\":1, \"method\":\"Thermostat.GetStatus\", \"params\":{\"id\":" + String(componentId) + "}}";
    String url = "http://" + ip + "/rpc";
    SysLog.debug(String("Shelly/BLU_TRV [") + id + "]: RPC get status -> " + url + " body=" + body);
    HttpResult r = httpPost(url, body);

    if (r.code <= 0) { SysLog.error(String("Shelly/BLU_TRV [") + id + "]: empty /rpc response"); isOnline = false; return; }

    JsonDocument doc;
    if (!deserializeJson(doc, r.payload) && !doc["result"].isNull()) {
        isOnline = true;
        currentTemp = doc["result"]["current_C"];
        targetTemp = doc["result"]["target_C"];
    } else {
        SysLog.error(String("Shelly/BLU_TRV [") + id + "]: RPC parse error or missing result");
        isOnline = false;
    }
}

void ShellyBluTrv::fetchMetadata() {}