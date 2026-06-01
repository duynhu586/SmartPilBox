#include "MQTTManager.h"
#include "../Config.h"
#include "esp_wifi.h"

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager() : client(espClient), lastReconnectAttempt(0) {
    _instance = this;
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

    // ================================================================
    // CHÈN LỆNH KÍCH HOẠT QUẢN LÝ NGUỒN WIFI TẠI ĐÂY
    // ================================================================
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Bật chế độ tiết kiệm điện Modem nhưng giữ kết nối
    Serial.println("[POWER] WiFi Modem Power Save Enabled.");
    // ================================================================

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

void MQTTManager::setOnRTCUpdate(RTCUpdateCallback cb) {
    _rtcCb = cb;
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

        // XỬ LÝ LỊCH MỚI TẠI LUỒNG CHÍNH (Chạy trên Core 1 - An toàn tuyệt đối)
        if (_hasNewSchedule) {
            _hasNewSchedule = false; // Hạ cờ hiệu
            
            Serial.printf("[MQTT] Luồng chính phát hiện lịch mới nhận từ mạng: %02d:%02d\n", _currentHour, _currentMinute);
            
            if (_currentHour >= 0 && _currentHour <= 23 && _currentMinute >= 0 && _currentMinute <= 59) {
                if (_timeCb != nullptr) {
                    _timeCb(_currentHour, _currentMinute); // Gọi cập nhật FSM một cách an toàn
                }
            }
        }

        // XỬ LÝ ĐỒNG BỘ RTC TẠI LUỒNG CHÍNH
        if (_hasNewRTC) {
            _hasNewRTC = false; // Hạ cờ hiệu
            Serial.printf("[MQTT] Luồng chính phát hiện lệnh đồng bộ RTC: %u\n", _newRTCVal);
            if (_newRTCVal > 0 && _rtcCb != nullptr) {
                _rtcCb(_newRTCVal);
            }
        }
    }
}

bool MQTTManager::reconnectNonBlocking() {
    Serial.print("[MQTT] Attempting connection to Local Broker...");
    
    // Sử dụng địa chỉ MAC của chip làm Client ID để tránh tuyệt đối việc trùng lặp Client trên Broker
    String clientId = "ESP32PillBox-" + WiFi.macAddress();
    clientId.replace(":", ""); 

    if (client.connect(clientId.c_str())) {
        Serial.println("Connected!");
        client.subscribe(_topicHour);
        client.subscribe(_topicMinute);
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

// CALLBACK CHẠY TRÊN CORE NẰM NGẦM (Core 0): Siêu tốc, chỉ lưu giá trị rồi thoát!
void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance == nullptr) return;

    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    int value = atoi(buffer); 

    if (strcmp(topic, _instance->_topicHour) == 0) {
        _instance->_currentHour = value; 
    } 
    else if (strcmp(topic, _instance->_topicMinute) == 0) {
        _instance->_currentMinute = value;
        _instance->_hasNewSchedule = true; // Dựng cờ hiệu báo cho hàm update() biết
    }
    else if (strcmp(topic, "pillbox/set_rtc") == 0) {
        _instance->_newRTCVal = strtoul(buffer, nullptr, 10);
        _instance->_hasNewRTC = true;
    }
}

void MQTTManager::publishStatus(const char* message) {
    if (client.connected()) {
        client.publish("pillbox/status", message, true); 
        Serial.printf("[MQTT] Sent status update: %s\n", message);
    }
}