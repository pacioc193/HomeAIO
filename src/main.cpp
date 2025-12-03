#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"
#include "AppManager.h"
#include "LogManager.h"
#include <time.h>

#define SD_SPI_CS_PIN 42
#define SD_SPI_SCK_PIN 43
#define SD_SPI_MOSI_PIN 44
#define SD_SPI_MISO_PIN 39

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
    M5.Display.setRotation(1); // Landscape
    M5.Display.fillScreen(TFT_BLACK);
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
}

// DMA-optimized flush function for M5GFX
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    volatile uint32_t w = (area->x2 - area->x1 + 1);
    volatile uint32_t h = (area->y2 - area->y1 + 1);

    M5.Display.startWrite();
    M5.Display.pushImageDMA(area->x1, area->y1, w, h, (const uint16_t *)px_map);
    M5.Display.endWrite();

    lv_display_flush_ready(disp);
}

// Touchpad read function for LVGL 9.4
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    M5.update();
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
    lv_tick_set_cb([]() -> uint32_t
                   { return millis(); });

    size_t buf_size = screenWidth * 100 * sizeof(lv_color_t);
    buf1 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2)
    {
        SysLog.error("Failed to allocate LVGL buffers!");
        while (1)
            delay(100);
    }

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

    // Waiting user to touch the screen to start UI
    SysLog.log("Waiting for user touch to start UI...");
    while (!M5.Touch.getDetail().isPressed())
    {
        M5.update();
        delay(10);
    }
    // Disable screen logging before UI takes over
    SysLog.setScreenLogging(false);

    // 8. Start UI
    ui_init();
}

void loop()
{
    lv_timer_handler();
    M5.update();

    static uint32_t last_ui_update = 0;
    uint32_t now = millis();
    if (now - last_ui_update >= 100)
    {
        last_ui_update = now;
        // SystemState state = appManager.getSystemState();
        // Update UI widgets...
    }

    // Periodic SD remount attempt: try to remount every 10s if SD is not mounted
    static uint32_t last_sd_remount = 0;
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

    delay(5);
}
