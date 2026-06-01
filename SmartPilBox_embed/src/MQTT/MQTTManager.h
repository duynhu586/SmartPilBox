#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>

typedef void (*TimeUpdateCallback)(int hour, int minute);
typedef void (*RTCUpdateCallback)(uint32_t epoch);

class MQTTManager {
public:
    MQTTManager();
    void begin(const char* ssid, const char* password, const char* brokerIp, int port);
    void update();
    void setOnTimeUpdate(TimeUpdateCallback cb);
    void setOnRTCUpdate(RTCUpdateCallback cb);
    void publishStatus(const char* message);
    bool connected() { return client.connected(); }

private:
    WiFiClient espClient;
    PubSubClient client;
    const char* _brokerIp;
    int _port;
    
    const char* _topicHour = "pillbox/set_hour";
    const char* _topicMinute = "pillbox/set_minute";
    
    TimeUpdateCallback _timeCb = nullptr;
    RTCUpdateCallback _rtcCb = nullptr;

    int _currentHour = 0; 
    int _currentMinute = 0;                // <-- THÊM: Lưu tạm phút
    volatile bool _hasNewSchedule = false; // <-- THÊM: Cờ hiệu báo có lịch mới

    uint32_t _newRTCVal = 0;
    volatile bool _hasNewRTC = false;

    void reconnect();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    static MQTTManager* _instance;

    unsigned long lastReconnectAttempt;
    bool reconnectNonBlocking(); 
};

#endif