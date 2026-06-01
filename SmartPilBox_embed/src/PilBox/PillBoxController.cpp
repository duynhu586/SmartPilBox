#include "PillBoxController.h"
#include "config.h"
#include <Arduino.h>
#include "esp_sleep.h" 
#include "esp_wifi.h" 
#include "esp_pm.h" 

PillBoxController::PillBoxController() : 
    currentState(IDLE), 
    beforeWeight(0.0), 
    afterWeight(0.0), 
    stateTimer(0), 
    scheduleTriggeredToday(false),
    lastCheckedMinute(-1),
    alarmHour(SCHEDULE_HOUR),     
    alarmMinute(SCHEDULE_MINUTE)  
{}

void PillBoxController::begin() {
    Serial.begin(115200);
    Serial.println("\n=====================================");
    Serial.println("  ESP32 SMART PILL BOX INITIALIZED   ");
    Serial.println("=====================================");

    // [QUẢN LÝ NĂNG LƯỢNG THÔNG MINH]
    #if CONFIG_PM_ENABLE
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 40,        // CPU tự giảm tốc xuống 40MHz khi rảnh
        .light_sleep_enable = true // Tự động ngậm ngủ đồng bộ chu kỳ Wifi
    };
    if (esp_pm_configure(&pm_config) == ESP_OK) {
        Serial.println("[POWER] Auto Light Sleep Mode Configured Successfully.");
    } else {
        Serial.println("[POWER] Failed to configure Auto Light Sleep.");
    }
    #endif

    // -----------------------------------------------------------
    // KHỞI TẠO OLED TRÊN LUỒNG WIRE MẶC ĐỊNH (I2C CỔNG 0)
    // -----------------------------------------------------------
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN, 100000);
    // Nếu bạn có khai báo đối tượng hiển thị (ví dụ: display), hãy khởi tạo tại đây:
    // display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

    // -----------------------------------------------------------
    // KHỞI TẠO RTC TRÊN LUỒNG WIRE1 ĐỘC LẬP (I2C CỔNG 1)
    // -----------------------------------------------------------
    // XÓA BỎ dòng Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN) cũ tại đây để bảo vệ chân OLED
    if (!rtcManager.begin(RTC_SDA_PIN, RTC_SCL_PIN)) {
        Serial.println("[ERROR] RTC module missing on Wire1 Bus!");
    } else {
        Serial.println("[OK] RTC Module Online via Dedicated Wire1 Bus.");
        if (rtcManager.lostPower()) { 
            Serial.println("[WARN] RTC lost power! Syncing time from compile timestamp (+offset for build host)...");
            DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
            // Cộng thêm múi giờ chuyển từ UTC của máy build sang giờ địa phương
            DateTime localTime = compileTime + TimeSpan(TIMEZONE_OFFSET * 3600);
            rtcManager.adjust(localTime);
        }
    }

    // [QUẢN LÝ NGUỒN NGOẠI VI]: Khóa chốt ngay rồi gỡ xung Servo chống hao điện dòng tĩnh
    servoManager.begin(SERVO_PIN);
    servoManager.lock();
    delay(500); 
    servoManager.detach(); 
    Serial.println("[OK] Servo Actuator Locked & Detached.");

    buzzerManager.begin(BUZZER_PIN);
    Serial.println("[OK] Non-blocking Audio Buzz Node Linked.");

    // [QUẢN LÝ NGUỒN NGOẠI VI]: Ép chip cân HX711 đi ngủ
    loadCellManager.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    loadCellManager.powerDown(); 
    Serial.println("[OK] HX711 Load Cell Driver Initialized & Powered Down.");

    // [QUẢN LÝ NGUỒN NGOẠI VI]: Ngắt nguồn cảm biến IR
    irSensorManager.begin(IR_SENSOR_PIN, IR_POWER_PIN);
    irSensorManager.powerOff(); 
    Serial.println("[OK] Safety IR Limit System Configured (Default: OFF).");

    // Khởi chạy mạng MQTT
    mqttManager.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP, MQTT_PORT); 
    
    mqttManager.setOnTimeUpdate([](int h, int m) {
        extern PillBoxController controller; 
        controller.setAlarmTime(h, m);
    });

    mqttManager.setOnRTCUpdate([](uint32_t epoch) {
        extern PillBoxController controller;
        controller.adjustRTC(epoch);
    });

    Serial.printf("[INFO] Monitor alarm set for %02d:%02d daily.\n", alarmHour, alarmMinute);
    Serial.println("System Core Mode: IDLE (Power Saving Management Active)");
}

void PillBoxController::setAlarmTime(int hour, int minute) {
    alarmHour = hour;
    alarmMinute = minute;
    scheduleTriggeredToday = false; 
    Serial.printf("[FSM] Lịch uống thuốc đã được cập nhật động sang: %02d:%02d\n", alarmHour, alarmMinute);
}

void PillBoxController::adjustRTC(uint32_t epoch) {
    rtcManager.adjust(DateTime(epoch));
    Serial.printf("[FSM] RTC Time updated via MQTT. Epoch: %u\n", epoch);
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
    
    // CHỐNG NHIỄU SLEEP: Tạo độ trễ micro giây cực ngắn để ổn định phần cứng khi CPU thay đổi tần số (40Mhz <-> 240Mhz)
    delayMicroseconds(5000); 

    DateTime now = rtcManager.getCurrentTime();

    if (now.hour() == 0 && now.minute() == 0 && scheduleTriggeredToday) {
        scheduleTriggeredToday = false;
        Serial.println("[INFO] Daily system event tracking cycle reset.");
    }

    switch (currentState) {
        case IDLE:
            if (now.hour() == alarmHour && now.minute() == alarmMinute && !scheduleTriggeredToday) {
                Serial.println("\n--- ALARM EVENT DETECTED ---");
                Serial.print("[RTC] System Clock Stamp: ");
                Serial.println(rtcManager.getTimeString());

                loadCellManager.powerUp();
                delay(500); // Chờ áp nguồn cảm biến cân ổn định sau khi thức giấc

                beforeWeight = loadCellManager.getWeight();
                Serial.printf("[CELL] Pre-ingestion weight base: %.2f g\n", beforeWeight);

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
                // Giảm delay xuống 100ms để vòng lặp update nhạy bén hơn nhưng không tốn tài nguyên CPU
                delay(100); 
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
                    // Chuyển tiếp trạng thái tiếp theo của bạn tại đây...
                }
            }
            break;
            
        default:
            break;
    }
}
