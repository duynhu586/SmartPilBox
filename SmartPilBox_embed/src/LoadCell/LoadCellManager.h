#ifndef LOAD_CELL_MANAGER_H
#define LOAD_CELL_MANAGER_H

#include <HX711.h>

class LoadCellManager {
public:
    LoadCellManager();
    void begin(int doutPin, int sckPin);
    float getWeight();
    void tare();

    void powerDown();
    void powerUp();
private:
    HX711 scale;
    long tareOffset;
};

#endif