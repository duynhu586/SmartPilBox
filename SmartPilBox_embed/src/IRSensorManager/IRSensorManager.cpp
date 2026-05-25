#include "IRSensorManager.h"

IRSensorManager::IRSensorManager() : sensorPin(-1), pwrPin(-1) {}

void IRSensorManager::begin(int dataPin, int powerPin) {
    sensorPin = dataPin;
    pwrPin = powerPin;
    
    pinMode(sensorPin, INPUT_PULLUP);
    pinMode(pwrPin, OUTPUT);
    
    // Mặc định lúc khởi động: TẮT NGUỒN cảm biến để tiết kiệm điện
    powerOff();
}

void IRSensorManager::powerOn() {
    digitalWrite(pwrPin, HIGH); // Cấp điện 3.3V cho module IR hoạt động
    delay(10); // Đợi 10ms để linh kiện trên module ổn định dòng điện
}

void IRSensorManager::powerOff() {
    digitalWrite(pwrPin, LOW);  // Ngắt điện hoàn toàn, module IR tắt hẳn
}

bool IRSensorManager::isObstacleDetected() {
    // Chỉ đọc khi chân nguồn đang BẬT
    if (digitalRead(pwrPin) == LOW) return false; 
    return (digitalRead(sensorPin) == LOW);
}