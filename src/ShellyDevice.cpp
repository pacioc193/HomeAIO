#include "ShellyDevice.h"
#include "NetworkUtils.h"

// --- Base Class ---

ShellyDevice::ShellyDevice(String ip, String id, int channel, DeviceRole role, int priority)
    : ip(ip), id(id), channelIndex(channel), role(role), priority(priority) {}

ShellyDevice* ShellyDevice::create(DeviceType type, String ip, String id, int channel, DeviceRole role, int priority) {
    switch (type) {
        case DeviceType::SHELLY_GEN1:
            return new ShellyGen1(ip, id, channel, role, priority);
        case DeviceType::SHELLY_GEN2:
            return new ShellyGen2(ip, id, channel, role, priority);
        case DeviceType::SHELLY_BLU_TRV:
            // For BLU TRV, we assume channel is the component ID (e.g. 200)
            return new ShellyBluTrv(ip, id, channel, role, priority);
        default:
            return nullptr;
    }
}

// --- Shelly Gen 1 ---

ShellyGen1::ShellyGen1(String ip, String id, int channel, DeviceRole role, int priority)
    : ShellyDevice(ip, id, channel, role, priority) {}

void ShellyGen1::turnOn() {
    httpGet("http://" + ip + "/relay/" + String(channelIndex) + "?turn=on");
    isOn = true; // Optimistic update
}

void ShellyGen1::turnOff() {
    httpGet("http://" + ip + "/relay/" + String(channelIndex) + "?turn=off");
    isOn = false;
}

void ShellyGen1::update() {
    String json = httpGet("http://" + ip + "/status");
    JsonDocument doc;
    deserializeJson(doc, json);
    
    if (!doc["relays"].isNull()) {
        isOnline = true;
        isOn = doc["relays"][channelIndex]["ison"];
        // Gen1 meters might be separate or inside relays depending on device (PM vs 1)
        // Assuming meters array matches relays for PM devices
        if (!doc["meters"].isNull() && doc["meters"].size() > channelIndex) {
            power = doc["meters"][channelIndex]["power"];
        }
    } else {
        isOnline = false;
    }
}

void ShellyGen1::fetchMetadata() {
    String json = httpGet("http://" + ip + "/settings");
    JsonDocument doc;
    deserializeJson(doc, json);
    if (!doc["name"].isNull()) {
        String n = doc["name"].as<String>();
        if (n.length() > 0) friendlyName = n;
    }
    // Gen1 also has /settings/relay/0 name
    if (!doc["relays"].isNull()) {
         String n = doc["relays"][channelIndex]["name"].as<String>();
         if (n.length() > 0) friendlyName = n;
    }
}

// --- Shelly Gen 2 ---

ShellyGen2::ShellyGen2(String ip, String id, int channel, DeviceRole role, int priority)
    : ShellyDevice(ip, id, channel, role, priority) {}

void ShellyGen2::turnOn() {
    String body = "{\"id\":1, \"method\":\"Switch.Set\", \"params\":{\"id\":" + String(channelIndex) + ", \"on\":true}}";
    httpPost("http://" + ip + "/rpc", body);
    isOn = true;
}

void ShellyGen2::turnOff() {
    String body = "{\"id\":1, \"method\":\"Switch.Set\", \"params\":{\"id\":" + String(channelIndex) + ", \"on\":false}}";
    httpPost("http://" + ip + "/rpc", body);
    isOn = false;
}

void ShellyGen2::update() {
    String body = "{\"id\":1, \"method\":\"Switch.GetStatus\", \"params\":{\"id\":" + String(channelIndex) + "}}";
    String json = httpPost("http://" + ip + "/rpc", body);
    JsonDocument doc;
    deserializeJson(doc, json);
    
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
    String json = httpPost("http://" + ip + "/rpc", body);
    JsonDocument doc;
    deserializeJson(doc, json);
    if (!doc["result"].isNull()) {
        String n = doc["result"]["name"].as<String>();
        if (n.length() > 0) friendlyName = n;
    }
}

// --- Shelly Blu TRV ---

ShellyBluTrv::ShellyBluTrv(String gatewayIp, String id, int componentId, DeviceRole role, int priority)
    : ShellyDevice(gatewayIp, id, componentId, role, priority), componentId(componentId) {}

void ShellyBluTrv::turnOn() {
    // Maybe set mode to heat?
}

void ShellyBluTrv::turnOff() {
    // Maybe set mode to off?
}

void ShellyBluTrv::setTargetTemperature(float temp) {
    String body = "{\"id\":1, \"method\":\"Thermostat.SetTargetTemp\", \"params\":{\"id\":" + String(componentId) + ", \"target_C\":" + String(temp) + "}}";
    httpPost("http://" + ip + "/rpc", body);
    targetTemp = temp;
}

void ShellyBluTrv::update() {
    // Assuming Thermostat component on Gateway
    String body = "{\"id\":1, \"method\":\"Thermostat.GetStatus\", \"params\":{\"id\":" + String(componentId) + "}}";
    String json = httpPost("http://" + ip + "/rpc", body);
    JsonDocument doc;
    deserializeJson(doc, json);
    
    if (!doc["result"].isNull()) {
        isOnline = true;
        currentTemp = doc["result"]["current_C"];
        targetTemp = doc["result"]["target_C"];
        // valvePos might be available?
        // doc["result"]["pos"] ?
    } else {
        isOnline = false;
    }
}

void ShellyBluTrv::fetchMetadata() {
    // Get name from config
    String body = "{\"id\":1, \"method\":\"Thermostat.GetConfig\", \"params\":{\"id\":" + String(componentId) + "}}";
    String json = httpPost("http://" + ip + "/rpc", body);
    JsonDocument doc;
    deserializeJson(doc, json);
    if (!doc["result"].isNull()) {
        String n = doc["result"]["name"].as<String>();
        if (n.length() > 0) friendlyName = n;
    }
}
