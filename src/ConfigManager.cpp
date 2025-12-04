#include "ConfigManager.h"
#include "LogManager.h"

// Centralized defaults
void ConfigManager::setDefaults(AppConfig &config) {
    config.wifi_ssid = "YOUR_SSID";
    config.wifi_password = "YOUR_PASSWORD";

    config.energy.max_power_w = 3300;
    config.energy.buffer_power_w = 200;
    config.energy.cut_off_delay_s = 10;
    config.energy.restore_delay_s = 60;
    config.energy.alarm_enabled = true;
    config.energy.alarm_freq_hz = 2000;
    config.energy.main_meter_id = "AABBCC_0";

    config.climate.enabled = true;
    config.climate.summer_mode = false;
    config.climate.global_setpoint = 21.0;
    config.climate.hysteresis = 0.5;
    config.climate.boiler_relay_id = "DDEEFF_0";
    config.log_level = 0; // INFO
    // Default timezone: CET + DST
    config.tz_gmt_offset_sec = 3600;
    config.tz_dst_offset_sec = 3600;
}

// Load configuration with clear, linear logic:
// - if SD not mounted: log error, set defaults, return false
// - if file not found: set defaults, save file, return true
// - if file exists: parse JSON, detect missing fields, fill defaults for missing, resave if needed
bool ConfigManager::load(AppConfig &config) {
    // 1) Try to read config file from SD dynamically.
    // If SD is not present or file operations fail, fall back to defaults.

    // 2) File missing -> create defaults and persist (attempt)
    if (!SD.exists(filename)) {
        SysLog.log("Config file not found. Creating default configuration file.");
        setDefaults(config);
        if (!save(config)) {
            SysLog.error("Failed to save default config to SD.");
            return false;
        }
        return true;
    }

    // 3) File exists -> read and validate
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        SysLog.error("Unable to open config file. Using defaults.");
        setDefaults(config);
        return false;
    }

    StaticJsonDocument<8192> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        SysLog.error(String("Config JSON parse error: ") + err.c_str());
        setDefaults(config);
        return false;
    }

    bool missing = false;
    String missingFields = "";

    // helper to mark missing
    auto markMissing = [&](const char *name) {
        missing = true;
        if (missingFields.length()) missingFields += ", ";
        missingFields += name;
    };

    // Defaults container to copy missing values from
    AppConfig defs;
    setDefaults(defs);

    // wifi
    if (!doc["wifi_ssid"].isNull()) config.wifi_ssid = doc["wifi_ssid"].as<String>();
    else { config.wifi_ssid = defs.wifi_ssid; markMissing("wifi_ssid"); }

    if (!doc["wifi_password"].isNull()) config.wifi_password = doc["wifi_password"].as<String>();
    else { config.wifi_password = defs.wifi_password; markMissing("wifi_password"); }

    // log level (numeric: 0=INFO,1=ERROR,2=DEBUG)
    if (!doc["log_level"].isNull()) {
        config.log_level = doc["log_level"].as<int>();
    } else {
        config.log_level = defs.log_level;
        markMissing("log_level");
    }

    // timezone offsets (seconds)
    if (!doc["tz_gmt_offset_sec"].isNull()) {
        config.tz_gmt_offset_sec = doc["tz_gmt_offset_sec"].as<int>();
    } else {
        config.tz_gmt_offset_sec = defs.tz_gmt_offset_sec;
        markMissing("tz_gmt_offset_sec");
    }
    if (!doc["tz_dst_offset_sec"].isNull()) {
        config.tz_dst_offset_sec = doc["tz_dst_offset_sec"].as<int>();
    } else {
        config.tz_dst_offset_sec = defs.tz_dst_offset_sec;
        markMissing("tz_dst_offset_sec");
    }

    // Apply log level immediately so subsequent logs during load use it
    SysLog.setLogLevel(config.log_level);

    // energy - check subfields individually
    if (!doc["energy"].isNull()) {
        JsonObject e = doc["energy"];
        if (!e["max_power_w"].isNull()) config.energy.max_power_w = e["max_power_w"] | defs.energy.max_power_w;
        else { config.energy.max_power_w = defs.energy.max_power_w; markMissing("energy.max_power_w"); }

        if (!e["buffer_power_w"].isNull()) config.energy.buffer_power_w = e["buffer_power_w"] | defs.energy.buffer_power_w;
        else { config.energy.buffer_power_w = defs.energy.buffer_power_w; markMissing("energy.buffer_power_w"); }

        if (!e["cut_off_delay_s"].isNull()) config.energy.cut_off_delay_s = e["cut_off_delay_s"] | defs.energy.cut_off_delay_s;
        else { config.energy.cut_off_delay_s = defs.energy.cut_off_delay_s; markMissing("energy.cut_off_delay_s"); }

        if (!e["restore_delay_s"].isNull()) config.energy.restore_delay_s = e["restore_delay_s"] | defs.energy.restore_delay_s;
        else { config.energy.restore_delay_s = defs.energy.restore_delay_s; markMissing("energy.restore_delay_s"); }

        if (!e["alarm_enabled"].isNull()) config.energy.alarm_enabled = e["alarm_enabled"] | defs.energy.alarm_enabled;
        else { config.energy.alarm_enabled = defs.energy.alarm_enabled; markMissing("energy.alarm_enabled"); }

        if (!e["alarm_freq_hz"].isNull()) config.energy.alarm_freq_hz = e["alarm_freq_hz"] | defs.energy.alarm_freq_hz;
        else { config.energy.alarm_freq_hz = defs.energy.alarm_freq_hz; markMissing("energy.alarm_freq_hz"); }

        if (!e["main_meter_id"].isNull()) config.energy.main_meter_id = e["main_meter_id"].as<String>();
        else { config.energy.main_meter_id = defs.energy.main_meter_id; markMissing("energy.main_meter_id"); }
    } else {
        config.energy = defs.energy;
        markMissing("energy");
    }

    // climate
    if (!doc["climate"].isNull()) {
        JsonObject c = doc["climate"];
        if (!c["enabled"].isNull()) config.climate.enabled = c["enabled"] | defs.climate.enabled;
        else { config.climate.enabled = defs.climate.enabled; markMissing("climate.enabled"); }

        if (!c["summer_mode"].isNull()) config.climate.summer_mode = c["summer_mode"] | defs.climate.summer_mode;
        else { config.climate.summer_mode = defs.climate.summer_mode; markMissing("climate.summer_mode"); }

        if (!c["global_setpoint"].isNull()) config.climate.global_setpoint = c["global_setpoint"] | defs.climate.global_setpoint;
        else { config.climate.global_setpoint = defs.climate.global_setpoint; markMissing("climate.global_setpoint"); }

        if (!c["hysteresis"].isNull()) config.climate.hysteresis = c["hysteresis"] | defs.climate.hysteresis;
        else { config.climate.hysteresis = defs.climate.hysteresis; markMissing("climate.hysteresis"); }

        if (!c["boiler_relay_id"].isNull()) config.climate.boiler_relay_id = c["boiler_relay_id"].as<String>();
        else { config.climate.boiler_relay_id = defs.climate.boiler_relay_id; markMissing("climate.boiler_relay_id"); }
    } else {
        config.climate = defs.climate;
        markMissing("climate");
    }

    // devices (optional)
    config.devices.clear();
    if (!doc["devices"].isNull()) {
        JsonObject devs = doc["devices"];
        for (JsonPair kv : devs) {
            String id = kv.key().c_str();
            JsonObject d = kv.value().as<JsonObject>();
            DeviceConfig dc;
            dc.id = id;
            dc.name = d["name"].as<String>();
            dc.room = d["room"].as<String>();
            dc.priority = d["priority"] | 0;
            String roleStr = d["role"].as<String>();
            if (roleStr == "LOAD") dc.role = DeviceRole::LOAD;
            else if (roleStr == "TRV") dc.role = DeviceRole::TRV;
            else dc.role = DeviceRole::UNKNOWN;
            dc.schedule_enabled = d["schedule_enabled"] | false;
            if (!d["schedule"].isNull()) {
                JsonArray sched = d["schedule"];
                for (JsonObject pt : sched) {
                    SchedulePoint sp;
                    sp.time = pt["time"].as<String>();
                    sp.temp = pt["temp"] | 20.0;
                    dc.schedule.push_back(sp);
                }
            }
            dc.type = DeviceType::UNKNOWN;
            config.devices[id] = dc;
        }
    }

    // If any fields were missing, log once and resave corrected file
    if (missing) {
        SysLog.error(String("Config missing fields: ") + missingFields);
        SysLog.log("Resaving config with defaults for missing fields...");
        if (!save(config)) {
            SysLog.error("Failed to resave corrected config to SD.");
        }
    }

    return true;
}

