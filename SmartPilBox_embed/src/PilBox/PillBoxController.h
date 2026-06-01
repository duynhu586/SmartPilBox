#ifndef PILL_BOX_CONTROLLER_H
#define PILL_BOX_CONTROLLER_H

#include "Config.h"
#include "../RTC/RTCManager.h"
#include "../Servo/ServoManager.h"
#include "../Buzzer/BuzzerManager.h"
#include "../LoadCell/LoadCellManager.h"
#include "../IRSensorManager/IRSensorManager.h"
#include "../MQTT/MQTTManager.h" 
#include "../Oled/OledManager.h"

struct MedicationSchedule {
    int hour;
    int minute;
    bool completedToday;
};

// Số lượng lịch trình tối đa hỗ trợ trong ngày
const int MAX_SCHEDULES = 3;    

class PillBoxController {
public:
    PillBoxController();
    void begin();
    void update();
    const char* getStateString(PillBoxState state);
    
    // Đã sửa lại thứ tự tham số cho khớp chuẩn với file .cpp: (slot, hour, minute)
    void setAlarmTime(int slot, int hour, int minute);
    
    bool boxOpenInitialized = false;

    // Khai báo 2 hàm xử lý lệnh MQTT (Để public hoặc private đều được, để public cho thông thoáng)
    void handleRtcTimeSync(int hour, int minute, int second);
    void handleLoadCellTare();

private:
    PillBoxState currentState;
    RTCManager rtcManager;
    ServoManager servoManager;
    BuzzerManager buzzerManager;
    LoadCellManager loadCellManager;
    IRSensorManager irSensorManager;
    MQTTManager mqttManager; 
    OledManager oled;

    float beforeWeight;
    float afterWeight;
    unsigned long stateTimer;
    bool scheduleTriggeredToday;
    int lastCheckedMinute;

    int alarmHour;
    int alarmMinute;

    MedicationSchedule schedules[MAX_SCHEDULES];
    int activeScheduleIndex;

    bool lidCloseTimerRunning;
    unsigned long lidClosedStartTime;

    // --- CÁC HÀM HELPER ĐÃ GOM NHÓM LOGIC (THÊM MỚI VÀO ĐÂY) ---
    void triggerAlarmSequence(int scheduleIndex);
    void initializeBoxOpenScale();
    void processWeightVerification();
    void handleLidClosingRoutine();
    String getNextScheduleStr();
    
    void resetSchedulesForNewDay();
};

#endif