#include "ServoManager.h"
#include "Config.h"
#include <Arduino.h>

ServoManager::ServoManager() : servoPin(-1) {}

void ServoManager::begin(int pin) {
    servoPin = pin;
    myServo.attach(servoPin);
    lock();
}

void ServoManager::lock() {
    myServo.write(SERVO_LOCKED_ANGLE);
}

void ServoManager::open() {
    myServo.write(SERVO_OPEN_ANGLE);
}