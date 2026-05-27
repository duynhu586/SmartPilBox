#include "ServoManager.h"
#include "Config.h"
#include <Arduino.h>

ServoManager::ServoManager() : servoPin(-1) {}

void ServoManager::begin(int pin) {
    servoPin = pin;
    myServo.attach(servoPin);
    currentAngle = SERVO_LOCKED_ANGLE;
    lock();
}

void ServoManager::lock() {
    currentAngle = SERVO_LOCKED_ANGLE;
    myServo.write(currentAngle);
}

void ServoManager::open() {
    currentAngle = SERVO_OPEN_ANGLE;
    myServo.write(currentAngle);
}

void ServoManager::attach() {
    if (!myServo.attached() && servoPin != -1) {
        myServo.attach(servoPin);
        myServo.write(currentAngle);
        Serial.println("[ServoManager] Servo ATTACHED - PWM Signal Active.");
    }
}

void ServoManager::detach() {
    if (myServo.attached()) {
        myServo.detach();
        Serial.println("[ServoManager] Servo DETACHED - Power Saving Active.");
    }
}