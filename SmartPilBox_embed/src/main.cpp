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

// #include <Arduino.h>
// #include "Servo/ServoManager.h"

// // Giả định các cấu hình chân và góc nếu bạn chưa định nghĩa trong Config.h
// #define SERVO_PIN 18          // Thay bằng chân GPIO bạn đang kết nối trên ESP32

// ServoManager servoMgr;

// void setup() {
//     // Khởi tạo Serial Baudrate cao để theo dõi log hệ thống nhanh nhất
//     Serial.begin(115200);
//     delay(1000); // Đợi Serial Monitor ổn định
    
//     Serial.println("\n=================================");
//     Serial.println("[TEST] Bắt đầu khởi tạo ServoManager...");
    
//     // Khởi tạo servo (Hàm begin đã tích hợp sẵn attach và lock)
//     servoMgr.begin(SERVO_PIN);
    
//     Serial.println("[TEST] Khởi tạo thành công! Chuẩn bị vào vòng lặp.");
//     Serial.println("=================================\n");
    
//     // Đợi 2 giây trước khi bắt đầu test vòng lặp
//     delay(2000);
// }

// void loop() {
//     // ----------------------------------------------------
//     // BƯỚC 1: TIẾN HÀNH MỞ (ATTACH -> OPEN -> DETACH)
//     // ----------------------------------------------------
//     Serial.println("[LOOP] ---> Kích hoạt quy trình MỞ KHÓA...");
    
//     Serial.println("[LOOP] Chờ attach()...");
//     servoMgr.attach(); 
    
//     Serial.println("[LOOP] Đang quay sang góc OPEN...");
//     servoMgr.open();
    
//     // Quan trọng: Phải đợi servo chạy hết hành trình cơ học trước khi ngắt xung PWM
//     Serial.println("[LOOP] Đang đợi servo quay xong (600ms)...");
//     delay(600); 
    
//     Serial.println("[LOOP] Chờ detach()...");
//     servoMgr.detach();
    
//     Serial.println("[LOOP] ĐÃ MỞ KHÓA VÀ DETACH AN TOÀN.");
//     Serial.println("---------------------------------");
    
//     // Giữ trạng thái mở trong 3 giây (Lúc này servo không bị ăn dòng, có thể dùng tay xoay nhẹ được)
//     delay(3000);


//     // ----------------------------------------------------
//     // BƯỚC 2: TIẾN HÀNH KHÓA (ATTACH -> LOCK -> DETACH)
//     // ----------------------------------------------------
//     Serial.println("[LOOP] ---> Kích hoạt quy trình ĐÓNG KHÓA...");
    
//     Serial.println("[LOOP] Chờ attach()...");
//     servoMgr.attach();
    
//     Serial.println("[LOOP] Đang quay sang góc LOCKED...");
//     servoMgr.lock();
    
//     // Đợi servo quay về vị trí ban đầu
//     Serial.println("[LOOP] Đang đợi servo quay xong (600ms)...");
//     delay(600);
    
//     Serial.println("[LOOP] Chờ detach()...");
//     servoMgr.detach();
    
//     Serial.println("[LOOP] ĐÃ KHÓA VÀ DETACH AN TOÀN.");
//     Serial.println("=================================\n");
    
//     // Giữ trạng thái khóa trong 3 giây trước khi lặp lại
//     delay(3000);
// }

// #include <Arduino.h>
// #include "LoadCell/LoadCellManager.h"

// #define DOUT_PIN 4
// #define SCK_PIN 5

// LoadCellManager loadCell;

// void setup() {
//     Serial.begin(115200);

//     Serial.println("=== INIT SCALE ===");

//     loadCell.begin(DOUT_PIN, SCK_PIN);

//     delay(2000);

//     Serial.println("Tare done.");
//     Serial.println("Remove all weight.");

//     delay(5000);

//     // Sleep HX711
//     loadCell.powerDown();

//     Serial.println("\nHX711 sleeping...");
//     Serial.println("NOW PLACE AN OBJECT ON THE LOAD CELL");

//     delay(10000);

//     // Wake HX711
//     loadCell.powerUp();

//     delay(1000);

//     Serial.println("\nReading weight after wakeup:");

//     for (int i = 0; i < 10; i++) {
//         float w = loadCell.getWeight();

//         Serial.print("Weight: ");
//         Serial.println(w);

//         delay(1000);
//     }

//     Serial.println("\nREMOVE OBJECT");

//     delay(10000);

//     Serial.println("\nPlace object AGAIN");

//     delay(10000);

//     Serial.println("\nReading second placement:");

//     for (int i = 0; i < 10; i++) {
//         float w = loadCell.getWeight();

//         Serial.print("Weight: ");
//         Serial.println(w);

//         delay(1000);
//     }
// }

// void loop() {
// }