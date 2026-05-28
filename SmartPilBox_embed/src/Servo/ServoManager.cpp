#include "ServoManager.h"
#include "Config.h"
#include <Arduino.h>

ServoManager::ServoManager() : servoPin(-1) {}

void ServoManager::begin(int pin) {
    servoPin = pin;
    // Khởi tạo trạng thái ban đầu một cách an toàn
    attach(); 
    lock();
}

void ServoManager::open() {
    currentAngle = SERVO_OPEN_ANGLE;
    if (myServo.attached()) {
        // Thay vì ghi thẳng 1 góc, cho nó quét từ từ từ góc cũ sang góc mới
        int startAngle = SERVO_LOCKED_ANGLE; 
        for (int angle = startAngle; angle <= SERVO_OPEN_ANGLE; angle += 5) {
            myServo.write(angle);
            delay(20); // Mỗi 20ms chỉ nhích 5 độ để giảm dòng khởi động
        }
    }
}

void ServoManager::lock() {
    currentAngle = SERVO_LOCKED_ANGLE;
    if (myServo.attached()) {
        int startAngle = SERVO_OPEN_ANGLE;
        for (int angle = startAngle; angle >= SERVO_LOCKED_ANGLE; angle -= 5) {
            myServo.write(angle);
            delay(20);
        }
    }
}

void ServoManager::attach() {
    if (!myServo.attached() && servoPin != -1) {
        myServo.attach(servoPin);
        delay(200); // Let PWM timer stabilize before any write
        Serial.println("[ServoManager] Servo ATTACHED - PWM Signal Active.");
    }
}

void ServoManager::detach() {
    if (myServo.attached()) {
        myServo.detach();
        Serial.println("[ServoManager] Servo DETACHED - Power Saving Active.");
    }
}

