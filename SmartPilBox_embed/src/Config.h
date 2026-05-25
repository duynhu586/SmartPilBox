#ifndef CONFIG_H
#define CONFIG_H

// --- Pin Assignments ---
#define SERVO_PIN         18
#define BUZZER_PIN        19
#define HX711_DOUT_PIN    4
#define HX711_SCK_PIN     5
#define RTC_SDA_PIN       21
#define RTC_SCL_PIN       22
#define IR_SENSOR_PIN     23
#define IR_POWER_PIN     12  // Chân cấp nguồn cho cảm biến IR

// --- System Thresholds & Constants ---
#define HX711_CALIBRATION_FACTOR    420.0  // Custom load cell scaling factor
#define MEDICINE_WEIGHT_THRESHOLD   5.0    // Required delta mass reduction (grams) to pass verification
#define CONTAINER_REMOVED_THRESHOLD 15.0   // Delta reduction (grams) signifying container displacement

// --- Servo Angle Parameters ---
#define SERVO_LOCKED_ANGLE          0      // Vault locked orientation
#define SERVO_OPEN_ANGLE            90     // Vault open orientation

// --- Audio Configuration ---
#define BUZZER_BEEP_INTERVAL        500    // Non-blocking pulse cycle duration (milliseconds)

// --- Daily Schedule Configuration (24h) ---
#define SCHEDULE_HOUR               16      // Scheduled Hour (0-23)
#define SCHEDULE_MINUTE             48      // Scheduled Minute (0-59)

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