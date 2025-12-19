#ifndef THERMOSTAT_CHART_H
#define THERMOSTAT_CHART_H

#include <lvgl.h>

class ThermostatChart {
private:
    lv_obj_t* chartObj;
    
public:
    ThermostatChart();
    
    // Inizializza il grafico del termostato con assi e configurazioni
    void init();
    
    // Aggiorna il grafico per una specifica stanza
    void updateForRoom(int roomIndex);
};

// Istanza globale
extern ThermostatChart thermostatChart;

#endif // THERMOSTAT_CHART_H
