#include "PillBoxController.h"
#include <Arduino.h>
#include "esp_sleep.h" // Thư viện lõi điều khiển ngủ Sleep của ESP32
#include "esp_wifi.h" // <-- THÊM THƯ VIỆN NÀY
#include "esp_pm.h" // <-- THÊM THƯ VIỆN QUẢN LÝ NĂNG LƯỢNG (Power Management) CỦA ESP32

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

    // ================================================================
    // BẮT ĐẦU CHÈN CODE CẤU HÌNH TIẾT KIỆM PIN TỰ ĐỘNG TẠI ĐÂY
    // ================================================================
    #if CONFIG_PM_ENABLE
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 40,        // CPU giảm tốc xuống 40MHz khi rảnh để tiết kiệm điện
        .light_sleep_enable = true // Tự động light sleep đồng bộ với chu kỳ beacon của WiFi
    };
    if (esp_pm_configure(&pm_config) == ESP_OK) {
        Serial.println("[POWER] Auto Light Sleep Mode Configured Successfully.");
    } else {
        Serial.println("[POWER] Failed to configure Auto Light Sleep.");
    }
    #endif
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
    servoManager.lock();
    delay(500); // Đợi ngắn cho servo quay hết hành trình
    servoManager.detach(); 
    Serial.println("[OK] Servo Actuator Locked & Detached.");

    buzzerManager.begin(BUZZER_PIN);
    Serial.println("[OK] Non-blocking Audio Buzz Node Linked.");

    // [QUẢN LÝ NGUỒN]: Đọc tải trọng ban đầu xong ép chip cân HX711 đi ngủ lập tức
    loadCellManager.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    loadCellManager.powerDown(); 
    Serial.println("[OK] HX711 Load Cell Driver Initialized & Powered Down.");

    irSensorManager.begin(IR_SENSOR_PIN, IR_POWER_PIN);
    irSensorManager.powerOff(); // Mặc định tắt ngay lúc khởi động để tiết kiệm pin
    Serial.println("[OK] Safety IR Limit System Configured (Default: OFF).");

    // Khởi chạy mạng MQTT
    mqttManager.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP, MQTT_PORT); 
    
    // Đăng ký nhận lịch hẹn từ Server đổ về hộp thuốc thông qua cơ chế Callback
    mqttManager.setOnTimeUpdate([](int h, int m) {
        extern PillBoxController controller; 
        controller.setAlarmTime(h, m);
    });

    Serial.printf("[INFO] Monitor alarm set for %02d:%02d daily.\n", alarmHour, alarmMinute);
    Serial.println("System Core Mode: IDLE (Power Saving Management Active)");
}

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
    // Thêm log này vào update() đầu hàm
    // Serial.printf("[RTC] Current time: %02d:%02d:%02d\n", 
    //     now.hour(), now.minute(), now.second());

    if (now.hour() == 0 && now.minute() == 0 && scheduleTriggeredToday) {
        scheduleTriggeredToday = false;
        Serial.println("[INFO] Daily system event tracking cycle reset.");
    }

    PillBoxState previousState = currentState;

    switch (currentState) {
        case IDLE:
            Serial.printf("[DEBUG] Now: %02d:%02d | Alarm: %02d:%02d | TriggeredToday: %s\n",
            now.hour(), now.minute(), alarmHour, alarmMinute,
            scheduleTriggeredToday ? "TRUE" : "FALSE");

            if (now.hour() == alarmHour && now.minute() == alarmMinute && !scheduleTriggeredToday) {
                Serial.println("\n--- ALARM EVENT DETECTED ---");
                Serial.print("[RTC] System Clock Stamp: ");
                Serial.println(rtcManager.getTimeString());

                // [QUẢN LÝ NGUỒN]: Đánh thức cảm biến lực cân khi bắt đầu chu trình kiểm tra lịch
                loadCellManager.powerUp();
                delay(500); // Chờ điện áp IC ổn định sau khi thức dậy

                beforeWeight = loadCellManager.getWeight();
                Serial.printf("[CELL] Pre-ingestion weight base: %.2f g\n", beforeWeight);

                // [QUẢN LÝ NGUỒN]: Cấp lại năng lượng và tín hiệu PWM điều khiển Servo
                servoManager.attach();
                servoManager.open();
                Serial.println("[MECHANISM] Container hatch latch unlocked.");

                buzzerManager.startBeeping();
                Serial.println("[ALARM] Transmitting auditory loop signal.");

                scheduleTriggeredToday = true;
                currentState = ALARM_RINGING;
                stateTimer = millis();
            }
            else {
                // SỬA TẠI ĐÂY: KHÔNG gọi esp_light_sleep_start() thủ công nữa!
                // Chip đã tự ngủ ngầm nhờ cấu hình tự động ở begin().
                // Ta chỉ delay nhẹ 50ms để giải phóng CPU cho các task hệ thống.
                delay(1000);
            }
            break;

        case ALARM_RINGING:
            if (millis() - stateTimer >= ALARM_RINGING_TIMEOUT) {
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
            if (millis() - stateTimer >= WEIGHT_STABILIZATION_TIME) { 
                afterWeight = loadCellManager.getWeight();
                float weightDelta = beforeWeight - afterWeight;

                if (weightDelta >= MEDICINE_WEIGHT_THRESHOLD) {
                    Serial.println("[OK] Dose match validated.");
                    
                    String statusMsg = "COMPLETED|" + rtcManager.getTimeString(); 
                    mqttManager.publishStatus(statusMsg.c_str()); 

                    // [QUẢN LÝ NGUỒN]: Bật nguồn cảm biến IR kiểm tra dị vật và cấp lại điện Servo để chuẩn bị khóa chốt
                    irSensorManager.powerOn(); 
                    servoManager.attach();

                    currentState = BOX_CLOSING;
                } else {
                    Serial.println("[WARN] Failure: Container replaced without safe consumption variance.");
                    buzzerManager.startBeeping(); 
                    currentState = BOX_OPEN;
                }
            }
            break;

        case BOX_CLOSING:
            if (irSensorManager.isObstacleDetected()) {
                Serial.println("[CRITICAL] Latch path blocked via IR feedback! Halting lock servo.");
                stateTimer = millis(); 
            } else {
                Serial.println("[MECHANISM] Target pathway empty. Throwing locking bolt deadbolt.");
                servoManager.lock();
                delay(250); // Chờ ngắn đảm bảo Servo gạt chốt cơ khí an toàn xong xuôi
                
                // [QUẢN LÝ NGUỒN]: Ngắt điện dứt điểm ngoại vi sau khi hoàn tất chu trình vật lý
                servoManager.detach();     // Ngắt xung điều khiển Servo (Chống rung, chống nóng)
                irSensorManager.powerOff(); // Cắt điện hoàn toàn cụm LED phát thu hồng ngoại
                
                currentState = COMPLETED;
            }
            break;

        case COMPLETED:
            // Khóa an toàn: Đảm bảo tắt triệt để các thiết bị tiêu thụ lớn ở trạng thái tĩnh
            irSensorManager.powerOff(); 
            servoManager.detach();
            loadCellManager.powerDown(); // Đưa module cân trở lại chế độ ngủ sâu bằng chân SCK
            
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