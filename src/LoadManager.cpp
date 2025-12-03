#include "LoadManager.h"

LoadManager::LoadManager(ShellyManager* mgr, EnergyConfig* cfg) : shellyManager(mgr), config(cfg) {}

void LoadManager::update() {
    unsigned long now = millis();
    if (now - lastCheck < 250) return; // Check every 250ms
    lastCheck = now;
    
    float totalPower = shellyManager->getTotalPower(config->main_meter_id);
    
    if (totalPower > config->max_power_w) {
        if (!isOverloaded) {
            isOverloaded = true;
            overloadStartTime = now;
        }
        
        // Alarm
        if (config->alarm_enabled) {
            M5.Speaker.tone(config->alarm_freq_hz, 500); // 500ms beep
        }
        
        // Check delay
        if (now - overloadStartTime > (config->cut_off_delay_s * 1000)) {
            // Shed a device
            ShellyDevice* candidate = nullptr;
            int maxPrio = -1;
            
            auto devices = shellyManager->getAllDevices();
            for (auto* dev : devices) {
                if (dev->getRole() == DeviceRole::LOAD && dev->getIsOn() && dev->getPriority() > 0) {
                    if (dev->getPriority() > maxPrio) {
                        maxPrio = dev->getPriority();
                        candidate = dev;
                    }
                }
            }
            
            if (candidate) {
                candidate->turnOff();
                shedDevices.push_back({candidate->getId(), now});
                // Reset timer to avoid shedding everything at once? 
                // Or keep shedding if still overloaded?
                // Prompt says "If duration > cut_off_delay: Turn OFF active device".
                // It implies one action. If power stays high, we need to shed another.
                // So I should reset start time or have a "last shed time".
                // I'll reset overloadStartTime to give time for power to drop.
                overloadStartTime = now; 
            }
        }
    } else {
        isOverloaded = false;
        
        // Restore logic
        if (totalPower < (config->max_power_w - config->buffer_power_w)) {
            if (!shedDevices.empty()) {
                // Check if we can restore
                // We need to ensure we don't oscillate.
                // We restore one device, wait for restore_delay.
                // The prompt says "Restore devices after restore_delay".
                // This could mean "Wait X seconds of safe power, then restore".
                
                // I'll use the timestamp of the last shed device.
                // If (now - lastShedTime) > restore_delay, restore.
                // But we might have multiple shed devices.
                
                // Let's look at the last shed device.
                ShedDevice& last = shedDevices.back();
                if (now - last.shedTime > (config->restore_delay_s * 1000)) {
                    // Restore
                    ShellyDevice* dev = shellyManager->getDevice(last.id);
                    if (dev) {
                        dev->turnOn();
                    }
                    shedDevices.pop_back();
                    // Reset time for next restore?
                    // If we restore one, power goes up. We need to wait again.
                    // So we should update the shedTime of the *next* device in list to now?
                    // Or just rely on the loop.
                    // If we restore, power increases. Next loop, we check power again.
                    // If still safe, we check next device.
                    // But we need a delay between restores.
                    // So I should enforce a delay since LAST ACTION.
                    // I'll use `overloadStartTime` as `lastActionTime`? No.
                    // I'll update the shedTime of the remaining devices to now, so they wait another cycle?
                    if (!shedDevices.empty()) {
                        shedDevices.back().shedTime = now;
                    }
                }
            }
        }
    }
}
