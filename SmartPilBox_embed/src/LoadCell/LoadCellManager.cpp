#include "LoadCellManager.h"
#include "Config.h"
#include <Arduino.h>

LoadCellManager::LoadCellManager() {}

void LoadCellManager::begin(int doutPin, int sckPin) {
    scale.begin(doutPin, sckPin);
    scale.set_scale(HX711_CALIBRATION_FACTOR);
    scale.tare();
    tareOffset = scale.get_offset();
    Serial.println("[LoadCellManager] Initialized HX711 with calibration factor:" );
    Serial.printf("  factor=%.2f offset=%ld\n", HX711_CALIBRATION_FACTOR, tareOffset);
}

float LoadCellManager::getWeight() {
    // Wait up to 200ms for HX711 to be ready
    unsigned long start = millis();
    while (!scale.is_ready() && (millis() - start) < 200) {
        delay(5);
    }
    if (!scale.is_ready()) {
        Serial.println("[LoadCellManager] WARNING: HX711 not ready (timeout)");
        return 0.0;
    }

    // Take multiple samples and average to reduce noise
    const int samples = 10;
    float units = scale.get_units(samples);
    return units;
}

void LoadCellManager::tare() {
    if (scale.is_ready()) {
        scale.tare();
    }
}

void LoadCellManager::powerDown() {
    scale.power_down(); // Thư viện kéo chân SCK lên mức HIGH để ép HX711 vào Sleep Mode
    Serial.println("[LoadCellManager] HX711 chip entered Low-Power Down state.");
}

void LoadCellManager::powerUp() {
    scale.power_up(); // Thư viện thả chân SCK về LOW để kích hoạt lại chip
    // Small delay to allow HX711 to settle after waking
    delay(100);
    scale.set_offset(tareOffset);
    Serial.println("[LoadCellManager] HX711 chip awakened.");
}