#ifndef IR_SENSOR_MANAGER_H
#define IR_SENSOR_MANAGER_H

#include <Arduino.h>

class IRSensorManager {
public:
    IRSensorManager();
    void begin(int dataPin, int powerPin);
    void powerOn();
    void powerOff();
    bool isObstacleDetected();

private:
    int sensorPin;
    int pwrPin;

    bool lastRawState;
    bool stableState;
    unsigned long lastChangeTime;
};

#endif