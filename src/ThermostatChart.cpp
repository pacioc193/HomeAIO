#include "ThermostatChart.h"
#include "ui/screens.h"
#include "RoomDataManager.h"
#include <stdio.h>

// Buffer per formattare i tick
static char tickBuffer[16];

// Callback per formattare i tick degli assi
static void format_chart_ticks(lv_event_t* e) {
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(e);
    
    if(dsc->part != LV_PART_TICKS) return;
    
    // IMPORTANTE: Modifica solo se è un Major Tick (che ha già un testo allocato).
    // Se dsc->text è NULL, è un Minor Tick e non deve avere etichetta.
    if(dsc->text == NULL) return;
    
    if(dsc->id == LV_CHART_AXIS_PRIMARY_Y) {
        // Asse Y: temperatura in °C (es. 195 -> 19.5°)
        float temp = dsc->value / 10.0f;
        lv_snprintf(tickBuffer, sizeof(tickBuffer), "%.1f°", temp);
        dsc->text = tickBuffer;
    }
    else if(dsc->id == LV_CHART_AXIS_PRIMARY_X) {
        // Asse X: minuti (0m, 5m, 10m...)
        lv_snprintf(tickBuffer, sizeof(tickBuffer), "%dm", (int)dsc->value);
        dsc->text = tickBuffer;
    }
}

ThermostatChart::ThermostatChart() : chartObj(nullptr) {
}

void ThermostatChart::init() {
    chartObj = objects.temperature_chart;    
    if (!chartObj) return;
    
    // 1. Configurazione base del grafico
    lv_chart_set_type(chartObj, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chartObj, 30);  // Finestra di 30 minuti
    
    // 2. Configurazione Range Asse Y (18.0°C - 21.0°C)
    // I valori sono in decimi di grado (180 - 210)
    lv_chart_set_range(chartObj, LV_CHART_AXIS_PRIMARY_Y, 180, 210);
    
    // 3. Configurazione Griglia
    // 5 linee orizzontali (divisioni Y), 6 linee verticali (divisioni X)
    lv_chart_set_div_line_count(chartObj, 5, 6);
    
    // 4. Configurazione Tick Assi
    // Asse Y: 7 major ticks (18.0, 18.5, 19.0, 19.5, 20.0, 20.5, 21.0)
    lv_chart_set_axis_tick(chartObj, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 7, 2, true, 50);
    
    // Asse X: 7 major ticks (0, 5, 10, 15, 20, 25, 30)
    lv_chart_set_axis_tick(chartObj, LV_CHART_AXIS_PRIMARY_X, 10, 5, 7, 2, true, 40);
    
    // 5. Configurazione Serie Dati
    lv_chart_series_t* series = lv_chart_get_series_next(chartObj, NULL);
    if (!series) {
        series = lv_chart_add_series(chartObj, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    }
    
    // 6. Stile
    lv_obj_set_style_line_width(chartObj, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chartObj, 0, LV_PART_INDICATOR); // Nascondi i punti, mostra solo la linea
    
    lv_obj_set_style_text_color(chartObj, lv_color_hex(0xFFFFFF), LV_PART_TICKS);
    lv_obj_set_style_line_color(chartObj, lv_color_hex(0x808080), LV_PART_MAIN); // Colore griglia
    lv_obj_set_style_border_width(chartObj, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(chartObj, lv_color_hex(0x404040), LV_PART_MAIN);
    
    // 7. Eventi
    // Rimuovi eventuali callback precedenti per evitare duplicati se init viene richiamata
    lv_obj_remove_event_cb(chartObj, format_chart_ticks);
    lv_obj_add_event_cb(chartObj, format_chart_ticks, LV_EVENT_DRAW_PART_BEGIN, NULL);
}

void ThermostatChart::updateForRoom(int roomIndex) {
    if (!chartObj) {
        init();
        if (!chartObj) return;
    }
    
    RoomData& room = roomDataManager.getRoom(roomIndex);
    
    lv_chart_series_t* series = lv_chart_get_series_next(chartObj, NULL);
    if (!series) return;
    
    // Pulisci il grafico
    lv_chart_set_all_value(chartObj, series, LV_CHART_POINT_NONE);
    
    // Logica: Mostra sempre gli ultimi 30 minuti disponibili
    // Se abbiamo meno di 30 punti, li mostriamo tutti partendo da sinistra
    // Se ne abbiamo più di 30, prendiamo solo gli ultimi 30
    
    const auto& history = room.history;
    size_t totalPoints = history.size();
    size_t pointsToShow = (totalPoints > 30) ? 30 : totalPoints;
    size_t startIndex = (totalPoints > 30) ? (totalPoints - 30) : 0;
    
    for (size_t i = 0; i < pointsToShow; i++) {
        float temp = history[startIndex + i].temperature;
        int16_t val = (int16_t)(temp * 10.0f);
        lv_chart_set_next_value(chartObj, series, val);
    }
    
    lv_chart_refresh(chartObj);
}

// Istanza globale
ThermostatChart thermostatChart;
