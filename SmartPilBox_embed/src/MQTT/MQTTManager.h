#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>

typedef void (*TimeUpdateCallback)(int hour, int minute);

class MQTTManager {
public:
    MQTTManager();
    void begin(const char* ssid, const char* password, const char* brokerIp, int port);
    void update();
    void setOnTimeUpdate(TimeUpdateCallback cb);
    void publishStatus(const char* message);

private:
    WiFiClient espClient;
    PubSubClient client;
    const char* _brokerIp;
    int _port;
    
    // Đăng ký nghe cả 2 topic riêng biệt từ Server
    const char* _topicHour = "pillbox/set_hour";
    const char* _topicMinute = "pillbox/set_minute";
    
    TimeUpdateCallback _timeCb = nullptr;

    // THÊM BIẾN NÀY ĐỂ HẾT LỖI: Lưu tạm giờ trước khi ghép với phút
    int _currentHour = 0; 

    void reconnect();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    static MQTTManager* _instance;

    
};

#endif