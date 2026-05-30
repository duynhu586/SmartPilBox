#include "PillBoxController.h"
#include <Arduino.h>

// Khởi tạo mặc định lấy giá trị gốc từ Config.h làm mốc dự phòng ban đầu
PillBoxController::PillBoxController() : 
    currentState(IDLE), 
    beforeWeight(0.0), 
    afterWeight(0.0), 
    stateTimer(0), 
    scheduleTriggeredToday(false),
    lastCheckedMinute(-1),
    alarmHour(SCHEDULE_HOUR),     // Giờ ban đầu
    alarmMinute(SCHEDULE_MINUTE)  // Phút ban đầu
{
    schedules[0] = {8, 0, false};
    schedules[1] = {12, 0, false};
    schedules[2] = {20, 0, false};  

}

void PillBoxController::begin() {
    Serial.begin(115200);
    Serial.println("\n=====================================");
    Serial.println("  ESP32 SMART PILL BOX INITIALIZED   ");
    Serial.println("=====================================");

    // ================================================================
    // BẮT ĐẦU CHÈN CODE CẤU HÌNH TIẾT KIỆM PIN TỰ ĐỘNG TẠI ĐÂY
    // ================================================================
    
    // ================================================================

    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);

    if (!rtcManager.begin(RTC_SDA_PIN, RTC_SCL_PIN)) {
        Serial.println("[ERROR] RTC DS1307 module missing!");
    } else {
        Serial.println("[OK] RTC DS1307 Module Online.");
        if (rtcManager.lostPower()) { 
            Serial.println("[WARN] RTC lost power! Syncing time from compile timestamp...");
            rtcManager.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    }

    // [QUẢN LÝ NGUỒN]: Khởi tạo, khóa chốt ngay rồi gỡ xung Servo (Triệt tiêu dòng ngậm điện tĩnh)
    servoManager.begin(SERVO_PIN);
    delay(1500);
     // Đợi ngắn cho servo quay hết hành trình
    servoManager.detach(); 
    Serial.println("[OK] Servo Actuator Locked & Detached.");

    buzzerManager.begin(BUZZER_PIN);
    Serial.println("[OK] Non-blocking Audio Buzz Node Linked.");

    // [QUẢN LÝ NGUỒN]: Đọc tải trọng ban đầu xong ép chip cân HX711 đi ngủ lập tức
    loadCellManager.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    delay(500);

    // warm up samples
    for(int i = 0; i < 5; i++) {
        loadCellManager.getWeight();
        delay(50);
    }

    float startupWeight = loadCellManager.getWeight();

    Serial.printf(
        "[CELL] Startup weight reading: %.2f g\n",
        startupWeight
    );

    loadCellManager.powerDown(); 
    Serial.println("[OK] HX711 Load Cell Driver Initialized & Powered Down.");

    irSensorManager.begin(IR_SENSOR_PIN, IR_POWER_PIN);
    irSensorManager.powerOff(); // Mặc định tắt ngay lúc khởi động để tiết kiệm pin
    Serial.println("[OK] Safety IR Limit System Configured (Default: OFF).");

    // Khởi chạy mạng MQTT
    mqttManager.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP, MQTT_PORT); 
    
    // Đăng ký nhận lịch hẹn từ Server đổ về hộp thuốc thông qua cơ chế Callback
// Đăng ký nhận lịch hẹn từ Server đổ về hộp thuốc thông qua cơ chế Callback mới
    mqttManager.setOnTimeUpdate([](int slot, int h, int m) {
        extern PillBoxController controller; 
        // Gọi hàm setAlarmTime đã được nâng cấp (nhận thêm tham số slot)
        controller.setAlarmTime(slot, h, m); 
    });

    Serial.printf("[INFO] Monitor alarm set for %02d:%02d daily.\n", alarmHour, alarmMinute);
    Serial.println("System Core Mode: IDLE (Power Saving Management Active)");
}

// Tìm đến hàm setAlarmTime cũ và thay thế toàn bộ bằng hàm này:
void PillBoxController::setAlarmTime(int slot, int hour, int minute) {
    if (slot >= 0 && slot < MAX_SCHEDULES) {
        schedules[slot].hour = hour;
        schedules[slot].minute = minute;
        schedules[slot].completedToday = false; // Có lịch mới thì reset trạng thái lịch đó
        Serial.printf("[FSM] Lịch uống thuốc Slot [%d] cập nhật động thành: %02d:%02d\n", slot, hour, minute);
    } else {
        Serial.printf("[FSM][ERROR] Slot %d vượt quá giới hạn hệ thống!\n", slot);
    }
}

