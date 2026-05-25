#include "LoadCellManager.h"
#include "Config.h"
#include <Arduino.h>

LoadCellManager::LoadCellManager() {}

void LoadCellManager::begin(int doutPin, int sckPin) {
    scale.begin(doutPin, sckPin);
    scale.set_scale(HX711_CALIBRATION_FACTOR);
    scale.tare();
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