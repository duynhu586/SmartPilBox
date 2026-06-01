#include "MQTTManager.h"
#include "../Config.h"
#include "esp_wifi.h"

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager() : client(espClient), lastReconnectAttempt(0) {
    _instance = this;
    
    // Khởi tạo biến cho Schedule
    _currentSlot = 0;
    _currentHour = SCHEDULE_HOUR; 
    _currentMinute = SCHEDULE_MINUTE;
    _hasNewSchedule = false;
    _timeCb = nullptr;

    // Khởi tạo biến cho RTC mới
    _rtcHour = 0; _rtcMinute = 0; _rtcSecond = 0;
    _hasRtcSync = false;
    _rtcCb = nullptr;

    // Khởi tạo biến cho Tare mới
    _hasTareCommand = false;
    _tareCb = nullptr;
}

void MQTTManager::begin(const char* ssid, const char* password, const char* brokerIp, int port) {
    _brokerIp = brokerIp;
    _port = port;

    Serial.print("[WiFi] Connecting to "); Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Connected!");

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM); 
    Serial.println("[POWER] WiFi Modem Power Save Enabled.");

    WiFiClient testClient;
    Serial.println("[TEST] Connecting to broker socket...");
    if (testClient.connect(_brokerIp, _port)) {
        Serial.println("[TEST] TCP connection SUCCESS");
        testClient.stop();
    } else {
        Serial.println("[TEST] TCP connection FAILED");
    }

    client.setServer(_brokerIp, _port);
    client.setCallback(mqttCallback);
}

// Các hàm Setter đăng ký hàm xử lý từ Controller đổ về
void MQTTManager::setOnTimeUpdate(TimeUpdateCallback cb)   { _timeCb = cb; }
void MQTTManager::setOnRtcSync(RtcSyncCallback cb)         { _rtcCb = cb; }
void MQTTManager::setOnTareCommand(TareCommandCallback cb) { _tareCb = cb; }

void MQTTManager::update() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (reconnectNonBlocking()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        client.loop();

        // 1. XỬ LÝ LỊCH MỚI TẠI LUỒNG CHÍNH (Core 1)
        if (_hasNewSchedule) {
            _hasNewSchedule = false; 
            Serial.printf("[MQTT] Luồng chính phát hiện lịch mới cho Slot [%d]: %02d:%02d\n", _currentSlot, _currentHour, _currentMinute);
            if (_currentHour >= 0 && _currentHour <= 23 && _currentMinute >= 0 && _currentMinute <= 59) {
                if (_timeCb != nullptr) _timeCb(_currentSlot, _currentHour, _currentMinute); 
            }
        }

        // 2. XỬ LÝ ĐỒNG BỘ GIỜ RTC TẠI LUỒNG CHÍNH
        if (_hasRtcSync) {
            _hasRtcSync = false; // Hạ cờ hiệu rtc
            Serial.printf("[MQTT] Luồng chính thực hiện đồng bộ RTC: %02d:%02d:%02d\n", _rtcHour, _rtcMinute, _rtcSecond);
            if (_rtcHour >= 0 && _rtcHour <= 23 && _rtcMinute >= 0 && _rtcMinute <= 59 && _rtcSecond >= 0 && _rtcSecond <= 59) {
                if (_rtcCb != nullptr) _rtcCb(_rtcHour, _rtcMinute, _rtcSecond);
            }
        }

        // 3. XỬ LÝ LỆNH TARE CÂN TẠI LUỒNG CHÍNH
        if (_hasTareCommand) {
            _hasTareCommand = false; // Hạ cờ hiệu tare
            Serial.println("[MQTT] Luồng chính kích hoạt hàm Tare cân nặng.");
            if (_tareCb != nullptr) _tareCb();
        }
    }
}

bool MQTTManager::reconnectNonBlocking() {
    Serial.print("[MQTT] Attempting connection to Local Broker...");
    String clientId = "ESP32PillBox-" + WiFi.macAddress();
    clientId.replace(":", ""); 

    if (client.connect(clientId.c_str())) {
        Serial.println("Connected!");
        
        // Đăng ký (Subscribe) đồng thời 3 Topics lệnh từ Server gửi xuống
        client.subscribe(_topicSchedule); // pillbox/schedule
        client.subscribe(_topicSetRtc);   // pillbox/set_rtc
        client.subscribe(_topicTare);     // pillbox/tare
        return true;
    } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" -> Will retry in 5 seconds without blocking CPU.");
        return false;
    }
}

void MQTTManager::reconnect() {
    reconnectNonBlocking();
}

// CALLBACK CHẠY TRÊN CORE NGẦM (Core 0): Phân tách chuỗi nhanh và dựng cờ hiệu tương ứng
void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance == nullptr) return;

    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';

    // Xử lý 1: Nhận lịch hẹn mới "slot|hour|minute"
    if (strcmp(topic, _instance->_topicSchedule) == 0) {
        int parsedSlot = 0, parsedHour = 0, parsedMinute = 0;
        if (sscanf(buffer, "%d|%d|%d", &parsedSlot, &parsedHour, &parsedMinute) == 3) {
            _instance->_currentSlot = parsedSlot;
            _instance->_currentHour = parsedHour;
            _instance->_currentMinute = parsedMinute;
            _instance->_hasNewSchedule = true; 
        } else {
            Serial.println("[MQTT][ERROR] Sai định dạng lịch trình! Kỳ vọng: 'slot|hour|minute'");
        }
    }
    
    // Xử lý 2: Nhận tin nhắn chỉnh giờ RTC. Kỳ vọng chuỗi định dạng: "hour|minute|second" (Ví dụ: "15|30|00")
    else if (strcmp(topic, _instance->_topicSetRtc) == 0) {
        int parsedHour = 0, parsedMinute = 0, parsedSecond = 0;
        if (sscanf(buffer, "%d|%d|%d", &parsedHour, &parsedMinute, &parsedSecond) == 3) {
            _instance->_rtcHour = parsedHour;
            _instance->_rtcMinute = parsedMinute;
            _instance->_rtcSecond = parsedSecond;
            _instance->_hasRtcSync = true; // Dựng cờ hiệu RTC
        } else {
            Serial.println("[MQTT][ERROR] Sai định dạng cấu hình RTC! Kỳ vọng: 'hour|minute|second'");
        }
    }

    // Xử lý 3: Nhận tin nhắn yêu cầu Tare cân. Nội dung payload bất kỳ hoặc chữ "TARE"
    else if (strcmp(topic, _instance->_topicTare) == 0) {
        _instance->_hasTareCommand = true; // Chỉ cần nhận được tín hiệu vào topic này là dựng cờ ngay
    }
}

void MQTTManager::publishStatus(const char* message) {
    if (client.connected()) {
        client.publish("pillbox/status", message, true); 
        Serial.printf("[MQTT] Sent status update: %s\n", message);
    }
}