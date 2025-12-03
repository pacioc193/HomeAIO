#include "LogManager.h"
#include <time.h>

// Optional external time provider set by user code
static LogManager::TimeProvider g_timeProvider = nullptr;

LogManager SysLog;

void LogManager::begin() {
    // Initialize Serial (backend for SysLog)
    Serial.begin(115200);
    delay(50);

    // Setup text scroll for "Linux boot" effect
    M5.Display.setTextScroll(true);
    M5.Display.setFont(&fonts::FreeMono9pt7b);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    tryRemount();
}

void LogManager::setSdPins(int cs, int sck, int mosi, int miso) {
    _sd_cs = cs;
    _sd_sck = sck;
    _sd_mosi = mosi;
    _sd_miso = miso;
    _sdPinsSet = true;
}

bool LogManager::tryRemount() {
    // If pins are set, use hardware SPI with those pins
    bool ok = false;
    if (_sdPinsSet) {
        // Attempt to initialize SPI and mount SD
        SPI.begin(_sd_sck, _sd_miso, _sd_mosi, _sd_cs);
        ok = SD.begin(_sd_cs, SPI, 25000000);
    } else {
        // Try default SD.begin(); may work if library/platform supports it
        ok = SD.begin();
    }

    _sdMounted = ok;
    return ok;
}

// Public setter/getter implementations
void LogManager::setLogLevel(Level lvl) { _level = lvl; }
LogManager::Level LogManager::getLogLevel() const { return _level; }

void LogManager::setScreenLogging(bool enabled) {
    _screenLogging = enabled;
}

// Default timestamping: use system time (time()) if it looks initialized,
// otherwise fall back to millis().
static String make_timestamped(const String &prefix, const String &msg) {
    if (g_timeProvider) {
        String ts = g_timeProvider();
        return ts + prefix + msg;
    }
    time_t now = time(nullptr);
    char buf[64];
    if (now > 1609459200) { // If time > Jan 1, 2021 then it's probably valid
        struct tm tm;
        localtime_r(&now, &tm);
        // Format: YYYY-MM-DD HH:MM:SS
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return String(buf) + prefix + msg;
    } else {
        // Fallback to millis since boot, formatted as seconds.millis
        unsigned long ms = millis();
        unsigned long s = ms / 1000UL;
        unsigned long rem = ms % 1000UL;
        snprintf(buf, sizeof(buf), "%lu.%03lus", s, rem);
        return String(buf) + prefix + msg;
    }
}

void LogManager::setTimeProvider(TimeProvider tp) {
    g_timeProvider = tp;
}

// Helper: check if a message at `lvl` should be emitted given current _level
inline bool should_emit(LogManager::Level current, LogManager::Level msgLevel) {
    return msgLevel <= current;
}

void LogManager::log(const String &msg) {
    // INFO level
    if (!should_emit(_level, LogManager::INFO)) return;
    String fullMsg = make_timestamped(": ", msg);
    // Serial (terminal)
    Serial.println(fullMsg);
    // Screen
    if (_screenLogging) printToScreen(fullMsg, TFT_WHITE);
    // SD
    if (_sdMounted) writeToSD(fullMsg);
}

void LogManager::error(const String &msg) {
    // ERROR level
    if (!should_emit(_level, LogManager::ERROR)) return;
    String fullMsg = make_timestamped(" [ERR]: ", msg);
    Serial.println(fullMsg);
    if (_screenLogging) printToScreen(fullMsg, TFT_RED);
    if (_sdMounted) writeToSD(fullMsg);
}

void LogManager::debug(const String &msg) {
    // DEBUG level
    if (!should_emit(_level, LogManager::DEBUG)) return;
    String fullMsg = make_timestamped(" [DBG]: ", msg);
    Serial.println(fullMsg);
    if (_screenLogging) printToScreen(fullMsg, TFT_CYAN);
    if (_sdMounted) writeToSD(fullMsg);
}

void LogManager::printToScreen(const String& line, uint16_t color) {
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.println(line);
}

void LogManager::writeToSD(const String& line) {
    File f = SD.open(_logPath, FILE_APPEND);
    if (!f) {
        // SD became unavailable between checks
        _sdMounted = false;
        return;
    }
    if (f) {
        // Check size
        if (f.size() > _maxLogSize) {
            f.close();
            // Rotate or clear? For simplicity, clear if too big
            SD.remove(_logPath);
            f = SD.open(_logPath, FILE_WRITE);
            f.println("--- Log Rotated ---");
        }
        f.println(line);
        f.close();
    }
}

String LogManager::getLogContent() {
    if (!_sdMounted) return "SD Not Mounted";
    
    File f = SD.open(_logPath, FILE_READ);
    if (!f) return "Log file not found";
    
    String content = "";
    // Read last 4KB or so to avoid memory issues? 
    // Or user asked for 1MB persistent log "recallable". 
    // Reading 1MB into String will crash ESP32.
    // Let's read the last 2KB for display purposes.
    
    size_t size = f.size();
    size_t readSize = 2048;
    if (size > readSize) {
        f.seek(size - readSize);
        content += "...[truncated]...\n";
    }
    
    while (f.available()) {
        content += (char)f.read();
    }
    f.close();
    return content;
}
