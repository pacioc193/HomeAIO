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
    // Check if the channel is valid and supported (Relay mode)
    if (!hasRelay) {
        SysLog.log(String("ShellyGen1: ERR - turnOn ignored on ") + id + ". Roller mode or meter-only channel not supported for control.");
        return;
    }

    String url = "http://" + ip + "/relay/" + String(channelIndex) + "?turn=on";
    SysLog.log(String("ShellyGen1: sending turnOn to ") + url);
    HttpResult r = httpGet(url);
    SysLog.debug(String("ShellyGen1: turnOn HTTP code=") + String(r.code) + " payload=" + r.payload);
    
    if (r.code == 200) {
        isOn = true; 
    }
}

void ShellyGen1::turnOff() {
    // Check if the channel is valid and supported (Relay mode)
    if (!hasRelay) {
        SysLog.log(String("ShellyGen1: ERR - turnOff ignored on ") + id + ". Roller mode or meter-only channel not supported for control.");
        return;
    }

    String url = "http://" + ip + "/relay/" + String(channelIndex) + "?turn=off";
    SysLog.log(String("ShellyGen1: sending turnOff to ") + url);
    HttpResult r = httpGet(url);
    SysLog.debug(String("ShellyGen1: turnOff HTTP code=") + String(r.code) + " payload=" + r.payload);
    
    if (r.code == 200) {
        isOn = false;
    }
}

void ShellyGen1::update() {
    String url = "http://" + ip + "/status";
    HttpResult r = httpGet(url);
    
    if (r.code <= 0 || r.payload.length() == 0) {
        SysLog.error(String("ShellyGen1: empty /status response from ") + ip);
        isOnline = false;
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, r.payload);
    if (err) {
        SysLog.error(String("ShellyGen1: JSON parse error from /status ") + ip);
        isOnline = false;
        return;
    }

    isOnline = true;

    // --- ROLLER SHUTTER MODE CHECK (Shelly 2.5) ---
    // If we find the "rollers" array, the device is in shutter/roller mode.
    if (!doc["rollers"].isNull()) {
        // Mode NOT SUPPORTED for control
        hasRelay = false; 
        isOn = false; // State undefined for us
        
        // Attempt to read power if available in the meters
        if (!doc["meters"].isNull() && doc["meters"].size() > 0) {
            // In roller mode there is usually a single aggregated/logical meter at index 0
            power = doc["meters"][0]["power"];
        } else {
            power = 0.0f;
        }

        // Log once or in debug that this is an unsupported mode
        SysLog.debug(String("ShellyGen1: Device ") + id + " in ROLLER SHUTTER mode (unsupported for control). Power=" + String(power));
        return; // Exit, don't look for relays
    }

    // --- STANDARD RELAY / EMETERS LOGIC ---

    // 1. Extract Relay State
    // Shelly 3EM: usually has relays only on channel 0; others are null or missing.
    if (!doc["relays"].isNull() && doc["relays"].size() > channelIndex) {
        isOn = doc["relays"][channelIndex]["ison"];
        hasRelay = true;
    } else {
        // Channel without relay (e.g., Shelly 3EM phases 2 and 3, or device in unusual mode)
        isOn = false;
        hasRelay = false;
    }

    // 2. Extract consumption (Meters or Emeters)
    bool powerFound = false;

    // Priority 1: "meters" (Shelly 1PM, 2.5 Relay Mode)
    if (!doc["meters"].isNull() && doc["meters"].size() > channelIndex) {
        power = doc["meters"][channelIndex]["power"];
        powerFound = true;
    } 
    // Priority 2: "emeters" (Shelly 3EM, EM)
    else if (!doc["emeters"].isNull() && doc["emeters"].size() > channelIndex) {
        power = doc["emeters"][channelIndex]["power"];
        powerFound = true;
    }

    if (!powerFound) power = 0.0f;

    SysLog.debug(String("ShellyGen1: ") + id + " On=" + String(isOn) + " Pwr=" + String(power) + " Rel=" + String(hasRelay));
}

