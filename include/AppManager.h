#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "ConfigTypes.h"
#include "ConfigManager.h"
#include "ShellyManager.h"
#include "LoadManager.h"
#include "ClimateController.h"

class AppManager {
private:
    AppConfig config;
    ConfigManager configManager;
    ShellyManager* shellyManager;
    LoadManager* loadManager;
    ClimateController* climateController;
    
    SemaphoreHandle_t dataMutex;
    TaskHandle_t taskHandle;
    
    bool wifiConnected = false;
    bool ntpConfigured = false;
    unsigned long lastWifiReconnectAttempt = 0;
    SystemState sharedState;
    
    static void taskFunction(void* parameter);
    void runLoop();
    
    // WiFi management
    bool connectWiFi();
    void setupNTP();
    void handleWiFiReconnect();

public:
    AppManager();
    void begin();
    void start();
    
    bool isWifiConnected() { return wifiConnected; }
    void saveShellyDevicesToSD(const String& path) { shellyManager->saveDiscoveredDevices(path); }

    // Thread-safe access for UI
    SystemState getSystemState();
    int getMaxPowerW();
    
    // Control methods for UI
    void setDeviceState(String id, bool on);
    void setDeviceTargetTemp(String id, float temp);
};
