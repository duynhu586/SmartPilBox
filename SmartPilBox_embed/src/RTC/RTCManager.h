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
    void setAlarmTime(int hour, int minute); 
    bool isItMedicineTime();
    void adjust(const DateTime& dt);
    bool lostPower();

private:
    RTC_DS3231 rtc; 
    int alarmHour = -1;   
    int alarmMinute = -1;
};

#endif
