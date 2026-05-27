#include "LoadCellManager.h"
#include "Config.h"
#include <Arduino.h>

LoadCellManager::LoadCellManager() {}

void LoadCellManager::begin(int doutPin, int sckPin) {
    scale.begin(doutPin, sckPin);
    scale.set_scale(HX711_CALIBRATION_FACTOR);
    scale.tare();
    tareOffset = scale.get_offset();
}

float LoadCellManager::getWeight() {
    if (scale.is_ready()) {
        return scale.get_units(5); // Mean smoothing factor over 5 simulation sweeps
    }
    return 0.0;
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
    scale.set_offset(tareOffset); 
    Serial.println("[LoadCellManager] HX711 chip awakened.");
}