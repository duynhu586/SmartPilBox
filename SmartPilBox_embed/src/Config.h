#ifndef CONFIG_H
#define CONFIG_H

// --- Pin Assignments ---
// --- CẤU HÌNH SƠ ĐỒ CHÂN MỚI (TÁCH RỜI HOÀN TOÀN & AN TOÀN) ---

#define OLED_SDA_PIN        32  // Đã đổi: Chuyển từ 12 sang 32
#define OLED_SCL_PIN        33  // Đã đổi: Chuyển từ 13 sang 33

#define BUZZER_PIN          25  // Đã đổi: Chuyển từ 15 sang 25

#define RTC_SDA_PIN         22  // Giữ nguyên
#define RTC_SCL_PIN         23  // Giữ nguyên

#define SERVO_PIN           14  // Giữ nguyên
#define HX711_DOUT_PIN      27  // Giữ nguyên
#define HX711_SCK_PIN       26  // Giữ nguyên
#define IR_SENSOR_PIN       18  // Giữ nguyên
#define IR_POWER_PIN        19  // Giữ nguyên  // Chân cấp nguồn cho cảm biến IR

// --- System Thresholds & Constants ---
#define HX711_CALIBRATION_FACTOR    420.0  // Custom load cell scaling factor
#define MEDICINE_WEIGHT_THRESHOLD   2       // Required delta mass reduction (grams) to pass verification
#define CONTAINER_REMOVED_THRESHOLD 15.0   // Delta reduction (grams) signifying container displacement

// --- Servo Angle Parameters ---
#define SERVO_LOCKED_ANGLE          0      // Vault locked orientation
#define SERVO_OPEN_ANGLE            90     // Vault open orientation

// --- Daily Schedule Configuration (24h) ---
#define SCHEDULE_HOUR               16      // Scheduled Hour (0-23)
#define SCHEDULE_MINUTE             48      // Scheduled Minute (0-59)

// --- Timezone Configuration ---
#define TIMEZONE_OFFSET             7       // Việt Nam (UTC+7)


constexpr unsigned long IR_DEBOUNCE_TIME = 200;
constexpr unsigned long LID_CLOSE_CONFIRMATION_TIME = 2000;


#define BUZZER_FREQUENCY 800   // Giảm từ 2000 xuống 800Hz → nhỏ hơn
#define BUZZER_BEEP_INTERVAL 500 // Thời gian giữa các lần reo chuông (ms)
#define ALARM_RINGING_TIMEOUT       1000   // Thời gian reo chuông trước khi mở hộp (ms)
#define WEIGHT_STABILIZATION_TIME   1500   // Thời gian chờ cân ổn định để xác định (ms)

// --- OLED Custom Pin Assignments ---


// --- OLED Display Configuration ---
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define OLED_ADDRESS        0x3C

// --- State Machine Declarations ---
enum PillBoxState {
    IDLE,
    ALARM_RINGING,
    BOX_OPEN,
    WAIT_FOR_RETURN,
    VERIFY_MEDICINE,
    BOX_CLOSING,
    COMPLETED
};

#endif