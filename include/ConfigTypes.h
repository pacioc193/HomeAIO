#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include <string>

// Enums
enum class DeviceRole {
    LOAD,
    TRV,
    UNKNOWN
};

enum class DeviceType {
    SHELLY_GEN1,
    SHELLY_GEN2,
    SHELLY_BLU_TRV,
    UNKNOWN
};

// Configuration Structures
struct SchedulePoint {
    String time; // "HH:MM"
    float temp;
};

struct DeviceConfig {
    String id; // MAC_Channel or similar
    String name;
    String room;
    int priority; // 0-100
    DeviceRole role;
    bool schedule_enabled;
    std::vector<SchedulePoint> schedule;
    
    // Runtime/Discovery info (not necessarily in JSON, but useful here)
    String ip;
    DeviceType type;
};

struct EnergyConfig {
    int max_power_w;
    int buffer_power_w;
    int cut_off_delay_s;
    int restore_delay_s;
    bool alarm_enabled;
    int alarm_freq_hz;
    String main_meter_id;
};

struct ClimateConfig {
    bool enabled;
    bool summer_mode;
    float global_setpoint;
    float hysteresis;
    String boiler_relay_id;
};

struct AppConfig {
    String wifi_ssid;
    String wifi_password;
    EnergyConfig energy;
    ClimateConfig climate;
    std::map<String, DeviceConfig> devices; // Keyed by ID
    // Log level as numeric: 0=INFO, 1=ERROR, 2=DEBUG
    int log_level = 0; // default INFO
    // Timezone configuration in seconds: GMT offset and daylight offset (seconds)
    // Example for CET with DST: tz_gmt_offset_sec = 3600, tz_dst_offset_sec = 3600
    int tz_gmt_offset_sec = 3600;
    int tz_dst_offset_sec = 3600;
};

// UI / Shared State Structures
struct DeviceState {
    String id;
    String name;
    String room;
    bool isOn;
    float power; // Watts
    float currentTemp; // For TRVs
    float targetTemp;  // For TRVs
    float valvePos;    // For TRVs
    bool isOnline;
    DeviceRole role;
};

struct SystemState {
    float totalPower;
    bool alarmActive;
    bool boilerOn;
    // Battery info for the host device (M5)
    int batteryPercent; // 0-100% (or -1 if unavailable)
    int batteryMv;      // millivolts (or -1)
    int batteryMa;      // milliamps (positive=charging, negative=discharging, or 0)
    bool batteryCharging;
    std::vector<DeviceState> devices;
};
