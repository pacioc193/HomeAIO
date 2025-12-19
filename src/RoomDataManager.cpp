#include "RoomDataManager.h"

// Implementazione RoomData

RoomData::RoomData(String name) 
    : roomName(name), currentTemp(19.0), setpoint(20.0) {
}

void RoomData::addTemperature(float temp) {
    uint32_t currentMinute = millis() / 60000;
    
    // Rimuovi punti più vecchi di 30 minuti
    while (!history.empty() && (currentMinute - history[0].timestamp) > 30) {
        history.erase(history.begin());
    }
    
    // Aggiungi nuovo punto
    TempPoint point;
    point.timestamp = currentMinute;
    point.temperature = temp;
    history.push_back(point);
}

float RoomData::getLastTemperature() const {
    if (history.empty()) return currentTemp;
    return history.back().temperature;
}

// Implementazione RoomDataManager

RoomDataManager::RoomDataManager() 
    : rooms{
        RoomData("Camera Letto"),
        RoomData("Salotto"),
        RoomData("Cameretta"),
        RoomData("Bagno")
    },
    currentRoomIndex(0) {
}

RoomData& RoomDataManager::getRoom(int index) {
    if (index >= 0 && index < 4) {
        return rooms[index];
    }
    return rooms[0];
}

void RoomDataManager::setCurrentRoom(int index) {
    if (index >= 0 && index < 4) {
        currentRoomIndex = index;
    }
}

int RoomDataManager::getCurrentRoomIndex() const {
    return currentRoomIndex;
}

RoomData& RoomDataManager::getCurrentRoom() {
    return rooms[currentRoomIndex];
}

void RoomDataManager::generateRandomTemp(int roomIndex) {
    if (roomIndex >= 0 && roomIndex < 4) {
        float temp = 19.0 + (random(0, 50) / 100.0); // 19.0 - 19.5
        rooms[roomIndex].addTemperature(temp);
        rooms[roomIndex].currentTemp = temp;
    }
}

void RoomDataManager::generateAllRandomTemps() {
    for (int i = 0; i < 4; i++) {
        generateRandomTemp(i);
    }
}

// Definizione dell'istanza globale
RoomDataManager roomDataManager;
