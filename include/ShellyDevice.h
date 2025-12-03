#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "ConfigTypes.h"

class ShellyDevice {
protected:
    String ip;
    String id; // Unique ID
    String mac;
    int channelIndex;
    String friendlyName;
    DeviceRole role;
    int priority;
    
    bool isOn = false;
    float power = 0.0f;
    bool isOnline = false;

    // For TRVs
    float currentTemp = 0.0f;
    float targetTemp = 0.0f;
    float valvePos = 0.0f;

public:
    ShellyDevice(String ip, String id, int channel, DeviceRole role, int priority);
    virtual ~ShellyDevice() {}

    virtual void turnOn() = 0;
    virtual void turnOff() = 0;
    virtual void update() = 0; // Poll status
    virtual void fetchMetadata() = 0; // Get name, etc.
    
    virtual void setTargetTemperature(float temp) {} // Default empty

    // Getters
    String getId() const { return id; }
    String getIp() const { return ip; }
    String getName() const { return friendlyName; }
    bool getIsOn() const { return isOn; }
    float getPower() const { return power; }
    bool getIsOnline() const { return isOnline; }
    float getCurrentTemp() const { return currentTemp; }
    float getTargetTemp() const { return targetTemp; }
    float getValvePos() const { return valvePos; }
    DeviceRole getRole() const { return role; }
    int getPriority() const { return priority; }

    void setFriendlyName(String name) { friendlyName = name; }
    void setPriority(int p) { priority = p; }
    void setRole(DeviceRole r) { role = r; }

    // Factory
    static ShellyDevice* create(DeviceType type, String ip, String id, int channel, DeviceRole role = DeviceRole::UNKNOWN, int priority = 0);
};

class ShellyGen1 : public ShellyDevice {
public:
    ShellyGen1(String ip, String id, int channel, DeviceRole role, int priority);
    void turnOn() override;
    void turnOff() override;
    void update() override;
    void fetchMetadata() override;
};

class ShellyGen2 : public ShellyDevice {
public:
    ShellyGen2(String ip, String id, int channel, DeviceRole role, int priority);
    void turnOn() override;
    void turnOff() override;
    void update() override;
    void fetchMetadata() override;
};

class ShellyBluTrv : public ShellyDevice {
private:
    int componentId; // e.g. 200 for thermostat:200
public:
    ShellyBluTrv(String gatewayIp, String id, int componentId, DeviceRole role, int priority);
    void turnOn() override; // Maybe set mode?
    void turnOff() override;
    void update() override;
    void fetchMetadata() override;
    void setTargetTemperature(float temp) override;
};
