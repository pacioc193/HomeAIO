#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

class LogManager {
public:
    // Log levels matching config values: 0=INFO, 1=ERROR, 2=DEBUG
    enum Level { INFO = 0, ERROR = 1, DEBUG = 2 };

    void begin();

    // Logging methods
    void log(const String& msg);    // INFO level
    void error(const String& msg);  // ERROR level
    void debug(const String& msg);  // DEBUG level

    // Configuration
    void setLogLevel(Level lvl) { _level = lvl; }
    void setLogLevel(int numericLevel); // Converts 0/1/2 to enum
    Level getLogLevel() const { return _level; }

    void setScreenLogging(bool enabled) { _screenLogging = enabled; }

    // SD card management
    bool isSdMounted() const { return _sdMounted; }
    void setSdPins(int cs, int sck, int mosi, int miso);
    bool tryRemount();
    String getLogContent();

private:
    void emit(Level lvl, const String& prefix, const String& msg, uint16_t color);
    void writeToSD(const String& line);
    void printToScreen(const String& line, uint16_t color);
    String makeTimestamp();

    Level _level = INFO;
    bool _sdMounted = false;
    bool _screenLogging = true;
    bool _sdPinsSet = false;
    int _sd_cs = -1;
    int _sd_sck = -1;
    int _sd_mosi = -1;
    int _sd_miso = -1;
    const char* _logPath = "/system.log";
    const size_t _maxLogSize = 1024 * 1024; // 1MB
};

extern LogManager SysLog;
