#include "LogManager.h"
#include <time.h>

LogManager SysLog;

void LogManager::begin() {
    Serial.begin(115200);
    delay(50);
    M5.Display.setTextScroll(true);
    M5.Display.setFont(&fonts::FreeMono12pt7b);
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
    bool ok = false;
    if (_sdPinsSet) {
        SPI.begin(_sd_sck, _sd_miso, _sd_mosi, _sd_cs);
        ok = SD.begin(_sd_cs, SPI, 25000000);
    } else {
        ok = SD.begin();
    }
    _sdMounted = ok;
    return ok;
}

void LogManager::setLogLevel(int numericLevel) {
    if (numericLevel <= 0) _level = INFO;
    else if (numericLevel == 1) _level = ERROR;
    else _level = DEBUG;

    SysLog.log("Config Loaded, Log Level Set to " + String(int(_level)));
}

String LogManager::makeTimestamp() {
    time_t now = time(nullptr);
    char buf[32];
    if (now > 1609459200) { // After Jan 1, 2021 => probably valid NTP time
        struct tm tm;
        localtime_r(&now, &tm);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    } else {
        unsigned long ms = millis();
        snprintf(buf, sizeof(buf), "%lu.%03lus", ms / 1000UL, ms % 1000UL);
    }
    return String(buf);
}

void LogManager::emit(Level lvl, const String& prefix, const String& msg, uint16_t color) {
    // Emit only if message level <= current configured level
    if (lvl > _level) return;

    String fullMsg = makeTimestamp() + prefix + msg;
    Serial.println(fullMsg);
    if (_screenLogging) printToScreen(fullMsg, color);
    if (_sdMounted) writeToSD(fullMsg);
}

void LogManager::log(const String& msg) {
    emit(INFO, " [INF]: ", msg, TFT_WHITE);
}

void LogManager::error(const String& msg) {
    emit(ERROR, " [ERR]: ", msg, TFT_RED);
}

void LogManager::debug(const String& msg) {
    emit(DEBUG, " [DBG]: ", msg, TFT_CYAN);
}

void LogManager::printToScreen(const String& line, uint16_t color) {
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.println(line);
}

void LogManager::writeToSD(const String& line) {
    File f = SD.open(_logPath, FILE_APPEND);
    if (!f) {
        _sdMounted = false;
        return;
    }
    if (f.size() > _maxLogSize) {
        f.close();
        SD.remove(_logPath);
        f = SD.open(_logPath, FILE_WRITE);
        if (f) f.println("--- Log Rotated ---");
    }
    if (f) {
        f.println(line);
        f.close();
    }
}

String LogManager::getLogContent() {
    if (!_sdMounted) return "SD Not Mounted";

    File f = SD.open(_logPath, FILE_READ);
    if (!f) return "Log file not found";

    String content;
    size_t size = f.size();
    const size_t readSize = 2048;
    if (size > readSize) {
        f.seek(size - readSize);
        content = "...[truncated]...\n";
    }
    while (f.available()) {
        content += (char)f.read();
    }
    f.close();
    return content;
}
