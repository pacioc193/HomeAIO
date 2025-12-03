#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>
#include "ConfigTypes.h"

class ConfigManager {
private:
    const char* filename = "/shelly_conf.json";
    void setDefaults(AppConfig& config);

public:
    bool load(AppConfig& config);
    bool save(const AppConfig& config);
};
