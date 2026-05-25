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
{}

void PillBoxController::begin() {
    Serial.begin(115200);
    Serial.println("\n=====================================");
    Serial.println("  ESP32 SMART PILL BOX INITIALIZED   ");
    Serial.println("=====================================");

    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);

    if (!rtcManager.begin(RTC_SDA_PIN, RTC_SCL_PIN)) {
        Serial.println("[ERROR] RTC DS1307 module missing!");
    } else {
        Serial.println("[OK] RTC DS1307 Module Online.");
    }

    servoManager.begin(SERVO_PIN);
    Serial.println("[OK] Servo Actuator Locked.");

    buzzerManager.begin(BUZZER_PIN);
    Serial.println("[OK] Non-blocking Audio Buzz Node Linked.");

    loadCellManager.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    Serial.println("[OK] HX711 Load Cell Driver Initialized & Zeroed.");

    // SỬA TẠI ĐÂY: Khởi tạo IR kèm chân cấp nguồn (IR_POWER_PIN) để có thể tắt mở
    irSensorManager.begin(IR_SENSOR_PIN, IR_POWER_PIN);
    irSensorManager.powerOff(); // Mặc định tắt ngay lúc khởi động để tiết kiệm pin
    Serial.println("[OK] Safety IR Limit System Configured (Default: OFF).");

    // ====================================================================
    // THÊM TẠI ĐÂY: Khởi chạy mạng MQTT (Thay đổi thông số cho khớp với Config.h của bạn)
    // ====================================================================
    // Ví dụ: mqttManager.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP, MQTT_PORT);
    mqttManager.begin("Ten_WiFi", "Mat_Khau", "10.10.15.97", 1883); 
    
    // Đăng ký nhận lịch hẹn từ Server đổ về hộp thuốc thông qua cơ chế Callback
    mqttManager.setOnTimeUpdate([](int h, int m) {
        // Khi nhận được giờ mới từ MQTT, cập nhật thẳng vào bộ FSM
        extern PillBoxController controller; // Hoặc dùng cơ chế con trỏ instance
        // controller.setAlarmTime(h, m);
    });
    // ====================================================================

    Serial.printf("[INFO] Monitor alarm set for %02d:%02d daily.\n", alarmHour, alarmMinute);
    Serial.println("System Core Mode: IDLE");
}

// THÊM HÀM MỚI: Để cập nhật dữ liệu động khi có MQTT bắn tín hiệu về
void PillBoxController::setAlarmTime(int hour, int minute) {
    alarmHour = hour;
    alarmMinute = minute;
    scheduleTriggeredToday = false; // Reset trạng thái để sẵn sàng cho khung giờ mới
    Serial.printf("[FSM] Lịch uống thuốc đã được cập nhật động sang: %02d:%02d\n", alarmHour, alarmMinute);
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

    if (now.hour() == 0 && now.minute() == 0 && scheduleTriggeredToday) {
        scheduleTriggeredToday = false;
        Serial.println("[INFO] Daily system event tracking cycle reset.");
    }

    PillBoxState previousState = currentState;

    switch (currentState) {
        case IDLE:
            // SỬA TẠI ĐÂY: Kiểm tra theo biến động alarmHour và alarmMinute thay vì cấu hình tĩnh macro
            if (now.hour() == alarmHour && now.minute() == alarmMinute && !scheduleTriggeredToday) {
                Serial.println("\n--- ALARM EVENT DETECTED ---");
                Serial.print("[RTC] System Clock Stamp: ");
                Serial.println(rtcManager.getTimeString());

                beforeWeight = loadCellManager.getWeight();
                Serial.printf("[CELL] Pre-ingestion weight base: %.2f g\n", beforeWeight);

                servoManager.open();
                Serial.println("[MECHANISM] Container hatch latch unlocked.");

                buzzerManager.startBeeping();
                Serial.println("[ALARM] Transmitting auditory loop signal.");

                scheduleTriggeredToday = true;
                currentState = ALARM_RINGING;
                stateTimer = millis();
            }
            break;

        case ALARM_RINGING:
            if (millis() - stateTimer >= 1000) {
                currentState = BOX_OPEN;
            }
            break;

        case BOX_OPEN:
            {
                float currentWeight = loadCellManager.getWeight();
                if (beforeWeight - currentWeight >= CONTAINER_REMOVED_THRESHOLD) {
                    Serial.println("[LOG] Container removed from platform scale.");

                    buzzerManager.stopBeeping();

                    currentState = WAIT_FOR_RETURN;
                }
            }
            break;

        case WAIT_FOR_RETURN:
            {
                float currentWeight = loadCellManager.getWeight();
                if (currentWeight > (beforeWeight - CONTAINER_REMOVED_THRESHOLD)) {
                    Serial.println("[LOG] Mass load detected on platform. Analyzing weight stabilization...");
                    stateTimer = millis();
                    currentState = VERIFY_MEDICINE;
                }
            }
            break;

        case VERIFY_MEDICINE:
            if (millis() - stateTimer >= 1500) { 
                afterWeight = loadCellManager.getWeight();
                float weightDelta = beforeWeight - afterWeight;

                if (weightDelta >= MEDICINE_WEIGHT_THRESHOLD) {
                    Serial.println("[OK] Dose match validated.");
                    
                    // ========================================================
                    // THÊM TẠI ĐÂY: Gửi gói tin xác nhận lên MQTT cho người dùng
                    // ========================================================
                    String statusMsg = "COMPLETED|" + rtcManager.getTimeString(); 
                    // Kết quả chuỗi gửi đi sẽ có dạng: "COMPLETED|2026-05-25 08:32:15"
                    mqttManager.publishStatus(statusMsg.c_str()); 
                    // ========================================================

                    irSensorManager.powerOn(); 
                    currentState = BOX_CLOSING;
                } else {
                    Serial.println("[WARN] Failure: Container replaced without safe consumption variance.");
                    buzzerManager.startBeeping(); 
                    currentState = BOX_OPEN;
                }
            }
            break;

        case BOX_CLOSING:
            // Lúc này cảm biến IR đang bật nguồn và quét vật cản
            if (irSensorManager.isObstacleDetected()) {
                Serial.println("[CRITICAL] Latch path blocked via IR feedback! Halting lock servo.");
                stateTimer = millis(); 
            } else {
                Serial.println("[MECHANISM] Target pathway empty. Throwing locking bolt deadbolt.");
                servoManager.lock();
                
                // SỬA TẠI ĐÂY: Khóa chốt an toàn xong xuôi, TẮT HẲN NGUỒN CẢM BIẾN IR để tiết kiệm điện
                irSensorManager.powerOff(); 
                
                currentState = COMPLETED;
            }
            break;

        case COMPLETED:
            // Đảm bảo an toàn: luôn tắt nguồn IR ở trạng thái nghỉ
            irSensorManager.powerOff(); 
            
            // SỬA TẠI ĐÂY: So sánh với alarmMinute động
            if (now.minute() != alarmMinute) {
                Serial.println("[SYSTEM] Operations loop concluded. Re-arming for tomorrow.");
                currentState = IDLE;
            }
            break;
    }

    if (currentState != previousState) {
        Serial.printf("[FSM TRANSITION] %s -> %s\n", getStateString(previousState), getStateString(currentState));
    }
}