void PillBoxController::resetSchedulesForNewDay() {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedules[i].completedToday = false;
    }
    Serial.println("[INFO] New day detected. All medication schedules reset to uncompleted.");
}

const char* PillBoxController::getStateString(PillBoxState state) {
    switch (state) {
        case IDLE:            return "IDLE";
        case ALARM_RINGING:   return "ALARM_RINGING";
        case BOX_OPEN:        return "BOX_OPEN";
        case WAIT_FOR_RETURN: return "WAIT_FOR_RETURN";
        case VERIFY_MEDICINE: return "VERIFY_MEDICINE";
        case BOX_CLOSING:     return "BOX_CLOSING";
        case COMPLETED:       return "COMPLETED";
        default:              return "UNKNOWN";
    }
}

void PillBoxController::update() {
    mqttManager.update();
    buzzerManager.update();
    DateTime now = rtcManager.getCurrentTime();
    // Thêm log này vào update() đầu hàm

    if (now.hour() == 0 && now.minute() == 0 && now.second() == 0) {
        resetSchedulesForNewDay();
        delay(1000); // Tránh trùng lặp log trong cùng 1 giây
    }

    PillBoxState previousState = currentState;

    switch (currentState) {
        case IDLE: // <-- SỬA LỖI: Thêm nhãn Case chuẩn xác ở đây
            for (int i = 0; i < MAX_SCHEDULES; i++) {
                if (!schedules[i].completedToday && 
                    now.hour() == schedules[i].hour && 
                    now.minute() == schedules[i].minute) {
                    
                    activeScheduleIndex = i; // Ghi nhận lịch hiện tại đang xử lý
                    
                    Serial.printf("\n--- ALARM EVENT DETECTED FOR SLOT [%d] ---\n", activeScheduleIndex);
                    Serial.printf("[RTC] Target Time: %02d:%02d\n", schedules[i].hour, schedules[i].minute);

                    // Kích hoạt cơ cấu Servo giải phóng chốt
                    servoManager.attach();
                    delay(500); 
                    servoManager.open();
                    delay(1500); 
                    servoManager.detach();

                    // Đánh thức cảm biến lực cân
                    loadCellManager.powerUp();
                    delay(500); 
                    for(int i = 0; i < 5; i++) {
                        loadCellManager.getWeight();
                        delay(20);
                    }

                    // Giờ mới lấy giá trị thật
                    beforeWeight = loadCellManager.getWeight();
                    Serial.printf("[CELL] Pre-ingestion weight base: %.2f g\n", beforeWeight);
                    loadCellManager.powerDown();

                    // Kích hoạt báo động chuông và cảm biến IR
                    buzzerManager.startBeeping();
                    delay(100); // Đợi buzzer khởi động trước khi bật cảm biến IR để tránh nhiễu
                    irSensorManager.powerOn();
                    delay(100); // Đợi cảm biến IR khởi động trước khi tiếp nhận tín hiệu để tránh nhiễu
                    currentState = ALARM_RINGING;
                    stateTimer = millis();
                    break; // Thoát vòng lặp quét lịch trình
                }
            }

            if (currentState == IDLE) {
                // 1. In thời gian hiện tại từ chip RTC
                Serial.printf("\n=========================================\n");
                Serial.printf("[RTC NOW] Thời gian thực: %02d:%02d:%02d\n", 
                              now.hour(), now.minute(), now.second());
                Serial.println("-----------------------------------------");
                
                // 2. Duyệt qua mảng và in danh sách lịch trình hiện tại
                Serial.println("[SCHEDULES] Danh sách lịch hẹn trong máy:");
                for (int i = 0; i < MAX_SCHEDULES; i++) {
                    Serial.printf("  -> Slot [%d]: %02d:%02d | Trạng thái: %s\n", 
                                  i, 
                                  schedules[i].hour, 
                                  schedules[i].minute, 
                                  schedules[i].completedToday ? "ĐÃ UỐNG ĐỦ" : "CHƯA UỐNG");
                }
                Serial.printf("=========================================\n");

                // Giảm tần suất quét (ví dụ nâng lên 3000ms - 3 giây in một lần để bạn kịp đọc Log)
                delay(3000); 
            }
            break;

        case ALARM_RINGING:
           if (irSensorManager.isObstacleDetected() == false) {
                Serial.println("[FSM] Lid opened detected via IR or Timeout. Moving to BOX_OPEN.");
                buzzerManager.stopBeeping();
                currentState = BOX_OPEN;
            }
            break;

        case BOX_OPEN:
            if (!boxOpenInitialized) {
                irSensorManager.powerOff();
                delay(100); // Đợi cảm biến IR tắt hẳn để tránh nhiễu khi cân
                loadCellManager.powerUp();
                delay(500);
                boxOpenInitialized = true;
            }  

            {   
                float currentWeight = loadCellManager.getWeight();
                Serial.printf(
                "[DEBUG] before=%.2f current=%.2f delta=%.2f\n",
                beforeWeight,
                currentWeight,
                beforeWeight - currentWeight
            );
                if (beforeWeight - currentWeight >= CONTAINER_REMOVED_THRESHOLD) {
                    Serial.println("[LOG] Container removed from platform scale.");
                    currentState = WAIT_FOR_RETURN;
                }
                delay(500); // Đợi 500ms trước khi đọc cân tiếp theo để giảm thiểu nhiễu và tiết kiệm pin
            }
            break;

        case WAIT_FOR_RETURN:
            {
                float currentWeight = loadCellManager.getWeight();
                if (currentWeight >= (beforeWeight - CONTAINER_REMOVED_THRESHOLD)) {
                    Serial.println("[LOG] Mass load detected on platform. Analyzing weight stabilization...");
                    stateTimer = millis();
                    currentState = VERIFY_MEDICINE;
                }
                delay(500); // Đợi 500ms trước khi đọc cân tiếp theo để giảm thiểu nhiễu và tiết kiệm pin
            }
            break;

        case VERIFY_MEDICINE:
            if (millis() - stateTimer >= WEIGHT_STABILIZATION_TIME) { 
                afterWeight = loadCellManager.getWeight();
                float weightDelta = beforeWeight - afterWeight;

                if (weightDelta >= MEDICINE_WEIGHT_THRESHOLD) {
                    Serial.println("[OK] Dose match validated.");
                    
                    // CHỐT HẠ: Đánh dấu ĐÃ UỐNG cho riêng slot lịch trình này
                    if (activeScheduleIndex != -1) {
                        schedules[activeScheduleIndex].completedToday = true;
                        Serial.printf("[FSM] Slot [%d] marked as COMPLETED for today.\n", activeScheduleIndex);
                    }
                    
                    String statusMsg = "COMPLETED|SLOT_" + String(activeScheduleIndex) + "|" + rtcManager.getTimeString(); 
                    mqttManager.publishStatus(statusMsg.c_str()); 

                    loadCellManager.powerDown(); 
                    delay(100); 

                    irSensorManager.powerOn(); 
                    currentState = BOX_CLOSING;
                } else {
                    Serial.println("[WARN] Failure: Container replaced without safe consumption variance.");
                    currentState = BOX_OPEN;
                }
            }
            break;

        case BOX_CLOSING:
            if (irSensorManager.isObstacleDetected()) {
                if (!lidCloseTimerRunning) {
                    lidClosedStartTime = millis();
                    lidCloseTimerRunning = true;
                    Serial.println("[LOG] Lid detected closed. Starting confirmation timer...");
                }

                if (millis() - lidClosedStartTime >= LID_CLOSE_CONFIRMATION_TIME) {
                    Serial.println("[OK] Lid remained closed long enough. Locking box.");

                    irSensorManager.powerOff(); // Tắt cảm biến IR ngay khi xác nhận nắp đã đóng để tiết kiệm pin
                    delay(100); // Đợi cảm biến IR tắt hẳn để tránh nhiễu khi điều khiển servo
                    servoManager.attach();
                    delay(200); // Let PWM timer stabilize before any write
                    servoManager.lock();
                    delay(500); // đợi servo quay xong
                    servoManager.detach();

                    lidCloseTimerRunning = false;
                    currentState = COMPLETED;
                }

            } else {
                lidCloseTimerRunning = false;
            }
            break;

        case COMPLETED:
            boxOpenInitialized = false; // Reset cờ flag phụ trợ cho chu kỳ tiếp theo
            activeScheduleIndex = -1;   // Trả định danh lịch kích hoạt về mặc định
            
            Serial.println("[SYSTEM] Operations loop concluded. Returning to IDLE for next schedule.");
            
            // Trả ngay về IDLE, vòng lặp IDLE sẽ tự bỏ qua lịch vừa hoàn thành (vì completedToday = true)
            currentState = IDLE; 
            break;
    }

    if (currentState != previousState) {
        Serial.printf("[FSM TRANSITION] %s -> %s\n", getStateString(previousState), getStateString(currentState));
    }
}