#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <functional> // Thêm thư viện này để dùng std::function

// Khai báo các kiểu dữ liệu Callback
typedef std::function<void(int, int, int)> TimeUpdateCallback; // slot, hour, minute
typedef std::function<void(int, int, int)> RtcSyncCallback;   // hour, minute, second
typedef std::function<void()> TareCommandCallback;             // hàm trống để tare cân

class MQTTManager {
private:
    static MQTTManager* _instance;
    WiFiClient espClient;
    PubSubClient client;
    unsigned long lastReconnectAttempt;

    const char* _brokerIp;
    int _port;
    const char* _topicSchedule = "pillbox/schedule";
    const char* _topicSetRtc = "pillbox/set_rtc";
    const char* _topicTare = "pillbox/tare";

    // --- Biến lưu trữ cho Lịch trình ---
    int _currentSlot;
    int _currentHour;
    int _currentMinute;
    bool _hasNewSchedule;
    TimeUpdateCallback _timeCb;

    // --- Biến lưu trữ cho Cấu hình RTC mới ---
    int _rtcHour;
    int _rtcMinute;
    int _rtcSecond;
    bool _hasRtcSync;
    RtcSyncCallback _rtcCb;

    // --- Biến lưu trữ cho Lệnh Tare mới ---
    bool _hasTareCommand;
    TareCommandCallback _tareCb;

    bool reconnectNonBlocking();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

public:
    MQTTManager();
    void begin(const char* ssid, const char* password, const char* brokerIp, int port);
    void update();
    void reconnect();
    void publishStatus(const char* message);

    // Các hàm đăng ký Callback kết nối với PillBoxController
    void setOnTimeUpdate(TimeUpdateCallback cb);
    void setOnRtcSync(RtcSyncCallback cb);        // Hàm mới
    void setOnTareCommand(TareCommandCallback cb);  // Hàm mới
};

#endif