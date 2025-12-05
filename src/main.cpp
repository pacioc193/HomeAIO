#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>
//#include "ui/ui.h"
//#include "ui/screens.h"

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

// --- INIZIO CODICE BENCHMARK (Nomi variabili corretti) ---

// Variabili per l'animazione (Rinominate per evitare conflitti con la UI)
#define OBJ_COUNT 50
static lv_obj_t * ppa_test_objects[OBJ_COUNT]; // Aggiunto 'static' e rinominato
static int8_t ppa_dir_x[OBJ_COUNT];           // Aggiunto 'static'
static int8_t ppa_dir_y[OBJ_COUNT];           // Aggiunto 'static'

// Timer per muovere gli oggetti
void anim_timer_cb(lv_timer_t * timer) {
    for(int i = 0; i < OBJ_COUNT; i++) {
        lv_obj_t * obj = ppa_test_objects[i];
        
        // Calcola nuova posizione
        int32_t x = lv_obj_get_x(obj) + ppa_dir_x[i] * 5; 
        int32_t y = lv_obj_get_y(obj) + ppa_dir_y[i] * 5;
        
        // Rimbalzo sui bordi (1280x720)
        if(x < 0 || x > (1280 - 100)) ppa_dir_x[i] *= -1;
        if(y < 0 || y > (720 - 100))  ppa_dir_y[i] *= -1;
        
        lv_obj_set_pos(obj, x, y);
    }
}

void create_ppa_benchmark() {
    lv_obj_t * scr = lv_screen_active();
    
    // Passa 'display' (la variabile globale) invece di 'scr'
    // Se 'display' non è accessibile qui, usa NULL (display di default)
    lv_obj_t * perf_label = lv_sysmon_create(NULL); 
    lv_obj_align(perf_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // Crea oggetti semi-trasparenti
    for(int i = 0; i < OBJ_COUNT; i++) {
        ppa_test_objects[i] = lv_obj_create(scr);
        lv_obj_set_size(ppa_test_objects[i], 100, 100);
        
        // Posizione iniziale casuale
        lv_obj_set_pos(ppa_test_objects[i], lv_rand(0, 1180), lv_rand(0, 620));
        
        // Colore casuale
        lv_obj_set_style_bg_color(ppa_test_objects[i], lv_color_hex(lv_rand(0, 0xFFFFFF)), 0);
        
        // OPACITÀ 50%: Il test critico per il PPA
        lv_obj_set_style_bg_opa(ppa_test_objects[i], LV_OPA_50, 0); 
        
        // Rimuovi decorazioni pesanti
        lv_obj_set_style_border_width(ppa_test_objects[i], 0, 0);
        lv_obj_set_style_shadow_width(ppa_test_objects[i], 0, 0);
        lv_obj_remove_flag(ppa_test_objects[i], LV_OBJ_FLAG_SCROLLABLE);

        // Direzione casuale
        ppa_dir_x[i] = (lv_rand(0, 1) ? 1 : -1);
        ppa_dir_y[i] = (lv_rand(0, 1) ? 1 : -1);
    }

    // Etichetta informativa
    lv_obj_t * label = lv_label_create(scr);
    lv_label_set_text(label, "PPA STRESS TEST\n50 Transparent Objects");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Timer animazione
    lv_timer_create(anim_timer_cb, 16, NULL);
}
// --- FINE CODICE BENCHMARK ---

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
    buf1 = (lv_color_t *)heap_caps_aligned_alloc(64, buf_size, MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_aligned_alloc(64, buf_size, MALLOC_CAP_SPIRAM);
    
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
    //create_screens();
    //ui_init();

    create_ppa_benchmark();
    
    Serial.println("Setup complete! DMA acceleration enabled.");
    Serial.println("Touch the 'TEST DMA' button to measure performance.\n");
}

void loop() {
    lv_timer_handler(); // Handler LVGL - gestisce animazioni e refresh
    //ui_tick();
    //// Aggiorna la power_bar dal main ogni 500 ms con valore random 0..100
    //static uint32_t last_power_update = 0;
    //uint32_t now = lv_tick_get();
    //if (now - last_power_update >= 500) {
    //    last_power_update = now;
    //   if (objects.bar_actual_percentage) {
    //       int val = lv_rand(0, 100);
    //       lv_bar_set_value(objects.bar_actual_percentage, val, LV_ANIM_ON);
    //   }
    //}
    delay(5);
}