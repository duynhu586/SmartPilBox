#include <Arduino.h>
#include "PilBox/PillBoxController.h"

// Khởi tạo đối tượng điều khiển toàn cục (Global Instance)
// Việc để toàn cục giúp hàm Callback của MQTT (ở PillBoxController::begin) 
// có thể tìm thấy và cập nhật lịch hẹn giờ động thông qua từ khóa 'extern'
PillBoxController controller;

void setup() {
    // Cấu hình ban đầu cho toàn bộ hệ thống hộp thuốc
    // Hàm này sẽ tự động thiết lập chân Pin, cấu hình RTC, Cân HX711, Servo, Buzzer, IR
    // và kích hoạt chế độ tiết kiệm điện mặc định cho các linh kiện ngoại vi.
    controller.begin();
    
    Serial.println("[MAIN] Setup completed. State Machine enters IDLE core loop.");
}

void loop() {
    // Chạy liên tục bộ điều khiển trạng thái (Finite State Machine)
    // - Khi ở trạng thái IDLE: Hàm này tự động gọi Light Sleep 500ms để tiết kiệm pin.
    // - Khi đến giờ hẹn: Tự động đánh thức hệ thống để reo chuông, kiểm tra cân nặng và an toàn IR.
    controller.update();
}