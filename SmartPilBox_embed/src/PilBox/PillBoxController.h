#ifndef PILL_BOX_CONTROLLER_H
#define PILL_BOX_CONTROLLER_H

#include "Config.h"
#include "../RTC/RTCManager.h"
#include "../Servo/ServoManager.h"
#include "../Buzzer/BuzzerManager.h"
#include "../LoadCell/LoadCellManager.h"
#include "../IRSensorManager/IRSensorManager.h"
#include "../MQTT/MQTTManager.h" // <-- 1. THÊM DÒNG NÀY (sửa lại đường dẫn cho đúng thư mục của bạn)

class PillBoxController {
public:
    PillBoxController();
    void begin();
    void update();
    const char* getStateString(PillBoxState state);
    void setAlarmTime(int hour, int minute);
    // In your .h, add:
    bool boxOpenInitialized = false;

private:
    PillBoxState currentState;
    RTCManager rtcManager;
    ServoManager servoManager;
    BuzzerManager buzzerManager;
    LoadCellManager loadCellManager;
    IRSensorManager irSensorManager;
    MQTTManager mqttManager; // <-- 2. THÊM DÒNG NÀY ĐỂ ĐỊNH DANH BIẾN

    float beforeWeight;
    float afterWeight;
    unsigned long stateTimer;
    bool scheduleTriggeredToday;
    int lastCheckedMinute;

    int alarmHour;
    int alarmMinute;
};

#endif