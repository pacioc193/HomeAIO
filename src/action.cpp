#include "ui/actions.h"
#include "ui/ui.h"
#include "ui/screens.h"
#include "LogManager.h"
#include "RoomDataManager.h"
#include "ThermostatChart.h"

void action_show_thermostat(lv_event_t * e)
{
    int roomIndex = -1;
    
    switch((intptr_t)e->user_data)
    {
        case 0://Camera da letto
            roomIndex = 0;
            break;
        case 1://Salotto
            roomIndex = 1;
            break;
        case 2://Cameretta
            roomIndex = 2;
            break;
        case 3://Bagno
            roomIndex = 3;
            break;
        default:
            SysLog.log("Action: Show Thermostat (unknown room)");
            return;
    }
    
    // Imposta la stanza corrente
    roomDataManager.setCurrentRoom(roomIndex);
    
    // Carica lo screen del termostato
    loadScreen(SCREEN_ID_THERMOSTAT);
    
    // Aggiorna il grafico con i dati della stanza selezionata
    thermostatChart.updateForRoom(roomIndex);
}

void action_goto_main(lv_event_t * e)
{
    loadScreen(SCREEN_ID_MAIN);
}