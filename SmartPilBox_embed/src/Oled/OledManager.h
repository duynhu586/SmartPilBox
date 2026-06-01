#ifndef OLED_MANAGER_H
#define OLED_MANAGER_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "../Config.h"

class OledManager {
private:
    Adafruit_SSD1306 display;

public:
    OledManager();
    
    // Khởi tạo màn hình
    bool begin();
    
    // Xóa toàn bộ màn hình
    void clear();
    
    // Đẩy dữ liệu từ bộ đệm lên màn hình thực tế (gọi sau khi vẽ xong)
    void show();
    
    // Hàm vạn năng: Hiển thị chữ căn giữa tại tọa độ Y bất kỳ với kích thước tùy chọn
    void printCenter(const char* text, int y, int textSize = 1);
    
    // Hàm vẽ nhanh giao diện tiêu chuẩn gồm: 1 Tiêu đề (Header) và 2 dòng nội dung bên dưới
    void displayStatus(const char* header, const char* line1, const char* line2 = "", const char* line3 = "");
};

#endif // OLED_MANAGER_H