#include "PillBoxController.h"
#include <Arduino.h>
#include "../wifi_config.h"

// Khai báo instance toàn cục của Controller để các hàm Lambda trong MQTT Callback có thể gọi tới
extern PillBoxController controller;

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

    // Khởi tạo màn hình OLED
    if (!oled.begin()) {
        Serial.println("[ERROR] OLED Display (SSD1306) failed to initialize!");
    } else {
        Serial.println("[OK] OLED Display Online.");
        oled.displayStatus("  INITIALIZING", "System checking...", "Please wait");
    }

    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);

    if (!rtcManager.begin(RTC_SDA_PIN, RTC_SCL_PIN)) {
        Serial.println("[ERROR] RTC DS1307 module missing!");
        oled.displayStatus("  RTC ERROR", "Module missing!", "Check wiring");
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
    Serial.printf("[CELL] Startup weight reading: %.2f g\n", startupWeight);

    loadCellManager.powerDown(); 
    Serial.println("[OK] HX711 Load Cell Driver Initialized & Powered Down.");

    irSensorManager.begin(IR_SENSOR_PIN, IR_POWER_PIN);
    irSensorManager.powerOff(); // Mặc định tắt ngay lúc khởi động để tiết kiệm pin
    Serial.println("[OK] Safety IR Limit System Configured (Default: OFF).");

    // Khởi chạy mạng MQTT
    mqttManager.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP, MQTT_PORT); 
    
    // ================================================================
    // ĐĂNG KÝ CÁC CALLBACK NHẬN LỆNH TỪ MQTT CHUYỂN GIAO XUỐNG FSM
    // ================================================================
    
    // 1. Cập nhật lịch hẹn uống thuốc động
    mqttManager.setOnTimeUpdate([](int slot, int h, int m) {
        controller.setAlarmTime(slot, h, m); 
    });

    // 2. Cập nhật đồng bộ thời gian thực cho chip RTC DS1307
    mqttManager.setOnRtcSync([](int h, int m, int s) {
        controller.handleRtcTimeSync(h, m, s);
    });

    // 3. Kích hoạt lệnh cân định chuẩn (Tare) từ xa
    mqttManager.setOnTareCommand([]() {
        controller.handleLoadCellTare();
    });

    Serial.printf("[INFO] Monitor alarm set for %02d:%02d daily.\n", alarmHour, alarmMinute);
    Serial.println("System Core Mode: IDLE (Power Saving Management Active)");
    oled.displayStatus("  SYSTEM READY", "Waiting for", "scheduled time...");
}

// ================================================================
// CÁC HÀM XỬ LÝ LỆNH MQTT MỚI
// ================================================================

void PillBoxController::handleRtcTimeSync(int hour, int minute, int second) {
    DateTime now = rtcManager.getCurrentTime();
    // Đồng bộ lại giờ phút giây nhưng giữ nguyên ngày tháng năm hiện tại
    DateTime newTime(now.year(), now.month(), now.day(), hour, minute, second);
    rtcManager.adjust(newTime);
    
    Serial.printf("[MQTT] RTC Time successfully synchronized to: %02d:%02d:%02d\n", hour, minute, second);
    
    char buf[20];
    snprintf(buf, sizeof(buf), "Synced: %02d:%02d:%02d", hour, minute, second);
    oled.displayStatus("  RTC SYNCED", "Time updated!", buf);
    delay(2000); // Giữ màn hình hiển thị trong 2 giây
}

void PillBoxController::handleLoadCellTare() {
    Serial.println("[MQTT] Tare command received. Waking up loadcell sensor...");
    
    // Đánh thức cảm biến lực lên để cấu hình điểm 0 gốc
    loadCellManager.powerUp();
    delay(200);
    
    loadCellManager.tare(); 
    Serial.println("[CELL] Scale Tare adjustment completed successfully.");
    
    // Nếu thiết bị đang ở chế độ chờ IDLE thì ép chip ngủ lại để tiết kiệm điện nguồn
    if (currentState == IDLE) {
        loadCellManager.powerDown();
    }
    
    oled.displayStatus("  SCALE TARE", "Loadcell reset", "to 0.0g done!");
    delay(2000);
}

void PillBoxController::setAlarmTime(int slot, int hour, int minute) {
    if (slot >= 0 && slot < MAX_SCHEDULES) {
        schedules[slot].hour = hour;
        schedules[slot].minute = minute;
        schedules[slot].completedToday = false; // Có lịch mới thì reset trạng thái lịch đó
        Serial.printf("[FSM] Lịch uống thuốc Slot [%d] cập nhật động thành: %02d:%02d\n", slot, hour, minute);
        
        char buf[20];
        snprintf(buf, sizeof(buf), "Slot [%d] -> %02d:%02d", slot, hour, minute);
        oled.displayStatus(" NEW SCHEDULE", "Received via MQTT:", buf);
        delay(2000); 
    } else {
        Serial.printf("[FSM][ERROR] Slot %d vượt quá giới hạn hệ thống!\n", slot);
    }
}

