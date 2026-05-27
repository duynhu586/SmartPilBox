// BuzzerManager.cpp - Viết lại cho passive buzzer
#include "BuzzerManager.h"
#include "Config.h"

BuzzerManager::BuzzerManager()
    : buzzerPin(-1),
      isBeeping(false),
      buzzerState(false),
      lastToggleTime(0) {}

void BuzzerManager::begin(int pin) {
    buzzerPin = pin;
    pinMode(buzzerPin, OUTPUT);
    noTone(buzzerPin); // Đảm bảo tắt ngay khi khởi động
}

void BuzzerManager::startBeeping() {
    if (!isBeeping) {
        isBeeping = true;
        buzzerState = false;
        lastToggleTime = millis();
    }
}

void BuzzerManager::stopBeeping() {
    isBeeping = false;
    buzzerState = false;
    noTone(buzzerPin); // ← Dùng noTone() thay vì ledcWriteTone()
}

void BuzzerManager::update() {
    if (!isBeeping) return;

    unsigned long currentTime = millis();
    if (currentTime - lastToggleTime >= BUZZER_BEEP_INTERVAL) {
        buzzerState = !buzzerState;
        if (buzzerState) {
            tone(buzzerPin, BUZZER_FREQUENCY); // ← Dùng tone() thay vì ledcWriteTone()
        } else {
            noTone(buzzerPin);
        }
        lastToggleTime = currentTime;
    }
}