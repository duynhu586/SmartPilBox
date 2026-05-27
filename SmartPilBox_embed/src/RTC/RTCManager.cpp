#include "RTCManager.h"

RTCManager::RTCManager() {}

bool RTCManager::begin(int sdaPin, int sclPin) {
    Wire.begin(sdaPin, sclPin);
    if (!rtc.begin()) {
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

// Cập nhật giờ uống thuốc động từ MQTT
void RTCManager::setAlarmTime(int hour, int minute) {
    alarmHour = hour;
    alarmMinute = minute;
}

bool RTCManager::isItMedicineTime() {
    // Nếu chưa được cài giờ từ điện thoại thì không báo động
    if (alarmHour == -1 || alarmMinute == -1) return false;

    DateTime now = rtc.now();
    
    // So khớp với giờ động được cài qua MQTT
    if (now.hour() == alarmHour && now.minute() == alarmMinute ) {
        return true; 
    }
    return false;
}

void RTCManager::adjust(const DateTime& dt) {
    rtc.adjust(dt);
}

bool RTCManager::lostPower() {
    return rtc.lostPower(); // DS1307 không có lostPower(), dùng isrunning() thay thế
}