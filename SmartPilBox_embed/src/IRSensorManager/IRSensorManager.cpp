#include "IRSensorManager.h"
#include <Config.h>

IRSensorManager::IRSensorManager()
    : sensorPin(-1),
      pwrPin(-1),
      lastRawState(false),
      stableState(false),
      lastChangeTime(0)
{
}

void IRSensorManager::begin(int dataPin, int powerPin) {
    sensorPin = dataPin;
    pwrPin = powerPin;
    
    pinMode(sensorPin, INPUT_PULLUP);
    pinMode(pwrPin, OUTPUT);
    
    // Mặc định lúc khởi động: TẮT NGUỒN cảm biến để tiết kiệm điện
    powerOff();
}

void IRSensorManager::powerOn() {
    digitalWrite(pwrPin, HIGH); 
    
    // TĂNG LÊN 150ms: Đảm bảo module IR (mắt phát + mắt thu + IC LM393) 
    // có đủ thời gian khởi động ổn định dòng điện hoàn toàn
    delay(150); 
    
    // Đọc ép một lần để đồng bộ trạng thái thật ngay sau khi bật nguồn,
    // tránh việc bộ lọc Debounce lấy giá trị rác cũ.
    lastRawState = (digitalRead(sensorPin) == LOW);
    stableState = lastRawState;
    lastChangeTime = millis();
}

bool IRSensorManager::isObstacleDetected() {
    // Nếu cảm biến đang tắt nguồn, mặc định báo không có vật cản
    if (digitalRead(pwrPin) == LOW)
        return false;

    // Đọc trạng thái thô: LOW = Có vật cản (Nắp đang đóng)
    bool rawState = (digitalRead(sensorPin) == LOW);

    // Nếu trạng thái thay đổi, reset lại cấu trúc thời gian Debounce
    if (rawState != lastRawState) {
        lastRawState = rawState;
        lastChangeTime = millis();
    }

    // Chỉ khi trạng thái ổn định suốt một khoảng thời gian IR_DEBOUNCE_TIME mới cập nhật
    if (millis() - lastChangeTime >= IR_DEBOUNCE_TIME) {
        stableState = rawState;
    }

    return stableState;
}

void IRSensorManager::powerOff() {
    digitalWrite(pwrPin, LOW);  // Ngắt điện hoàn toàn, module IR tắt hẳn
}

