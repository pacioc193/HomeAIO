#ifndef ROOM_DATA_MANAGER_H
#define ROOM_DATA_MANAGER_H

#include <Arduino.h>
#include <vector>

// Struttura per un singolo punto temperatura-tempo
struct TempPoint {
    uint32_t timestamp;  // minuti dall'avvio
    float temperature;
};

// Classe per gestire i dati di una singola stanza
class RoomData {
public:
    String roomName;
    float currentTemp;
    float setpoint;
    std::vector<TempPoint> history;  // Ultimi 30 minuti
    
    RoomData(String name);
    void addTemperature(float temp);
    float getLastTemperature() const;
};

// Manager globale per tutte le stanze
class RoomDataManager {
private:
    RoomData rooms[4];
    int currentRoomIndex;
    
public:
    RoomDataManager();
    
    RoomData& getRoom(int index);
    void setCurrentRoom(int index);
    int getCurrentRoomIndex() const;
    RoomData& getCurrentRoom();
    
    // Genera temperatura casuale per test
    void generateRandomTemp(int roomIndex);
    
    // Genera temperature per tutte le stanze
    void generateAllRandomTemps();
};

// Istanza globale
extern RoomDataManager roomDataManager;

#endif // ROOM_DATA_MANAGER_H
