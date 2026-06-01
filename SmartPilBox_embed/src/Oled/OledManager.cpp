#include "OledManager.h"
#include <Wire.h>

// Sử dụng bộ I2C số 1 trên chân 16 và 17 như đã cấu hình
TwoWire I2COled = TwoWire(1); 

OledManager::OledManager() 
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2COled, OLED_RESET) {}

bool OledManager::begin() {
    I2COled.begin(OLED_SDA_PIN, OLED_SCL_PIN, 400000);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        return false;
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
    return true;
}

void OledManager::clear() {
    display.clearDisplay();
}

void OledManager::show() {
    display.display();
}

void OledManager::printCenter(const char* text, int y, int textSize) {
    if (text == nullptr || strlen(text) == 0) return;
    
    display.setTextSize(textSize);
    
    int16_t x1, y1;
    uint16_t w, h;
    // Đo kích thước chuỗi text để tính toán vị trí X nằm chính giữa màn hình
    display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    if (x < 0) x = 0;
    
    display.setCursor(x, y);
    display.print(text);
}

void OledManager::displayStatus(const char* header, const char* line1, const char* line2, const char* line3) {
    display.clearDisplay();
    
    // 1. Vẽ Header (Tiêu đề trên cùng)
    if (header != nullptr && strlen(header) > 0) {
        printCenter(header, 0, 1);
        display.drawFastHLine(0, 11, SCREEN_WIDTH, SSD1306_WHITE); // Đường kẻ phân cách
    }
    
    // 2. Vẽ các dòng nội dung với tọa độ Y được tính toán sẵn
    printCenter(line1, 20, 1);
    printCenter(line2, 35, 1);
    printCenter(line3, 50, 1);
    
    display.display();
}