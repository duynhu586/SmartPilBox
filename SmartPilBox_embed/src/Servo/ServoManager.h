#ifndef SERVO_MANAGER_H
#define SERVO_MANAGER_H

#include <ESP32Servo.h>

class ServoManager {
public:
    ServoManager();
    void begin(int pin);
    void lock();
    void open();
private:
    Servo myServo;
    int servoPin;
};

#endif