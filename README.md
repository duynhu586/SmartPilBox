# SmartPilBox

**Hộp thuốc thông minh hỗ trợ y tế từ xa** — dự án IoT cho học phần *From Sensor to User* (Master M1 ICT), với hướng mở rộng *Security and Ethics for Data*.

---

## Mục lục

- [Tổng quan](#tổng-quan)
- [Vấn đề & giải pháp](#vấn-đề--giải-pháp)
- [Kiến trúc hệ thống](#kiến-trúc-hệ-thống)
- [Ứng dụng người dùng (Prototype MVP)](#ứng-dụng-người-dùng-prototype-mvp)
- [Phần cứng](#phần-cứng)
- [Sơ đồ chân ESP32](#sơ-đồ-chân-esp32)
- [Luồng vận hành](#luồng-vận-hành)
- [Lắp ráp cơ khí](#lắp-ráp-cơ-khí)
- [Phần mềm & cấu trúc repo](#phần-mềm--cấu-trúc-repo)
- [Cài đặt & chạy thử](#cài-đặt--chạy-thử)
- [An ninh & đạo đức dữ liệu](#an-ninh--đạo-đức-dữ-liệu)

---

## Tổng quan

**SmartPilBox** (Smart Pillbox / Medication Tracker) là hộp đựng thuốc IoT giúp người cao tuổi uống thuốc đúng giờ, đúng liều, đồng thời cho phép người thân hoặc nhân viên y tế **giám sát từ xa** qua dữ liệu cảm biến thời gian thực.

Hệ thống kết hợp:

- **Khóa cơ học** (Servo) — chỉ mở nắp khi đến giờ, hạn chế uống quá liều hoặc trẻ em mở nhầm
- **Cân tải trọng** (Loadcell + HX711) — xác nhận thuốc đã được lấy ra
- **Cảm biến hồng ngoại** — phát hiện tay thò vào khay
- **RTC** — lịch uống thuốc chính xác kể cả khi mất WiFi
- **OLED, Buzzer, DHT11** — hiển thị, cảnh báo và giám sát môi trường bảo quản

> **Trạng thái dự án:** Đang phát triển — firmware cơ bản (RTC, Servo, Buzzer) trong `SmartPilBox_embed/`; tích hợp Loadcell, IR, MQTT/Cloud theo lộ trình nhóm.

---

## Vấn đề & giải pháp

| Thách thức | Cách SmartPilBox xử lý |
|------------|-------------------------|
| Quên uống thuốc (Alzheimer, trí nhớ suy giảm) | Buzzer + OLED + mở khóa đúng giờ (RTC) |
| Uống quá liều (double-dosing) | Khóa nắp; chỉ mở theo lịch hoặc nút dự phòng |
| Người thân ở xa, khó giám sát | Đẩy trạng thái (thời gian, mở/đóng, khối lượng) lên Cloud / Telegram |
| Chỉ mở nắp mà không lấy thuốc | So sánh khối lượng trước/sau; cảnh báo nếu Δm ≈ 0 |

---

## Kiến trúc hệ thống

```mermaid
flowchart LR
  subgraph Device["Thiết bị ESP32"]
    RTC[RTC DS3231]
    DHT[DHT11]
    LC[Loadcell + HX711]
    IR[Cảm biến IR]
    SRV[Servo khóa]
    OLED[OLED I2C]
    BZ[Buzzer]
  end

  subgraph Cloud["Cloud / Backend"]
    MQTT[MQTT / HTTP]
    DB[(Lưu trữ sự kiện)]
  end

  subgraph User["Người dùng cuối"]
    DASH[Dashboard]
    TG[Telegram Bot]
  end

  RTC --> Device
  LC --> Device
  IR --> Device
  Device -->|WiFi| MQTT
  MQTT --> DB
  DB --> DASH
  DB --> TG
```

**Luồng dữ liệu (*From Sensor to User*):** Cảm biến → ESP32 (xử lý & quyết định) → mạng → Cloud → Dashboard / Telegram cho người giám hộ.

---

## Ứng dụng người dùng (Prototype MVP)

Trong giai đoạn prototype (1–2 tuần), ứng dụng chỉ tập trung vào các chức năng tối thiểu để chứng minh hệ thống SmartPilBox hoạt động hoàn chỉnh.

Mục tiêu chính:
- cài lịch uống thuốc,
- gửi lịch sang ESP32 qua MQTT,
- nhận trạng thái realtime từ ESP32,
- hiển thị người dùng đã uống thuốc hay chưa.

Hệ thống sử dụng MQTT để giao tiếp realtime giữa ứng dụng và ESP32.

---

### Kiến trúc hệ thống

```mermaid
flowchart LR
  MobileApp --> MQTT[(MQTT Broker)]
  MQTT --> ESP32
  ESP32 --> MQTT
  MQTT --> MobileApp
```

### Luồng hoạt động

1. Người dùng nhập giờ uống thuốc trên app.
2. App publish thời gian lên MQTT Broker.
3. ESP32 subscribe và cập nhật lịch uống thuốc.
4. Khi đến giờ:
   - buzzer reo,
   - servo mở khóa.
5. Sau khi xác nhận uống thuốc:
   - ESP32 publish trạng thái lên MQTT.
6. App nhận dữ liệu realtime và cập nhật giao diện.

---

### Chức năng chính của app

#### 1. Đặt lịch uống thuốc

Người dùng nhập:
- giờ,
- phút uống thuốc.

App publish MQTT message:

Topic:

```text
pillbox/hour
```

Payload ví dụ:

```text
8
```

và:

Topic:

```text
pillbox/minute
```

Payload ví dụ:

```text
30
```

ESP32 sẽ cập nhật lịch thành:

```text
08:30
```

---

#### 2. Xem trạng thái hộp thuốc

ESP32 publish trạng thái realtime lên:

Topic:

```text
pillbox/status
```

Ví dụ payload:

```text
Medicine Taken
```

hoặc:

```text
Waiting for User
```

App hiển thị trạng thái đơn giản:

```text
SMARTPILBOX

Schedule: 08:30

Current Status:
✓ Medicine Taken
```

---

#### 3. Cảnh báo đơn giản

Nếu:
- đến giờ nhưng chưa uống thuốc,
- hoặc mở hộp nhưng không có thay đổi khối lượng,

ESP32 sẽ publish trạng thái cảnh báo.

Ví dụ:

```text
Medicine Not Taken
```

App hiển thị cảnh báo màu đỏ hoặc popup đơn giản.

---

### MQTT Topics

| Topic | Publisher | Subscriber | Mục đích |
|-------|------------|-------------|----------|
| `pillbox/hour` | Mobile App | ESP32 | Gửi giờ uống thuốc |
| `pillbox/minute` | Mobile App | ESP32 | Gửi phút uống thuốc |
| `pillbox/status` | ESP32 | Mobile App | Gửi trạng thái hộp thuốc |

---

### Công nghệ đề xuất

| Thành phần | Gợi ý |
|------------|------|
| Mobile App | Flutter |
| MQTT Client | mqtt_client package |
| MQTT Broker | Mosquitto / EMQX / HiveMQ |

---

### Scope prototype

Prototype chỉ cần:
- 1 thiết bị,
- 1 người dùng,
- realtime MQTT,
- không cần database,
- không cần authentication,
- không cần backend server riêng.

Mục tiêu chính:
- chứng minh luồng IoT realtime hoạt động hoàn chỉnh từ:

```text
Sensor → ESP32 → MQTT → Mobile App
```

## Phần cứng

### Linh kiện chính

| STT | Linh kiện | Vai trò |
|-----|-----------|---------|
| 1 | **ESP32 DevKit** | Vi điều khiển, WiFi (có sẵn trong học phần) |
| 2 | **OLED I2C** | Hiển thị giờ, trạng thái, cảnh báo |
| 3 | **RTC DS3231** | Đồng hồ thời gian thực khi mất mạng |
| 4 | **DHT11** | Nhiệt độ / độ ẩm bảo quản thuốc |
| 5 | **Servo SG90** | Khóa / mở nắp hộp |
| 6 | **Loadcell 1 kg + HX711** | Đo khối lượng thuốc còn lại |
| 7 | **Cảm biến IR** | Phát hiện tay vào khay |
| 8 | **Buzzer** | Cảnh báo âm thanh |
| 9 | **Nút bấm** | Xác nhận / fail-safe khi mất mạng |
| 10 | **Hộp nhựa + vách Mica** | Cơ khí, ngăn thuốc / ngăn mạch |

---

## Sơ đồ chân ESP32

| Linh kiện | Giao tiếp | GPIO (gợi ý) | Ghi chú |
|-----------|----------|--------------|---------|
| OLED | I2C SDA / SCL | **21** / **22** | Chung bus với RTC |
| RTC DS3231 | I2C SDA / SCL | **21** / **22** | Đấu song song OLED |
| HX711 | DT / SCK | **19** / **18** | Tránh chân strapping lúc boot |
| Servo SG90 | PWM | **13** | Thư viện `ESP32Servo` |
| Cảm biến IR | Digital IN | **14** | HIGH/LOW khi có vật cản |
| Buzzer | PWM / DAC | **25** | Âm thanh cảnh báo |
| Button | Digital IN | **12** | `INPUT_PULLUP` |

> Pin trong firmware hiện tại (`main.cpp`) có thể khác trong giai đoạn prototype — đồng bộ lại theo bảng trên khi hoàn thiện board.

---

## Luồng vận hành

1. **Chờ** — Servo khóa nắp; OLED hiển thị giờ (RTC) và môi trường (DHT11).
2. **Đến giờ uống thuốc** — Buzzer reo, LED nháy (nếu có), Servo mở khóa; OLED: *"Đến giờ uống thuốc"*.
3. **Người dùng mở nắp & lấy thuốc** — IR phát hiện tay; lưu khối lượng \(W_1\) trước khi lấy.
4. **Xác thực** — Sau khi đóng nắp, đợi ổn định ~3 s, đọc \(W_2\):
   - Nếu \(W_1 - W_2 > 2\,\text{g}\) (ngưỡng cấu hình) → **Đã uống đúng liều** → khóa lại → gửi Cloud *Thành công*.
   - Nếu nắp/IR có tương tác nhưng \(\Delta m \approx 0\) → **Cảnh báo: chưa lấy thuốc** → tiếp tục Buzzer → gửi Cloud *Cảnh báo*.
5. **End-user** — Con cái / bác sĩ nhận thông báo qua Dashboard hoặc Telegram.

---

## Lắp ráp cơ khí

Hộp chia **2 ngăn**: ngăn thuốc ~14×15 cm, ngăn mạch ~6×15 cm.

### Tấm cắt (Mica 2–3 mm)

| Miếng | Kích thước | Mục đích |
|-------|------------|----------|
| Vách ngăn | 14,6 × 7,6 cm | Chia hộp; gắn Servo |
| Khay thuốc | 12 × 13 cm | Đặt trên Loadcell (hở ~1 cm quanh viền) |
| Đệm Loadcell ×2 | 2 × 3 cm | Sandwich loadcell — không chạm đáy/vách |

### Thứ tự lắp (tóm tắt)

1. **Sandwich Loadcell** — đệm dưới → loadcell → đệm trên → khay thuốc; kiểm tra khay nhún, không chạm thành hộp.
2. **Vách + Servo** — rãnh ~1,2×2,3 cm trên vách; cánh Servo khóa qua lỗ trên nắp.
3. **IR** — trên vách, hướng 45° xuống khay.
4. **Nắp** — lỗ OLED 3,5×2,2 cm; lỗ nút & thoát âm Buzzer.

Chi tiết đo đạc và mẹo đi dây: xem tài liệu nội bộ `project_context.md` (không đưa lên git).

---

## Phần mềm & cấu trúc repo

```
SmartPilBox/
├── README.md                 # Tài liệu dự án (file .md duy nhất trên git)
├── .gitignore
├── project_context.md        # Báo cáo / thiết kế nội bộ (local, không push)
└── SmartPilBox_embed/        # Firmware ESP32 (PlatformIO)
    ├── platformio.ini
    ├── src/main.cpp
    ├── diagram.json          # Mô phỏng Wokwi (nếu dùng)
    └── wokwi.toml
```

| Công nghệ | Mục đích |
|-----------|----------|
| **PlatformIO** + **Arduino** | Build & upload firmware ESP32 |
| **RTClib** | Đọc / cài giờ RTC |
| **ESP32Servo** | Điều khiển khóa |
| *Dự kiến* MQTT / HTTP, Telegram Bot API | Truyền dữ liệu lên Cloud |

---

## Cài đặt & chạy thử

### Yêu cầu

- [PlatformIO](https://platformio.org/) (VS Code extension hoặc CLI)
- Board **ESP32 DOIT DevKit V1**
- Cáp USB, driver CP210/CH340

### Build & upload

```bash
cd SmartPilBox_embed
pio run -t upload
pio device monitor
```

### Cài giờ RTC (một lần)

Trong `src/main.cpp`, bỏ comment dòng `rtc.adjust(...)` (hoặc tương đương cho DS3231), upload **một lần**, sau đó comment lại để tránh reset giờ mỗi lần nạp.

### Mô phỏng (tuỳ chọn)

```bash
cd SmartPilBox_embed
pio run
# Hoặc mở project trong Wokwi theo diagram.json / wokwi.toml
```

---

## An ninh & đạo đức dữ liệu

Hướng nghiên cứu cho học phần tiếp theo (không nằm trong scope firmware giai đoạn 2):

| Chủ đề | Nội dung |
|--------|----------|
| **Tính toàn vẹn** | Rủi ro MITM trên MQTT — giả trạng thái "đã uống" |
| **Kiểm soát truy cập** | TLS trên ESP32; nút cơ học fail-safe khi mất mạng |
| **Quyền riêng tư** | Dữ liệu tuân thủ uống thuốc — nhạy cảm (Nghị định 13/2023/NĐ-CP) |
| **Trách nhiệm** | Sai số Loadcell → hậu quả sức khỏe; retention policy |

---

## Giấy phép & disclaimer

Dự án học thuật — **không thay thế tư vấn y tế**. Sản phẩm prototype; cần hiệu chuẩn cân và kiểm thử trước khi dùng thực tế cho người bệnh.

---
