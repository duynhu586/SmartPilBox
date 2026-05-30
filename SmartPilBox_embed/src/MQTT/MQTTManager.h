#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>

// Thay đổi callback: Nhận thêm chỉ số Slot (vị trí lịch trình trong mảng)
typedef void (*TimeUpdateCallback)(int slot, int hour, int minute);

class MQTTManager {
public:
    MQTTManager();
    void begin(const char* ssid, const char* password, const char* brokerIp, int port);
    void update();
    void setOnTimeUpdate(TimeUpdateCallback cb);
    void publishStatus(const char* message);
    bool connected() { return client.connected(); }

private:
    WiFiClient espClient;
    PubSubClient client;
    const char* _brokerIp;
    int _port;
    
    // Đổi sang 1 topic quản lý lịch trình chung
    const char* _topicSchedule = "pillbox/set_schedule";
    
    TimeUpdateCallback _timeCb = nullptr;

    // Các biến lưu trữ tạm thời nhận từ luồng ngầm mạng (Core 0)
    int _currentSlot = 0;   // <-- THÊM: Lưu tạm index của lịch trình
    int _currentHour = 0; 
    int _currentMinute = 0; 
    volatile bool _hasNewSchedule = false; 

    void reconnect();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    static MQTTManager* _instance;

    unsigned long lastReconnectAttempt;
    bool reconnectNonBlocking(); 
};

#endif