#ifndef CONFIG_H
#define CONFIG_H

// --- Pin Assignments ---
#define OLED_SDA_PIN        32  // Bộ Wire mặc định (I2C 0)
#define OLED_SCL_PIN        33  // Bộ Wire mặc định (I2C 0)

#define BUZZER_PIN          25  

#define RTC_SDA_PIN         22  // Bộ Wire1 độc lập (I2C 1)
#define RTC_SCL_PIN         23  // Bộ Wire1 độc lập (I2C 1)

#define SERVO_PIN           14  
#define HX711_DOUT_PIN      27  
#define HX711_SCK_PIN       26  
#define IR_SENSOR_PIN       18  
#define IR_POWER_PIN        19  // Chân cấp nguồn cho cảm biến IR

// --- System Thresholds & Constants ---
#define HX711_CALIBRATION_FACTOR    420.0  
#define MEDICINE_WEIGHT_THRESHOLD   2       
#define CONTAINER_REMOVED_THRESHOLD 15.0   

// --- Servo Angle Parameters ---
#define SERVO_LOCKED_ANGLE          0      
#define SERVO_OPEN_ANGLE            90     

// --- Daily Schedule Configuration (24h) ---
#define SCHEDULE_HOUR               16      
#define SCHEDULE_MINUTE             48      

// --- Timezone Configuration ---
#define TIMEZONE_OFFSET             7       // Việt Nam (UTC+7)

constexpr unsigned long IR_DEBOUNCE_TIME = 200;
constexpr unsigned long LID_CLOSE_CONFIRMATION_TIME = 2000;

#define BUZZER_FREQUENCY 800   
#define BUZZER_BEEP_INTERVAL 500 
#define ALARM_RINGING_TIMEOUT       1000   
#define WEIGHT_STABILIZATION_TIME   1500   

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
