#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"
#include "AppManager.h"
#include "LogManager.h"
#include "NetworkUtils.h"
#include <ArduinoOTA.h>
#include <time.h>

#define SD_SPI_CS_PIN 42
#define SD_SPI_SCK_PIN 43
#define SD_SPI_MOSI_PIN 44
#define SD_SPI_MISO_PIN 39

// WiFi SDIO pins for M5Tab5 (use before WiFi.begin)
#define WIFI_SDIO_CLK 12
#define WIFI_SDIO_CMD 13
#define WIFI_SDIO_D0 11
#define WIFI_SDIO_D1 10
#define WIFI_SDIO_D2 9
#define WIFI_SDIO_D3 8
#define WIFI_SDIO_RST 15

// App Manager Instance
AppManager appManager;

// Display size (Tab5): 1280x720
static uint16_t screenWidth;
static uint16_t screenHeight;

// Buffer LVGL in PSRAM per DMA (2 buffer full-screen per DIRECT mode)
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

lv_display_t *display = nullptr;

// --- Helper Functions ---

void initHardware()
{
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.internal_spk = true;
    cfg.internal_mic = true;

    M5.begin(cfg);
    delay(200);
}

void initDisplay()
{
    M5.Display.setColorDepth(16); // RGB565
    M5.Display.setSwapBytes(true);
    M5.Display.setBrightness(200);
    M5.Display.setRotation(3); // Landscape
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.waitDisplay(); // Wait for DMA transfer to complete
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
}

// DMA-optimized flush function for M5GFX
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Wait for any previous DMA transfer to complete
    M5.Display.waitDisplay();

    M5.Display.startWrite();
    M5.Display.pushImageDMA(area->x1, area->y1, w, h, (const uint16_t *)px_map);
    M5.Display.endWrite();

    lv_display_flush_ready(disp);
}

// Touchpad read function for LVGL 9.4
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    // M5.update() is called in loop(), so we just read the state here
    auto touch = M5.Touch.getDetail();

    if (touch.isPressed() || touch.wasPressed())
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.x;
        data->point.y = touch.y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void initLVGL()
{
    lv_init();
    lv_tick_set_cb([]() -> uint32_t {
        return millis();
    });

    size_t buf_size = screenWidth * 100 * sizeof(lv_color_t);
    buf1 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2)
    {
        SysLog.error("Failed to allocate LVGL buffers!");
        while (1)
            delay(100);
    }

    // Zero-initialize buffers to prevent artifacts from uninitialized PSRAM
    memset(buf1, 0, buf_size);
    memset(buf2, 0, buf_size);

    SysLog.log("LVGL buffers allocated: 2x " + String(buf_size / (1024.0 * 1024.0)) + " MB");

    display = lv_display_create(screenWidth, screenHeight);
    lv_display_set_buffers(display, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, my_disp_flush);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
}

// Initialize system time from the M5 RTC. Returns true if system time
// was set (RTC appeared to contain a valid date), false otherwise.
bool initSystemTimeFromRtc()
{
    auto dt = M5.Rtc.getDateTime();
    struct tm tm{};
    tm.tm_year = dt.date.year - 1900;
    tm.tm_mon = dt.date.month - 1;
    tm.tm_mday = dt.date.date;
    tm.tm_hour = dt.time.hours;
    tm.tm_min = dt.time.minutes;
    tm.tm_sec = dt.time.seconds;
    time_t t = mktime(&tm);
    // Only set system time if RTC reports a year >= 2021 to avoid epoch defaults
    if (t > 1609459200)
    {
        struct timeval now = {.tv_sec = t, .tv_usec = 0};
        settimeofday(&now, NULL);
        return true;
    }
    return false;
}

// Setup OTA updates (call only once after WiFi connected)
void setupOTA()
{
    ArduinoOTA.setHostname("HomeAIO-Tab5");

    ArduinoOTA.onStart([]()
                       {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else
            type = "filesystem";
        SysLog.log(String("OTA Start: ") + type); });

    ArduinoOTA.onEnd([]()
                     { SysLog.log("OTA End"); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
        static unsigned int lastPct = 0;
        unsigned int pct = (total > 0) ? (progress * 100) / total : 0;
        // Log every 10% to avoid spamming
        if (pct == 0 || pct - lastPct >= 10)
        {
            lastPct = pct;
            SysLog.log(String("OTA Progress: ") + String(pct) + "%");
        } });

    ArduinoOTA.onError([](ota_error_t error)
                       {
        String err = "OTA Error[" + String(error) + "]: ";
        switch (error)
        {
        case OTA_AUTH_ERROR:    err += "Auth Failed"; break;
        case OTA_BEGIN_ERROR:   err += "Begin Failed"; break;
        case OTA_CONNECT_ERROR: err += "Connect Failed"; break;
        case OTA_RECEIVE_ERROR: err += "Receive Failed"; break;
        case OTA_END_ERROR:     err += "End Failed"; break;
        default:                err += "Unknown"; break;
        }
        SysLog.error(err); });

    ArduinoOTA.begin();
    SysLog.log("OTA Ready");
}

