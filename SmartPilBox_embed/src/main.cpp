#include <Arduino.h>
#include "PilBox/PillBoxController.h"
#include "MQTT/MQTTManager.h"

PillBoxController pillBox;
MQTTManager mqttManager;

// Hàm callback khi nhận được cấu hình giờ mới từ MQTT Server (đã được Server tách sẵn hoặc bóc tách số)
void onTimeConfigReceived(int hour, int minute) {
    // Đẩy thẳng hai số nguyên giờ/phút vừa nhận vào bộ điều khiển trung tâm
    pillBox.setAlarmTime(hour, minute); 
}

void setup() {
    pillBox.begin();
    
    // Khởi tạo mạng MQTT (Điền thông tin mạng và IP Mosquitto máy tính của bạn)
    mqttManager.begin("Ten_WiFi", "Mat_Khau", "192.168.1.15", 1883);
    mqttManager.setOnTimeUpdate(onTimeConfigReceived);
}

void loop() {
    mqttManager.update(); // Duy trì kết nối mạng nhận lệnh cài giờ hằng ngày
    pillBox.update();     // Chạy máy trạng thái FSM lõi phần cứng của hộp thuốc
    delay(10);
}