void ShellyGen1::fetchMetadata() {
    String url = "http://" + ip + "/settings";
    HttpResult r = httpGet(url);
    
    if (r.code <= 0 || r.payload.length() == 0) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, r.payload);
    if (err) return;

    // Explicit mode check (e.g., Shelly 2.5)
    if (!doc["mode"].isNull()) {
        String mode = doc["mode"].as<String>();
        if (mode == "roller") {
            SysLog.log(String("ShellyGen1: WARNING - Device ") + id + " detected in 'roller' mode. This mode is NOT supported for control.");
            // We can mark an internal flag here if present in the class header
            // isModeUnsupported = true; 
        }
    }

    // 1. Global device name
    if (!doc["name"].isNull()) {
        String n = doc["name"].as<String>();
        if (n.length() > 0) friendlyName = n;
    }

    // 2. Specific channel name
    String channelName = "";
    
    // Search in relays (Standard)
    if (!doc["relays"].isNull() && doc["relays"].size() > channelIndex) {
        if (!doc["relays"][channelIndex]["name"].isNull()) {
             channelName = doc["relays"][channelIndex]["name"].as<String>();
        }
    }

    // Search in emeters (Shelly 3EM) if not found in relays
    if (channelName.length() == 0 && !doc["emeters"].isNull() && doc["emeters"].size() > channelIndex) {
        if (!doc["emeters"][channelIndex]["name"].isNull()) {
            channelName = doc["emeters"][channelIndex]["name"].as<String>();
        }
    }

    if (channelName.length() > 0) {
        friendlyName = channelName;
    }
    
    if (friendlyName.length() == 0) friendlyName = id;

    SysLog.log(String("ShellyGen1: fetched metadata for ") + id + " name=" + friendlyName);
}

// --- Shelly Gen 2 ---

ShellyGen2::ShellyGen2(String ip, String id, int channel, DeviceRole role, int priority)
    : ShellyDevice(ip, id, channel, role, priority) {}

void ShellyGen2::turnOn() {
    String body = "{\"id\":1, \"method\":\"Switch.Set\", \"params\":{\"id\":" + String(channelIndex) + ", \"on\":true}}";
    String url = "http://" + ip + "/rpc";
    httpPost(url, body);
    isOn = true;
}

void ShellyGen2::turnOff() {
    String body = "{\"id\":1, \"method\":\"Switch.Set\", \"params\":{\"id\":" + String(channelIndex) + ", \"on\":false}}";
    String url = "http://" + ip + "/rpc";
    httpPost(url, body);
    isOn = false;
}

void ShellyGen2::update() {
    String body = "{\"id\":1, \"method\":\"Switch.GetStatus\", \"params\":{\"id\":" + String(channelIndex) + "}}";
    String url = "http://" + ip + "/rpc";
    HttpResult r = httpPost(url, body);

    if (r.code <= 0) {
        isOnline = false;
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, r.payload)) {
        isOnline = false; 
        return;
    }

    if (!doc["result"].isNull()) {
        isOnline = true;
        isOn = doc["result"]["output"];
        power = doc["result"]["apower"];
    } else {
        isOnline = false;
    }
}

void ShellyGen2::fetchMetadata() {
    String body = "{\"id\":1, \"method\":\"Switch.GetConfig\", \"params\":{\"id\":" + String(channelIndex) + "}}";
    String url = "http://" + ip + "/rpc";
    HttpResult r = httpPost(url, body);
    if (r.code <= 0) return;

    JsonDocument doc;
    if (!deserializeJson(doc, r.payload) && !doc["result"].isNull()) {
        String n = doc["result"]["name"].as<String>();
        if (n.length() > 0) friendlyName = n;
    }
}

// --- Shelly Blu TRV ---

ShellyBluTrv::ShellyBluTrv(String gatewayIp, String id, int componentId, DeviceRole role, int priority)
    : ShellyDevice(gatewayIp, id, componentId, role, priority), componentId(componentId) {}

void ShellyBluTrv::turnOn() {} 
void ShellyBluTrv::turnOff() {} 

void ShellyBluTrv::setTargetTemperature(float temp) {
    String body = "{\"id\":1, \"method\":\"Thermostat.SetTargetTemp\", \"params\":{\"id\":" + String(componentId) + ", \"target_C\":" + String(temp) + "}}";
    String url = "http://" + ip + "/rpc";
    httpPost(url, body);
    targetTemp = temp;
}

void ShellyBluTrv::update() {
    String body = "{\"id\":1, \"method\":\"Thermostat.GetStatus\", \"params\":{\"id\":" + String(componentId) + "}}";
    String url = "http://" + ip + "/rpc";
    HttpResult r = httpPost(url, body);

    if (r.code <= 0) { isOnline = false; return; }

    JsonDocument doc;
    if (!deserializeJson(doc, r.payload) && !doc["result"].isNull()) {
        isOnline = true;
        currentTemp = doc["result"]["current_C"];
        targetTemp = doc["result"]["target_C"];
    } else {
        isOnline = false;
    }
}

void ShellyBluTrv::fetchMetadata() {}