#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"
#include "AppManager.h"
#include "LogManager.h"
#include "NetworkUtils.h"
#include "RoomDataManager.h"
#include "ThermostatChart.h"
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

// Buffer LVGL in PSRAM per DMA (2 buffer per double buffering)
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

// LVGL 8 structures
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

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

// DMA-optimized flush function for LVGL 8 + M5GFX
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Wait for any previous DMA transfer to complete
    M5.Display.waitDisplay();

    M5.Display.startWrite();
    M5.Display.pushImageDMA(area->x1, area->y1, w, h, (const uint16_t *)&color_p->full);
    M5.Display.endWrite();

    lv_disp_flush_ready(disp);
}

// Touchpad read function for LVGL 8
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    auto touch = M5.Touch.getDetail();

    if (touch.isPressed() || touch.wasPressed())
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch.x;
        data->point.y = touch.y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

void initLVGL()
{
    SysLog.log("LVGL Init: Starting lv_init()...");
    lv_init();
    SysLog.log("LVGL Init: lv_init() completed");

    // OPZIONE 1: Full screen buffer (no banding, ma usa ~3.5MB PSRAM)
    //size_t buf_size = screenWidth * screenHeight;
    
    // OPZIONE 2: Half screen buffer (compromesso tra velocità e memoria ~1.75MB)
    // size_t buf_size = screenWidth * screenHeight / 2;
    
    // OPZIONE 3: 1/3 screen (veloce, minimo banding visibile ~1.2MB)
    size_t buf_size = screenWidth * screenHeight / 3;
    
    // OPZIONE 4: Attuale - 100 linee (banding visibile ma memoria bassa)
    // size_t buf_size = screenWidth * 100;
    
    SysLog.log("LVGL Init: Calculating buffer size: " + String(buf_size));
    
    buf1 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2)
    {
        SysLog.error("Failed to allocate LVGL buffers!");
        while (1)
            delay(100);
    }

    // Zero-initialize buffers to prevent artifacts from uninitialized PSRAM
    memset(buf1, 0, buf_size * sizeof(lv_color_t));
    memset(buf2, 0, buf_size * sizeof(lv_color_t));

    SysLog.log("LVGL buffers allocated: 2x " + String((buf_size * sizeof(lv_color_t)) / (1024.0 * 1024.0)) + " MB");

    // LVGL 8: lv_disp_draw_buf_init
    SysLog.log("LVGL Init: Initializing draw buffer...");
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);
    SysLog.log("LVGL Init: Draw buffer initialized");

    // LVGL 8: lv_disp_drv_init
    SysLog.log("LVGL Init: Initializing display driver...");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    
    // Ottimizzazioni per performance massima
    disp_drv.full_refresh = 0;      // Partial refresh per velocità
    disp_drv.direct_mode = 0;       // Buffered mode (migliore con DMA)
    disp_drv.antialiasing = 0;      // Disabilita antialiasing per velocità
    
    // IMPORTANTE: Se usi buffer >= 1/10 dello schermo, puoi ridurre il refresh period
    // per diminuire il carico CPU mantenendo fluidità visiva
    
    SysLog.log("LVGL Init: Registering display driver...");
    lv_disp_drv_register(&disp_drv);
    SysLog.log("LVGL Init: Display driver registered");

    // LVGL 8: lv_indev_drv_init
    SysLog.log("LVGL Init: Initializing input device...");
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    SysLog.log("LVGL Init: Input device registered - COMPLETE");
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
        float currentKw = st.totalPower / 1000.f; // current consumption from shared state
        float maxKw = appManager.getMaxPowerW() / 1000.f;   // configured max power in kW
        int pct = 0;
        
        currentKw = lv_rand(0, (int)(maxKw * 1000)) / 1000.0f; // TESTING: random value up to maxKw
        
        if (maxKw > 0)
        {
            float prop = currentKw / maxKw;
            if (prop < 0.0f)
                prop = 0.0f;
            if (prop > 1.0f)
                prop = 1.0f;
            pct = (int)(prop * 100.0f + 0.5f);
        }
        char *temp = new char[5];
        sprintf(temp, "%.2f", currentKw);
        lv_bar_set_value(objects.bar_power, pct, LV_ANIM_ON);
        lv_label_set_text(objects.lbl_power_val, temp);
        delete[] temp;
    }
}

// Helper: aggiorna dati temperatura ogni 30 secondi
static void handleTemperatureUpdates()
{
    static uint32_t last_temp_update = 0;
    uint32_t now = millis();
    if (now - last_temp_update >= 1000) // 1 second
    {
        last_temp_update = now;
        
        // Genera temperature casuali per tutte le stanze
        roomDataManager.generateAllRandomTemps();
        
        // Se siamo nella schermata termostato, aggiorna il grafico
        static lv_obj_t* lastScreen = nullptr;
        lv_obj_t* currentScreen = lv_scr_act();
        
        if (currentScreen == objects.thermostat)
        {
            // Aggiorna il grafico per la stanza corrente
            thermostatChart.updateForRoom(roomDataManager.getCurrentRoomIndex());
        }
        
        lastScreen = currentScreen;
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
    bool rtcInitialized = initSystemTimeFromRtc();

    // 3. Init Logging (Screen + SD)
    SysLog.setSdPins(SD_SPI_CS_PIN, SD_SPI_SCK_PIN, SD_SPI_MOSI_PIN, SD_SPI_MISO_PIN);
    SysLog.begin();
    
    // Configure WiFi SDIO pins for M5Tab5
    setWifiPins(WIFI_SDIO_CLK, WIFI_SDIO_CMD, WIFI_SDIO_D0, WIFI_SDIO_D1, WIFI_SDIO_D2, WIFI_SDIO_D3, WIFI_SDIO_RST);
    
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
    SysLog.log("=== Calling ui_init() ===");
    ui_init();
    SysLog.log("=== ui_init() completed ===");
}

void loop()
{
    M5.update();
    lv_timer_handler();  // LVGL 8 usa lv_timer_handler invece di lv_task_handler
    ui_tick();

    handleOTA();
    handleUIUpdates();
    handleTemperatureUpdates();  // Aggiorna temperature ogni 30 secondi
    handleSdRemount();
    logBatteryStateFromSystem();

    // Delay ottimizzato:
    // - Con buffer grandi (1/3+ screen): delay(10-20) riduce CPU senza perdere fluidità
    // - Con buffer piccoli (100 linee): delay(5) necessario per refresh rapido
    delay(5);  // Aumentato da 5 a 10 per ridurre CPU load
}