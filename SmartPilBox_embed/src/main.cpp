#include <Wire.h>
#include <RTClib.h>
#include <ESP32Servo.h>

RTC_DS1307 rtc;

#define SERVO_PIN 18
#define BUZZER_PIN 19
#define BUTTON_PIN 4

Servo lockServo;

bool alarmActive = false;
bool medicineTaken = false;
void triggerMedicineAlarm() {
  alarmActive = true;
  medicineTaken = false;

  Serial.println("TIME TO TAKE MEDICINE");

  // Unlock box
  lockServo.write(90);

  // Start buzzer
  digitalWrite(BUZZER_PIN, HIGH);
}

void stopAlarm() {
  alarmActive = false;

  // Stop buzzer
  digitalWrite(BUZZER_PIN, LOW);

  // Lock box again
  lockServo.write(0);

  Serial.println("Box Locked");
}

void setup() {
  Serial.begin(115200);

  // RTC
  Wire.begin(21, 22);

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1);
  }

  // Set RTC time once
  // Uncomment this line ONCE then upload
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Servo
  lockServo.attach(SERVO_PIN);

  // Locked position
  lockServo.write(0);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("System Ready");
}

void loop() {
  DateTime now = rtc.now();

  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());

  // TEST ALARM
  // Trigger every minute at second 10
  if (now.second() == 10 && !alarmActive) {
    triggerMedicineAlarm();
  }

  // Button pressed
  if (alarmActive && digitalRead(BUTTON_PIN) == LOW) {
    medicineTaken = true;

    Serial.println("Medicine Taken");

    stopAlarm();
  }

  delay(200);
}

