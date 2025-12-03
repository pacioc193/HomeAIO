#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

class LogManager {
public:
    enum Level { ERROR = 0, INFO = 1, DEBUG = 2 };

    void begin();
    void log(const String& msg);
    void error(const String& msg);
    void debug(const String& msg);
    String getLogContent();
    void setScreenLogging(bool enabled);
    void setLogLevel(Level lvl);
    Level getLogLevel() const;
    // Optional: set an explicit time provider (not required). If not set,
    // the logger will use system time (time()) when available, otherwise millis().
    using TimeProvider = String(*)();
    void setTimeProvider(TimeProvider tp);
    bool isSdMounted() const { return _sdMounted; }
    // Configure SD SPI pins so LogManager can attempt remounts
    void setSdPins(int cs, int sck, int mosi, int miso);
    // Attempt to (re)mount SD using stored pins (if set). Returns true if mounted.
    bool tryRemount();

private:
    void writeToSD(const String& line);
    void printToScreen(const String& line, uint16_t color);
    
    bool _sdMounted = false;
    bool _screenLogging = true;
    bool _sdPinsSet = false;
    int _sd_cs = -1;
    int _sd_sck = -1;
    int _sd_mosi = -1;
    int _sd_miso = -1;
    const char* _logPath = "/system.log";
    const size_t _maxLogSize = 1024 * 1024; // 1MB
    Level _level = INFO;
};

extern LogManager SysLog;
