#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Wire.h>
#include <RTClib.h>

class RTCManager {
public:
    RTCManager();
    bool begin(int sdaPin, int sclPin);
    DateTime getCurrentTime();
    String getTimeString();
    
    // Hàm mới: Để cập nhật giờ từ MQTT Server gửi về
    void setAlarmTime(int hour, int minute); 
    
    // Hàm check giờ uống thuốc (kiểm tra theo biến động)
    bool isItMedicineTime();

private:
    RTC_DS3231 rtc; // Hoặc RTC_DS1307 tùy mạch của bạn
    int alarmHour = -1;   // Mặc định -1 là chưa cài giờ
    int alarmMinute = -1;
};

#endif