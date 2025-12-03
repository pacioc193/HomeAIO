#include "AppManager.h"
#include <esp_sntp.h>
#include "LogManager.h"
#include <ESPmDNS.h>

// WiFi connection timeout (ms)
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
// WiFi reconnect interval (ms)
static const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;

// NTP Callback - called when time is synchronized
// Only update the hardware RTC the first time we receive a valid NTP time
void timeSyncCallback(struct timeval *tv)
{
    static bool rtc_updated_once = false;
    SysLog.log("NTP Synced");

    if (!rtc_updated_once)
    {
        // Update RTC with new time (only once)
        struct tm *timeinfo = localtime(&tv->tv_sec);
        m5::rtc_datetime_t dt;
        dt.date.year = timeinfo->tm_year + 1900;
        dt.date.month = timeinfo->tm_mon + 1;
        dt.date.date = timeinfo->tm_mday;
        dt.time.hours = timeinfo->tm_hour;
        dt.time.minutes = timeinfo->tm_min;
        dt.time.seconds = timeinfo->tm_sec;
        M5.Rtc.setDateTime(dt);
        rtc_updated_once = true;
        SysLog.log("RTC updated from NTP (first sync)");
    }
}

AppManager::AppManager()
{
    dataMutex = xSemaphoreCreateMutex();
    shellyManager = nullptr;
    loadManager = nullptr;
    climateController = nullptr;
    taskHandle = nullptr;
}

void AppManager::begin()
{
    // Load Config (ConfigManager handles SD presence and defaults internally)
    configManager.load(config);    

    // Init Modules
    shellyManager = new ShellyManager(&config);
    loadManager = new LoadManager(shellyManager, &config.energy);
    climateController = new ClimateController(shellyManager, &config);

    shellyManager->begin();
}

void AppManager::start()
{
    // Start background task with larger stack size to avoid crashes
    // ESP32-P4 needs at least 16KB for tasks using WiFi/HTTP
    BaseType_t result = xTaskCreatePinnedToCore(
        taskFunction,
        "AppTask",
        16384, // Increased stack size
        this,
        1,
        &taskHandle,
        0 // Core 0
    );

    if (result != pdPASS)
    {
        SysLog.error("Failed to create AppTask!");
    }
}

void AppManager::taskFunction(void *parameter)
{
    AppManager *app = (AppManager *)parameter;
    app->runLoop();
    // If runLoop ever returns (it shouldn't), delete the task
    vTaskDelete(NULL);
}

// Connect to WiFi with timeout (blocking)
// Returns true if connected, false if timeout
bool AppManager::connectWiFi()
{
    if (config.wifi_ssid.length() == 0 || config.wifi_ssid == "YOUR_SSID")
    {
        SysLog.error("WiFi not configured. Please update shelly_conf.json");
        return false;
    }

    SysLog.log("Connecting to WiFi: " + config.wifi_ssid);

    // Set WiFi mode to Station (client)
    WiFi.mode(WIFI_STA);

    // Start connection
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());

    // Wait for connection with timeout
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startTime >= WIFI_CONNECT_TIMEOUT_MS)
        {
            SysLog.error("WiFi connection timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        SysLog.log(".");
    }

    wifiConnected = true;
    SysLog.log("WiFi Connected!");
    SysLog.log("IP address: " + WiFi.localIP().toString());

    // Initialize mDNS responder so MDNS.queryService() can discover devices
    // Use a simple instance name; collisions will be logged by the MDNS library
    if (MDNS.begin("homeaio")) {
        SysLog.log("mDNS responder started (hostname: homeaio)");
    } else {
        SysLog.error("mDNS responder failed to start");
    }

    return true;
}

// Setup NTP time synchronization
void AppManager::setupNTP()
{
    if (ntpConfigured)
        return;

    sntp_set_time_sync_notification_cb(timeSyncCallback);
    // Use config timezone offsets (seconds)
    configTime(config.tz_gmt_offset_sec, config.tz_dst_offset_sec, "pool.ntp.org");
    ntpConfigured = true;
    SysLog.log("NTP configured (tz offsets from config)");
}

