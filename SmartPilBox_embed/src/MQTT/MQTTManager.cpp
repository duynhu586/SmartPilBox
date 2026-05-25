#include "MQTTManager.h"

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager() : client(espClient) {
    _instance = this;
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

    client.setServer(_brokerIp, _port);
    client.setCallback(mqttCallback);
}

void MQTTManager::setOnTimeUpdate(TimeUpdateCallback cb) {
    _timeCb = cb;
}

void MQTTManager::update() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}

void MQTTManager::reconnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting to Local Broker...");
        String clientId = "ESP32PillBox-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("Connected!");
            // Đăng ký lắng nghe cả 2 kênh
            client.subscribe(_topicHour);
            client.subscribe(_topicMinute);
        } else {
            Serial.print("Failed, rc="); Serial.print(client.state());
            Serial.println(" Try again in 5 seconds");
            delay(5000);
        }
    }
}

// Xử lý dữ liệu số nguyên đổ về từ Server
void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance == nullptr || _instance->_timeCb == nullptr) return;

    // Tạo một chuỗi tạm an toàn từ payload để ép kiểu sang số nguyên
    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    int value = atoi(buffer); 

    // Kiểm tra xem dữ liệu thuộc kênh nào
    if (strcmp(topic, _instance->_topicHour) == 0) {
        _instance->_currentHour = value; // Lưu tạm giá trị giờ vào biến thành viên
        Serial.printf("[MQTT] Received Hour: %d\n", value);
    } 
    else if (strcmp(topic, _instance->_topicMinute) == 0) {
        Serial.printf("[MQTT] Received Minute: %d\n", value);
        // Khi nhận được phút, kích hoạt callback gửi cả cặp (Giờ đã lưu trước đó, Phút vừa nhận) về main.cpp
        _instance->_timeCb(_instance->_currentHour, value); 
    }
}

void MQTTManager::publishStatus(const char* message) {
    if (client.connected()) {
        // Bắn dữ liệu trạng thái lên topic pillbox/status, bật cờ retain=true để lưu lại trạng thái mới nhất
        client.publish("pillbox/status", message, true); 
        Serial.printf("[MQTT] Sent status update: %s\n", message);
    }
}