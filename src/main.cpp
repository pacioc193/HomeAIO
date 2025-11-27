#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"

// Dimensioni display Tab5: 1280x720
static const uint16_t screenWidth = 1280;
static const uint16_t screenHeight = 720;

// Buffer LVGL in PSRAM per DMA (2 buffer full-screen per DIRECT mode)
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

lv_display_t *display = nullptr;

// Flag per gestione DMA asincrona
volatile bool dma_transfer_done = true;

// Funzione flush ottimizzata con DMA di M5GFX
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    volatile uint32_t w = (area->x2 - area->x1 + 1);
    volatile uint32_t h = (area->y2 - area->y1 + 1);    

    M5.Display.startWrite();
    M5.Display.pushImageDMA(area->x1, area->y1, w, h, (const uint16_t *)px_map);
    M5.Display.endWrite();
    
    // Segnala a LVGL che può continuare (non bloccante)
    lv_display_flush_ready(disp);
}

// Funzione touchpad per LVGL 9.4
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    M5.update(); // Aggiorna touch
    auto touch = M5.Touch.getDetail();
    
    if (touch.isPressed() || touch.wasPressed()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.x;
        data->point.y = touch.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// UI Demo
void create_demo_ui() {
    // Sfondo gradiente
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    // Titolo
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "M5Stack Tab5");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);
    
    // Sottotitolo
    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "LVGL 9.4 + DMA Acceleration");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x00d9ff), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 80);
    
    // Contenitore centrale
    lv_obj_t *cont = lv_obj_create(screen);
    lv_obj_set_size(cont, 600, 400);
    lv_obj_center(cont);
    lv_obj_set_style_radius(cont, 20, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_shadow_width(cont, 40, 0);
    lv_obj_set_style_shadow_color(cont, lv_color_black(), 0);
    
    // Bottone
    lv_obj_t *btn = lv_button_create(cont);
    lv_obj_set_size(btn, 250, 80);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -50);
    lv_obj_set_style_radius(btn, 15, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xe94560), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xff6b85), LV_STATE_PRESSED);
    
    // Callback bottone con performance test
    static uint32_t click_count = 0;
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        click_count++;
        Serial.printf("Button clicked! Count: %d\n", click_count);
        M5.Speaker.tone(1000, 100);
        
        // Misura performance
        uint32_t start = millis();
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(NULL);
        uint32_t elapsed = millis() - start;
        Serial.printf("Frame time: %d ms (%.1f FPS)\n", elapsed, 1000.0f / elapsed);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "TEST DMA");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_20, 0);
    lv_obj_center(btn_label);
    
    // Slider
    lv_obj_t *slider = lv_slider_create(cont);
    lv_obj_set_width(slider, 450);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 60);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x533483), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x00d9ff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xe94560), LV_PART_KNOB);
    
    // Label slider value
    lv_obj_t *slider_label = lv_label_create(cont);
    lv_label_set_text(slider_label, "50%");
    lv_obj_align(slider_label, LV_ALIGN_CENTER, 0, 110);
    lv_obj_set_style_text_color(slider_label, lv_color_white(), 0);
    
    lv_obj_add_event_cb(slider, [](lv_event_t *e) {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
        int32_t value = lv_slider_get_value(slider);
        lv_label_set_text_fmt(label, "%d%%", value);
        Serial.printf("Slider: %d%%\n", value);
    }, LV_EVENT_VALUE_CHANGED, slider_label);
    
    // Info display
    lv_obj_t *info = lv_label_create(screen);
    lv_label_set_text_fmt(info, "ESP32-P4 @ 400MHz | Display: %dx%d | DMA: ON", 
                          screenWidth, screenHeight);
    lv_obj_set_style_text_color(info, lv_color_hex(0x00d9ff), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void setup() {
    // Inizializza M5Unified
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.internal_spk = true;
    cfg.internal_mic = true;
    
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== M5Stack Tab5 + LVGL 9.4 + DMA ===");
    Serial.printf("Display: %dx%d\n", M5.Display.width(), M5.Display.height());
    
    // Configura M5GFX per prestazioni ottimali
    M5.Display.setColorDepth(16); // RGB565 per compatibilità LVGL
    M5.Display.setSwapBytes(true); // Swap RGB565 byte order (BGR <-> RGB)
    M5.Display.setBrightness(200);
    M5.Display.setRotation(1); // Landscape
    M5.Display.fillScreen(TFT_BLACK);
    
    // Inizializza LVGL
    lv_init();
    // Questo permette a LVGL di sapere quanto tempo è passato
    lv_tick_set_cb([]() -> uint32_t {
        return millis();
    });
    
    size_t buf_size = screenWidth * 100 * sizeof(lv_color_t); // full-screen
    buf1 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);
    
    if (!buf1 || !buf2) {
        Serial.println("ERROR: Failed to allocate LVGL buffers in PSRAM!");
        while(1) delay(100);
    }
    
    Serial.printf("LVGL buffers allocated: 2x %.2f MB in PSRAM\n", 
                  buf_size / (1024.0 * 1024.0));
    
    // Crea display LVGL 9.4
    display = lv_display_create(screenWidth, screenHeight);
    
    lv_display_set_buffers(display, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Imposta callback flush con DMA
    lv_display_set_flush_cb(display, my_disp_flush);
    
    // Crea input device per touch
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    
    // Crea UI
    //create_demo_ui();
    create_screens();
    ui_init();
    
    Serial.println("Setup complete! DMA acceleration enabled.");
    Serial.println("Touch the 'TEST DMA' button to measure performance.\n");
}

void loop() {
    lv_timer_handler(); // Handler LVGL - gestisce animazioni e refresh
    ui_tick();
    // Aggiorna la power_bar dal main ogni 500 ms con valore random 0..100
    static uint32_t last_power_update = 0;
    uint32_t now = lv_tick_get();
    if (now - last_power_update >= 500) {
        last_power_update = now;
       //if (objects.power_bar) {
       //    int val = lv_rand(0, 100);
       //    lv_bar_set_value(objects.power_bar, val, LV_ANIM_ON);
       //}
    }
    delay(5);
}