// Helper: handle OTA setup and periodic handling (call from `loop()`)
static void handleOTA()
{
    static bool otaConfigured = false;

    if (appManager.isWifiConnected())
    {
        if (!otaConfigured)
        {
            setupOTA();
            otaConfigured = true;
        }
        ArduinoOTA.handle();
    }
    else
    {
        // Clear flag so OTA will be reinitialized after reconnect
        otaConfigured = false;
    }
}

// Helper: UI periodic updates
static void handleUIUpdates()
{
    static uint32_t last_ui_update = 0;
    uint32_t now = millis();
    if (now - last_ui_update >= 100)
    {
        last_ui_update = now;
        // Show the actual main meter consumption as a percentage of configured max_power_w
        SystemState st = appManager.getSystemState();
        float currentWatts = st.totalPower; // current consumption from shared state
        int maxWatts = appManager.getMaxPowerW();

        int pct = 0;
        if (maxWatts > 0)
        {
            float prop = currentWatts / (float)maxWatts;
            if (prop < 0.0f)
                prop = 0.0f;
            if (prop > 1.0f)
                prop = 1.0f;
            pct = (int)(prop * 100.0f + 0.5f);
        }
        lv_bar_set_value(objects.bar_actual_percentage, pct, LV_ANIM_ON);
        lv_label_set_text_fmt(objects.lbl_power_consumption, "%d", (int)currentWatts);
        lv_label_set_text_fmt(objects.lbl_battery, "%d%% / %dW", st.batteryPercent);
    }
}

// Helper: attempt SD remount periodically
static void handleSdRemount()
{
    static uint32_t last_sd_remount = 0;
    uint32_t now = millis();

    if (!SysLog.isSdMounted())
    {
        if (now - last_sd_remount >= 10000)
        { // 10 seconds
            last_sd_remount = now;
            SysLog.log("Attempting SD remount...");
            if (SysLog.tryRemount())
            {
                SysLog.log("SD remounted successfully");
            }
            else
            {
                SysLog.log("SD remount attempt failed");
            }
        }
    }
}

// Helper: read battery info via getSystemState() and log it
static void logBatteryStateFromSystem()
{
    static uint32_t last_batt_log = 0;
    uint32_t now = millis();
    if (now - last_batt_log < 10000)
        return;
    last_batt_log = now;

    SystemState st = appManager.getSystemState();
    int pct = st.batteryPercent;
    int mv = st.batteryMv;
    int ma = st.batteryMa;
    bool charging = st.batteryCharging;

    String s = String("Battery: ") + String(pct) + "%  " + String(mv) + "mV  " + String(ma) + "mA " + (charging ? "(charging)" : "(discharging)");
    SysLog.log(s);
}

void setup()
{
    // 1. Init Hardware (M5)
    initHardware();
    // 2. Init System Time (RTC -> system clock)
    // Initialize system time from RTC so SysLog can use real timestamps.
    // If RTC holds a reasonable date, apply it to the system clock.
    bool rtcInitialized = initSystemTimeFromRtc();

    // 3. Init Logging (Screen + SD)
    // Provide SPI pins before begin so LogManager can initialize SD properly
    SysLog.setSdPins(SD_SPI_CS_PIN, SD_SPI_SCK_PIN, SD_SPI_MOSI_PIN, SD_SPI_MISO_PIN);
    SysLog.begin();
    // Configure WiFi SDIO pins for M5Tab5 (like SD pin setup)
    setWifiPins(WIFI_SDIO_CLK, WIFI_SDIO_CMD, WIFI_SDIO_D0, WIFI_SDIO_D1, WIFI_SDIO_D2, WIFI_SDIO_D3, WIFI_SDIO_RST);
    // Now that SysLog is active we can report RTC initialization result
    if (rtcInitialized)
        SysLog.log("System time initialized from RTC");
    else
        SysLog.log("System time not initialized from RTC; using millis()");

    SysLog.log("=== HomeAIO Boot Sequence ===");
    SysLog.log("Hardware Initialized");

    if (SysLog.isSdMounted())
        SysLog.log("SD Card Mounted");
    else
        SysLog.error("SD Card Mount Failed");

    // 4. Init Display Settings
    initDisplay();
    SysLog.log("Display Configured: " + String(screenWidth) + "x" + String(screenHeight));

    // 5. Start App Logic
    SysLog.log("Starting AppManager...");
    appManager.begin();
    SysLog.log("AppManager Started");

    // 6. Init LVGL
    SysLog.log("Initializing LVGL...");
    initLVGL();
    SysLog.log("LVGL Initialized");

    // 7. Finalize Boot
    SysLog.log("System Ready. Starting UI...");

    // 8. Start App Task (after UI init to avoid glitches)
    SysLog.log("Starting App Task...");
    appManager.start();

    // Waiting user to touch the screen to start UI
    SysLog.log("Waiting for user touch to start UI...");
    while (!M5.Touch.getDetail().isPressed())
    {
        M5.update();
        delay(10);
    }
    // Disable screen logging before UI takes over
    SysLog.setScreenLogging(false);

    // store all shelly devices to sd card
    appManager.saveShellyDevicesToSD("/shelly_discovered.json");

    // 9. Start UI
    ui_init();
}

void loop()
{
    M5.update();
    lv_timer_handler();
    ui_tick();

    handleOTA();
    handleUIUpdates();
    handleSdRemount();
    logBatteryStateFromSystem();

    delay(5);
}
