#include "MQTTManager.h"
#include "../Config.h"
#include "esp_wifi.h"

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager() : client(espClient), lastReconnectAttempt(0) {
    _instance = this;
    _currentSlot = 0;
    _currentHour = SCHEDULE_HOUR; 
    _currentMinute = SCHEDULE_MINUTE;
    _hasNewSchedule = false;
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

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Bật chế độ tiết kiệm điện Modem nhưng giữ kết nối
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

void MQTTManager::setOnTimeUpdate(TimeUpdateCallback cb) {
    _timeCb = cb;
}

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

        // XỬ LÝ LỊCH MỚI TẠI LUỒNG CHÍNH (Core 1)
        if (_hasNewSchedule) {
            _hasNewSchedule = false; // Hạ cờ hiệu
            
            Serial.printf("[MQTT] Luồng chính phát hiện lịch mới cho Slot [%d]: %02d:%02d\n", 
                          _currentSlot, _currentHour, _currentMinute);
            
            // Validate dữ liệu trước khi đẩy vào FSM
            if (_currentHour >= 0 && _currentHour <= 23 && _currentMinute >= 0 && _currentMinute <= 59) {
                if (_timeCb != nullptr) {
                    // Gọi cập nhật FSM kèm theo chỉ số Slot
                    _timeCb(_currentSlot, _currentHour, _currentMinute); 
                }
            }
        }
    }
}

bool MQTTManager::reconnectNonBlocking() {
    Serial.print("[MQTT] Attempting connection to Local Broker...");
    
    String clientId = "ESP32PillBox-" + WiFi.macAddress();
    clientId.replace(":", ""); 

    if (client.connect(clientId.c_str())) {
        Serial.println("Connected!");
        
        // Đăng ký nhận lịch trình từ topic tích hợp mới
        client.subscribe(_topicSchedule);
        client.subscribe("pillbox/set_rtc");
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

// CALLBACK CHẠY TRÊN CORE NGẦM (Core 0): Tách chuỗi nhanh và dựng cờ hiệu
void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance == nullptr) return;

    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';

    // Kiểm tra nếu tin nhắn đến từ topic cấu hình lịch trình
    if (strcmp(topic, _instance->_topicSchedule) == 0) {
        int parsedSlot = 0, parsedHour = 0, parsedMinute = 0;
        
        // Phân tích cú pháp chuỗi dạng "slot|hour|minute" (Ví dụ: "0|08|00")
        if (sscanf(buffer, "%d|%d|%d", &parsedSlot, &parsedHour, &parsedMinute) == 3) {
            _instance->_currentSlot = parsedSlot;
            _instance->_currentHour = parsedHour;
            _instance->_currentMinute = parsedMinute;
            _instance->_hasNewSchedule = true; // Dựng cờ hiệu thông báo cho hàm update()
        } else {
            Serial.println("[MQTT][ERROR] Sai định dạng dữ liệu lịch trình! Kỳ vọng: 'slot|hour|minute'");
        }
    }
}

void MQTTManager::publishStatus(const char* message) {
    if (client.connected()) {
        client.publish("pillbox/status", message, true); 
        Serial.printf("[MQTT] Sent status update: %s\n", message);
    }
}