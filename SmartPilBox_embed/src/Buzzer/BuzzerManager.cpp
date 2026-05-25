#include "BuzzerManager.h"
#include "Config.h"

BuzzerManager::BuzzerManager() : buzzerPin(-1), isBeeping(false), buzzerState(false), lastToggleTime(0) {}

void BuzzerManager::begin(int pin) {
    buzzerPin = pin;
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
}

void BuzzerManager::startBeeping() {
    if (!isBeeping) {
        isBeeping = true;
        lastToggleTime = millis();
        buzzerState = true;
        digitalWrite(buzzerPin, HIGH);
    }
}

void BuzzerManager::stopBeeping() {
    isBeeping = false;
    buzzerState = false;
    digitalWrite(buzzerPin, LOW);
}

void BuzzerManager::update() {
    if (!isBeeping) return;

    unsigned long currentTime = millis();
    if (currentTime - lastToggleTime >= BUZZER_BEEP_INTERVAL) {
        buzzerState = !buzzerState;
        digitalWrite(buzzerPin, buzzerState ? HIGH : LOW);
        lastToggleTime = currentTime;
    }
}