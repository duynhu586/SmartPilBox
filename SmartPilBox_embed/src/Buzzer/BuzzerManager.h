// BuzzerManager.h - Bỏ buzzerChannel
#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>

class BuzzerManager {
public:
    BuzzerManager();
    void begin(int pin);
    void startBeeping();
    void stopBeeping();
    void update();
private:
    int buzzerPin;
    // ❌ Xóa: int buzzerChannel
    bool isBeeping;
    bool buzzerState;
    unsigned long lastToggleTime;
};

#endif