// ================================================================
// VÒNG LẶP CHÍNH QUẢN LÝ TRẠNG THÁI (FSM)
// ================================================================

void PillBoxController::update() {
    mqttManager.update();
    buzzerManager.update();
    DateTime now = rtcManager.getCurrentTime();

    if (now.hour() == 0 && now.minute() == 0 && now.second() == 0) {
        resetSchedulesForNewDay();
        delay(1000); 
    }

    PillBoxState previousState = currentState;
    char weightStr[20] = "";
    char timeStr[20] = "";

    switch (currentState) {
        case IDLE: 
            for (int i = 0; i < MAX_SCHEDULES; i++) {
                if (!schedules[i].completedToday && now.hour() == schedules[i].hour && now.minute() == schedules[i].minute) {
                    triggerAlarmSequence(i); // <-- Gom logic kích hoạt chuông / mở chốt / đọc cân nặng nền
                    break; 
                }
            }

            if (currentState == IDLE) {
                snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d:%02d", now.hour(), now.minute(), now.second());
                oled.displayStatus("  SYSTEM READY", timeStr, getNextScheduleStr().c_str()); // <-- Gom logic lấy lịch tiếp theo hiển thị lên OLED
                
                // Ghi log trạng thái hệ thống ra cổng Serial Monitor định kỳ theo mỗi giây
                Serial.printf("\n=========================================\n");
                Serial.printf("[RTC NOW] Thời gian thực: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
                Serial.println("-----------------------------------------");
                Serial.println("[SCHEDULES] Danh sách lịch hẹn trong máy:");
                for (int i = 0; i < MAX_SCHEDULES; i++) {
                    Serial.printf("  -> Slot [%d]: %02d:%02d | Trạng thái: %s\n", 
                                  i, schedules[i].hour, schedules[i].minute, 
                                  schedules[i].completedToday ? "ĐÃ UỐNG ĐỦ" : "CHƯA UỐNG");
                }
                Serial.printf("=========================================\n");
                delay(1000); 
            }
            break;

        case ALARM_RINGING:
            // Hiệu ứng chữ nhấp nháy trên màn hình
            oled.displayStatus(" !!! ALARM !!!", ((millis() / 400) % 2 == 0) ? "TAKE MEDICINE!" : "", "Time to ingest");

            if (!irSensorManager.isObstacleDetected()) {
                Serial.println("[FSM] Lid opened detected via IR. Moving to BOX_OPEN.");
                buzzerManager.stopBeeping();
                currentState = BOX_OPEN;
            }
            break;

        case BOX_OPEN:
            if (!boxOpenInitialized) { 
                initializeBoxOpenScale(); // <-- Gom logic tắt cảm biến IR và đánh thức chip cân tránh sụt áp nguồn
            }  

            {   
                float currentWeight = loadCellManager.getWeight();
                Serial.printf("[DEBUG] before=%.2f current=%.2f delta=%.2f\n", beforeWeight, currentWeight, beforeWeight - currentWeight);

                snprintf(weightStr, sizeof(weightStr), "Weight: %.1fg", currentWeight);
                oled.displayStatus("  BOX OPENED", "Please take out", "the container.", weightStr);

                if (beforeWeight - currentWeight >= CONTAINER_REMOVED_THRESHOLD) {
                    Serial.println("[LOG] Container removed from platform scale.");
                    currentState = WAIT_FOR_RETURN;
                }
                delay(200); 
            }
            break;

        case WAIT_FOR_RETURN:
            {
                float currentWeight = loadCellManager.getWeight();
                snprintf(weightStr, sizeof(weightStr), "Weight: %.1fg", currentWeight);
                oled.displayStatus(" WAIT FOR RETURN", "Put the container", "back to scale!", weightStr);

                if (currentWeight >= (beforeWeight - CONTAINER_REMOVED_THRESHOLD)) {
                    Serial.println("[LOG] Mass load detected on platform. Analyzing weight stabilization...");
                    stateTimer = millis();
                    currentState = VERIFY_MEDICINE;
                }
                delay(200); 
            }
            break;

        case VERIFY_MEDICINE:
            oled.displayStatus("  VERIFYING...", "Analyzing mass,", "please sit tight");

            if (millis() - stateTimer >= WEIGHT_STABILIZATION_TIME) { 
                processWeightVerification(); // <-- Gom logic kiểm định trọng lượng và đẩy gói tin MQTT lên Server
            }
            break;

        case BOX_CLOSING:
            oled.displayStatus("  CLOSING BOX", "Please shut down", "the box lid.");
            handleLidClosingRoutine(); // <-- Gom bộ đếm thời gian xác nhận nắp đóng chặt và khóa chốt Servo cơ khí
            break;

        case COMPLETED:
            boxOpenInitialized = false; 
            activeScheduleIndex = -1;   
            Serial.println("[SYSTEM] Operations loop concluded. Returning to IDLE for next schedule.");
            
            oled.displayStatus("  SUCCESSFUL", "Medicine taken!", "Thank you!");
            delay(3000); 

            currentState = IDLE; 
            break;
    }

    if (currentState != previousState) {
        Serial.printf("[FSM TRANSITION] %s -> %s\n", getStateString(previousState), getStateString(currentState));
    }
}

// ================================================================
// ĐỊNH NGHĨA CHI TIẾT CÁC HÀM HELPER ĐÃ GOM NHÓM LOGIC
// ================================================================

void PillBoxController::triggerAlarmSequence(int scheduleIndex) {
    activeScheduleIndex = scheduleIndex;
    Serial.printf("\n--- ALARM EVENT DETECTED FOR SLOT [%d] ---\n", activeScheduleIndex);
    Serial.printf("[RTC] Target Time: %02d:%02d\n", schedules[scheduleIndex].hour, schedules[scheduleIndex].minute);

    oled.displayStatus(" !!! WARNING !!!", "Opening vault...", "Get ready!");

    // Điều khiển servo giải phóng chốt cơ khí
    servoManager.attach();
    delay(500); 
    servoManager.open();
    delay(1500); 
    servoManager.detach();

    // Bật cảm biến lực đọc cân nặng cơ sở tĩnh lúc đầu
    loadCellManager.powerUp();
    delay(500); 
    for(int i = 0; i < 5; i++) {
        loadCellManager.getWeight();
        delay(20);
    }
    beforeWeight = loadCellManager.getWeight();
    Serial.printf("[CELL] Pre-ingestion weight base: %.2f g\n", beforeWeight);
    loadCellManager.powerDown();

    // Kích hoạt còi báo động và hệ thống cảm biến hồng ngoại IR hành trình nắp
    buzzerManager.startBeeping();
    delay(100); 
    irSensorManager.powerOn();
    delay(100); 

    currentState = ALARM_RINGING;
    stateTimer = millis();
}

void PillBoxController::initializeBoxOpenScale() {
    irSensorManager.powerOff();
    delay(100); 
    loadCellManager.powerUp();
    delay(500);
    boxOpenInitialized = true;
}

void PillBoxController::processWeightVerification() {
    afterWeight = loadCellManager.getWeight();
    float weightDelta = beforeWeight - afterWeight;

    if (weightDelta >= MEDICINE_WEIGHT_THRESHOLD) {
        Serial.println("[OK] Dose match validated.");
        
        if (activeScheduleIndex != -1) {
            schedules[activeScheduleIndex].completedToday = true;
            Serial.printf("[FSM] Slot [%d] marked as COMPLETED for today.\n", activeScheduleIndex);
        }
        
        // Phát dữ liệu báo cáo trạng thái hoàn tất lên server thông qua MQTT broker
        String statusMsg = "COMPLETED|SLOT_" + String(activeScheduleIndex) + "|" + rtcManager.getTimeString(); 
        mqttManager.publishStatus(statusMsg.c_str()); 

        loadCellManager.powerDown(); 
        delay(100); 

        irSensorManager.powerOn(); 
        currentState = BOX_CLOSING;
    } else {
        Serial.println("[WARN] Failure: Container replaced without safe consumption variance.");
        oled.displayStatus(" VERIFY ERROR", "Incorrect dosage!", "Take original pill");
        delay(2000);
        currentState = BOX_OPEN; // Chuyển lại trạng thái trước yêu cầu nhấc khay thuốc thao tác lại
    }
}

void PillBoxController::handleLidClosingRoutine() {
    if (irSensorManager.isObstacleDetected()) {
        if (!lidCloseTimerRunning) {
            lidClosedStartTime = millis();
            lidCloseTimerRunning = true;
            Serial.println("[LOG] Lid detected closed. Starting confirmation timer...");
        }

        if (millis() - lidClosedStartTime >= LID_CLOSE_CONFIRMATION_TIME) {
            Serial.println("[OK] Lid remained closed long enough. Locking box.");
            oled.displayStatus("  LOCKING BOX", "Securing vault,", "please wait...");

            irSensorManager.powerOff(); 
            delay(100); 
            servoManager.attach();
            delay(200); 
            servoManager.lock();
            delay(500); 
            servoManager.detach();

            lidCloseTimerRunning = false;
            currentState = COMPLETED;
        }
    } else {
        lidCloseTimerRunning = false; 
    }
}

String PillBoxController::getNextScheduleStr() {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!schedules[i].completedToday) {
            char buf[25];
            snprintf(buf, sizeof(buf), "Next: %02d:%02d (S%d)", schedules[i].hour, schedules[i].minute, i);
            return String(buf);
        }
    }
    return String("No active slots");
}

void PillBoxController::resetSchedulesForNewDay() {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedules[i].completedToday = false;
    }
    Serial.println("[INFO] New day detected. All medication schedules reset to uncompleted.");
}

// Thêm đoạn này vào cuối file PillBoxController.cpp
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