bool ConfigManager::save(const AppConfig &config) {
    // Attempt to write to SD dynamically. If SD is missing or write fails, return false.

    StaticJsonDocument<8192> doc;
    doc["wifi_ssid"] = config.wifi_ssid;
    doc["wifi_password"] = config.wifi_password;

    JsonObject e = doc["energy"].to<JsonObject>();
    e["max_power_w"] = config.energy.max_power_w;
    e["buffer_power_w"] = config.energy.buffer_power_w;
    e["cut_off_delay_s"] = config.energy.cut_off_delay_s;
    e["restore_delay_s"] = config.energy.restore_delay_s;
    e["alarm_enabled"] = config.energy.alarm_enabled;
    e["alarm_freq_hz"] = config.energy.alarm_freq_hz;
    e["main_meter_id"] = config.energy.main_meter_id;

    JsonObject c = doc["climate"].to<JsonObject>();
    c["enabled"] = config.climate.enabled;
    c["summer_mode"] = config.climate.summer_mode;
    c["global_setpoint"] = config.climate.global_setpoint;
    c["hysteresis"] = config.climate.hysteresis;
    c["boiler_relay_id"] = config.climate.boiler_relay_id;

    doc["log_level"] = config.log_level;
    doc["tz_gmt_offset_sec"] = config.tz_gmt_offset_sec;
    doc["tz_dst_offset_sec"] = config.tz_dst_offset_sec;

    JsonObject devs = doc["devices"].to<JsonObject>();
    for (const auto &kv : config.devices) {
        const DeviceConfig &dc = kv.second;
        JsonObject d = devs[dc.id.c_str()].to<JsonObject>();
        d["name"] = dc.name;
        d["room"] = dc.room;
        d["priority"] = dc.priority;
        if (dc.role == DeviceRole::LOAD) d["role"] = "LOAD";
        else if (dc.role == DeviceRole::TRV) d["role"] = "TRV";
        else d["role"] = "UNKNOWN";
        d["schedule_enabled"] = dc.schedule_enabled;
        if (!dc.schedule.empty()) {
            JsonArray sched = d["schedule"].to<JsonArray>();
            for (const auto &pt : dc.schedule) {
                JsonObject p = sched.createNestedObject();
                p["time"] = pt.time;
                p["temp"] = pt.temp;
            }
        }
    }

    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        SysLog.error("Failed to open config file for writing. SD may be absent.");
        return false;
    }
    if (serializeJson(doc, file) == 0) {
        SysLog.error("Failed to write config to SD.");
        file.close();
        return false;
    }
    file.close();
    return true;
}
