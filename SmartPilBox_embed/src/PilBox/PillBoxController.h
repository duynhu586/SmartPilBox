#ifndef PILL_BOX_CONTROLLER_H
#define PILL_BOX_CONTROLLER_H

#include "Config.h"
#include "../RTC/RTCManager.h"
#include "../Servo/ServoManager.h"
#include "../Buzzer/BuzzerManager.h"
#include "../LoadCell/LoadCellManager.h"
#include "../IRSensorManager/IRSensorManager.h"
#include "../MQTT/MQTTManager.h" // <-- 1. THÊM DÒNG NÀY (sửa lại đường dẫn cho đúng thư mục của bạn)

// Error tracking structure
struct ErrorLog {
    int servoErrors = 0;
    int loadcellErrors = 0;
    int irSensorErrors = 0;
    int mqttErrors = 0;
    int rtcErrors = 0;
    unsigned long lastErrorTime = 0;
};

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
    void setAlarmTime(int hour, int minute, int scheduleIndex);
    void logError(const char* component, const char* errorMsg);
    void printErrorSummary();
    // In your .h, add:
    bool boxOpenInitialized = false;
    ErrorLog errorLog;

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

    MedicationSchedule schedules[MAX_SCHEDULES];
    int activeScheduleIndex;

    bool lidCloseTimerRunning;
    unsigned long lidClosedStartTime;

    void resetSchedulesForNewDay();
    void resetErrorCounters();
};

#endif  