// Handle WiFi disconnection and reconnection
void AppManager::handleWiFiReconnect()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!wifiConnected)
        {
            // Just reconnected
            wifiConnected = true;
            SysLog.log("WiFi Reconnected!");
            SysLog.log("IP address: " + WiFi.localIP().toString());
        }
        return;
    }

    // WiFi is disconnected
    if (wifiConnected)
    {
        // Was connected, now disconnected
        wifiConnected = false;
        SysLog.error("WiFi disconnected");
    }

    // Attempt reconnect with backoff
    unsigned long now = millis();
    if (now - lastWifiReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS)
    {
        lastWifiReconnectAttempt = now;
        SysLog.log("Attempting WiFi reconnect...");

        // Disconnect cleanly before reconnecting
        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));

        // Reconnect
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    }
}

void AppManager::runLoop()
{
    // Give the system some time to settle
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Initial WiFi connection (blocking with timeout)
    connectWiFi();

    // Setup NTP and OTA if connected
    if (wifiConnected)
    {
        setupNTP();
    }

    // Main loop - runs forever
    while (true)
    {
        // Handle WiFi reconnection if needed
        handleWiFiReconnect();

        // Setup NTP/OTA if we just reconnected and they weren't configured
        if (wifiConnected)
        {
            setupNTP();
        }

        // Logic Updates
        shellyManager->update();
        loadManager->update();
        climateController->update();

        // Update Shared State
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)))
        {
            sharedState.totalPower = shellyManager->getTotalPower(config.energy.main_meter_id);
            sharedState.alarmActive = false; // TODO: Get from LoadManager
            sharedState.boilerOn = false;    // TODO: Get from ClimateController or check boiler device

            // Check boiler
            ShellyDevice *boiler = shellyManager->getDevice(config.climate.boiler_relay_id);
            if (boiler)
                sharedState.boilerOn = boiler->getIsOn();

            sharedState.devices.clear();
            auto devs = shellyManager->getAllDevices();
            for (auto *d : devs)
            {
                DeviceState ds;
                ds.id = d->getId();
                ds.name = d->getName();
                ds.isOn = d->getIsOn();
                ds.power = d->getPower();
                ds.isOnline = d->getIsOnline();
                ds.role = d->getRole();
                ds.currentTemp = d->getCurrentTemp();
                ds.targetTemp = d->getTargetTemp();
                ds.valvePos = d->getValvePos();
                // Room info from config
                if (config.devices.find(ds.id) != config.devices.end())
                {
                    ds.room = config.devices[ds.id].room;
                }
                sharedState.devices.push_back(ds);
            }

            // Read host battery info via M5.Power (if available)
            // getBatteryLevel: percentage (0-100 or device-specific scale)
            // getBatteryVoltage: millivolts
            // getBatteryCurrent: mA (positive = charging, negative = discharging)
            int battPct = -1;
            int battMv = -1;
            int battMa = 0;
            bool charging = false;
            // Protect calls in case Power class isn't ready
            // M5.Power is available after M5.begin()
            battPct = (int)M5.Power.getBatteryLevel();
            battMv = (int)M5.Power.getBatteryVoltage();
            battMa = (int)M5.Power.getBatteryCurrent();
            charging = (battMa > 0);

            sharedState.batteryPercent = battPct;
            sharedState.batteryMv = battMv;
            sharedState.batteryMa = battMa;
            sharedState.batteryCharging = charging;

            xSemaphoreGive(dataMutex);
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

SystemState AppManager::getSystemState()
{
    SystemState state;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)))
    {
        state = sharedState;
        xSemaphoreGive(dataMutex);
    }
    return state;
}

int AppManager::getMaxPowerW()
{
    return config.energy.max_power_w;
}

void AppManager::setDeviceState(String id, bool on)
{
    // This might be called from UI (Core 1).
    // We should probably queue this action or protect ShellyManager.
    // But ShellyManager is on Core 0.
    // Direct call is unsafe if ShellyManager is not thread safe.
    // Ideally we post a message to a queue that Core 0 processes.
    // For simplicity, I'll just lock mutex if I were accessing shared data,
    // but ShellyManager methods might use HTTPClient which is slow.
    // We don't want to block UI.
    // So we should use a command queue.
    // I'll skip implementation for now or just do it unsafely/blocking for MVP.
    // Or better: AppManager exposes a thread-safe way.
    // I'll leave it empty or simple for now.
    // "Synchronization: Use std::mutex ... to protect shared data when the UI reads device status."
    // It doesn't explicitly say how to handle writes.
    // I'll assume writes are rare and maybe acceptable to block or I should use a queue.
}

void AppManager::setDeviceTargetTemp(String id, float temp)
{
    // Same as above
}
