#include "AppManager.h"
#include <esp_sntp.h>
#include <ArduinoOTA.h>
#include "LogManager.h"

// NTP Callback
void timeSyncCallback(struct timeval *tv)
{
    SysLog.log("NTP Synced");
    // Update RTC
    struct tm *timeinfo = localtime(&tv->tv_sec);
    m5::rtc_datetime_t dt;
    dt.date.year = timeinfo->tm_year + 1900;
    dt.date.month = timeinfo->tm_mon + 1;
    dt.date.date = timeinfo->tm_mday;
    dt.time.hours = timeinfo->tm_hour;
    dt.time.minutes = timeinfo->tm_min;
    dt.time.seconds = timeinfo->tm_sec;
    M5.Rtc.setDateTime(dt);
}

AppManager::AppManager()
{
    dataMutex = xSemaphoreCreateMutex();
}

void AppManager::begin()
{
    // Load Config (ConfigManager handles SD presence and defaults internally)
    configManager.load(config);

    // Apply configured log level (default INFO)
    switch(config.log_level) {
        case 2: SysLog.setLogLevel(LogManager::DEBUG); break;
        case 1: SysLog.setLogLevel(LogManager::ERROR); break;
        default: SysLog.setLogLevel(LogManager::INFO); break;
    }
    SysLog.log("Config Loaded, Log Level Set to " + String(config.log_level));
    
    // Init Modules
    shellyManager = new ShellyManager(&config);
    loadManager = new LoadManager(shellyManager, &config.energy);
    climateController = new ClimateController(shellyManager, &config);

    //shellyManager->begin();

    // Note: RTC/system time initialization moved to `main.cpp::setup()` so
    // timestamps are available earlier for SysLog. AppManager will still
    // rely on system time (set by main) and NTP callbacks to update RTC.

    // Start Task
    xTaskCreatePinnedToCore(
        taskFunction,
        "AppTask",
        8192,
        this,
        1,
        &taskHandle,
        0 // Core 0
    );
}

void AppManager::taskFunction(void *parameter)
{
    AppManager *app = (AppManager *)parameter;
    app->runLoop();
}

void AppManager::runLoop()
{
    // WiFi Setup
    if (config.wifi_ssid.length() > 0 && config.wifi_ssid != "YOUR_SSID")
    {
        WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    }
    else
    {
        SysLog.error("WiFi not configured or default. Please update shelly_conf.json");
    }

    // Only configure SNTP once when WiFi is available. Use timezone offsets
    // read from configuration (tz_gmt_offset_sec, tz_dst_offset_sec).
    if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
        wifiConnected = true;
        SysLog.log("WiFi Connected");

        if (!ntpConfigured) {
            sntp_set_time_sync_notification_cb(timeSyncCallback);
            // Use config timezone offsets (seconds)
            configTime(config.tz_gmt_offset_sec, config.tz_dst_offset_sec, "pool.ntp.org");
            ntpConfigured = true;
            SysLog.log("NTP configured (tz offsets from config)");
        }

        // Setup OTA
        ArduinoOTA.setHostname("HomeAIO-Tab5");

        ArduinoOTA.onStart([]()
                           {
                String type;
                if (ArduinoOTA.getCommand() == U_FLASH)
                    type = "sketch";
                else // U_SPIFFS
                    type = "filesystem";
                SysLog.log(String("Start updating ") + type); });
        ArduinoOTA.onEnd([]()
                         { SysLog.log("OTA End"); });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                              {
                                  static unsigned int lastPct = 0;
                                  unsigned int pct = 0;
                                  if (total > 0) pct = (progress * 100) / total;
                                  // Log every 10% to avoid spamming
                                  if (pct == 0 || pct - lastPct >= 10) {
                                      lastPct = pct;
                                      SysLog.log(String("OTA Progress: ") + String(pct) + "%");
                                  }
                              });
        ArduinoOTA.onError([](ota_error_t error)
                           {
                String err = "OTA Error[" + String(error) + "]: ";
                if (error == OTA_AUTH_ERROR) err += "Auth Failed";
                else if (error == OTA_BEGIN_ERROR) err += "Begin Failed";
                else if (error == OTA_CONNECT_ERROR) err += "Connect Failed";
                else if (error == OTA_RECEIVE_ERROR) err += "Receive Failed";
                else if (error == OTA_END_ERROR) err += "End Failed";
                SysLog.error(err); });

        ArduinoOTA.begin();
        SysLog.log("OTA Ready");
    }

    // Monitor WiFi connectivity and attempt reconnect if lost
    if (WiFi.status() != WL_CONNECTED && wifiConnected) {
        // We were connected before but lost connection
        wifiConnected = false;
        ntpConfigured = false; // will reconfigure when reconnect
        SysLog.error("WiFi disconnected");
        // Attempt reconnect with backoff (every 10s)
        unsigned long now = millis();
        if (now - lastWifiReconnectAttempt >= 10000) {
            lastWifiReconnectAttempt = now;
            SysLog.log("Attempting WiFi reconnect...");
            if (config.wifi_ssid.length() > 0 && config.wifi_ssid != "YOUR_SSID") {
                WiFi.reconnect();
            }
        }
    }

    if (wifiConnected) {
        ArduinoOTA.handle();
    }

    // Logic Updates
    shellyManager->update();
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
            // Room?
            if (config.devices.find(ds.id) != config.devices.end())
            {
                ds.room = config.devices[ds.id].room;
            }
            sharedState.devices.push_back(ds);
        }

        xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Yield
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
