import paho.mqtt.client as mqtt
import time
from datetime import datetime

# --- CẤU HÌNH ---
MQTT_BROKER = "192.168.2.28" 
MQTT_PORT = 1883
TOPIC_SET_RTC = "pillbox/set_rtc"

def send_rtc_time(hour_val, minute_val, second_val):
    """Đóng gói thời gian thành định dạng 'hour|minute|second' và gửi qua MQTT"""
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    
    try:
        print(f"[Connecting] Đang kết nối tới Broker {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        client.loop_start()
        time.sleep(0.2)
        
        # Đóng gói định dạng theo ESP32 sscanf chờ đợi: "hour|minute|second"
        payload_str = f"{hour_val}|{minute_val}|{second_val}"
        
        print(f"[Publish] Gửi payload thời gian: '{payload_str}' tới topic '{TOPIC_SET_RTC}'")
        res = client.publish(TOPIC_SET_RTC, payload=payload_str, qos=1)
        res.wait_for_publish()
        
        print("[Success] ✅ Đã gửi cấu hình cập nhật giờ RTC thành công!")
        
        client.loop_stop()
        client.disconnect()
        
    except Exception as e:
        print(f"[Error] ❌ Không thể kết nối tới Broker: {e}")

if __name__ == "__main__":
    print("=== CẬP NHẬT / CHỈNH GIỜ RTC CHO PILLBOX ===")
    print("1. Tự động lấy giờ hiện tại của Máy Tính gửi sang ESP32")
    print("2. Nhập giờ thủ công bằng tay (HH:MM:SS)")
    
    choice = input("Chọn phương thức (1 hoặc 2): ").strip()
    
    if choice == "1":
        now = datetime.now()
        print(f"-> Lấy giờ máy tính: {now.strftime('%H:%M:%S')}")
        send_rtc_time(now.hour, now.minute, now.second)
        
    elif choice == "2":
        try:
            time_input = input("Nhập giờ thủ công (Định dạng HH:MM:SS, ví dụ 16:45:00): ").strip()
            parts = time_input.split(":")
            
            if len(parts) == 3:
                hour = int(parts[0])
                minute = int(parts[1])
                second = int(parts[2])
                
                if 0 <= hour <= 23 and 0 <= minute <= 59 and 0 <= second <= 59:
                    send_rtc_time(hour, minute, second)
                else:
                    print("❌ Giá trị thời gian nhập vào không hợp lệ (Giờ: 0-23, Phút/Giây: 0-59)!")
            else:
                print("❌ Sai định dạng! Phải nhập đủ 3 thành phần ngăn cách bởi dấu hai chấm (HH:MM:SS).")
        except ValueError:
            print("❌ Lỗi dữ liệu! Vui lòng chỉ sử dụng ký tự số và dấu ':'")
    else:
        print("❌ Lựa chọn không hợp lệ.")