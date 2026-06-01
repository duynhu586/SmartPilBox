#include "RTCManager.h"

RTCManager::RTCManager() {}

bool RTCManager::begin(int sdaPin, int sclPin) {
    // Kích hoạt cổng I2C số 1 (Wire1) chạy trên chân 22, 23 với tốc độ ổn định 100kHz
    Wire1.begin(sdaPin, sclPin, 100000);
    
    // MẤU CHỐT: Ép thư viện RTC liên kết vào địa chỉ con trỏ &Wire1 để không tranh chấp với OLED
    if (!rtc.begin(&Wire1)) {
        return false;
    }
    return true;
}

DateTime RTCManager::getCurrentTime() {
    return rtc.now();
}

String RTCManager::getTimeString() {
    DateTime now = rtc.now();
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", 
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    return String(buffer);
}

void RTCManager::setAlarmTime(int hour, int minute) {
    alarmHour = hour;
    alarmMinute = minute;
}

bool RTCManager::isItMedicineTime() {
    if (alarmHour == -1 || alarmMinute == -1) return false;
    DateTime now = rtc.now();
    if (now.hour() == alarmHour && now.minute() == alarmMinute) {
        return true; 
    }
    return false;
}

void RTCManager::adjust(const DateTime& dt) {
    rtc.adjust(dt);
}

bool RTCManager::lostPower() {
    // DS3231 bắt buộc phải sử dụng lostPower(), không có isrunning()
    return rtc.lostPower(); 
}
