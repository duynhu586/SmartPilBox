import paho.mqtt.client as mqtt
import time

# --- CẤU HÌNH ---
# Giữ nguyên IP hiện tại của bạn
MQTT_BROKER = "10.10.15.97" 
MQTT_PORT = 1883

# THAY ĐỔI: Tách thành 2 topic riêng biệt như ESP32 đang chờ
TOPIC_HOUR = "pillbox/set_hour"
TOPIC_MINUTE = "pillbox/set_minute"

def send_medicine_time(hour_val, minute_val):
    """Hàm tự động bóc tách số và gửi qua 2 kênh độc lập"""
    # Khai báo chuẩn API VERSION2 để tránh lỗi cảnh báo của thư viện
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    
    try:
        print(f"[Connecting] Đang kết nối tới Broker {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        client.loop_start()
        time.sleep(0.2)  # Đợi một chút để thiết lập kết nối nền ổn định
        
        # 1. Gửi GIỜ trước
        print(f"[Publish] Gửi GIỜ: '{hour_val}' tới topic '{TOPIC_HOUR}'")
        res_hour = client.publish(TOPIC_HOUR, payload=str(hour_val), qos=1)
        res_hour.wait_for_publish() # Chờ Broker xác nhận đã nhận được Giờ
        
        # Nghỉ một chút giữa 2 lần gửi để tránh nghẽn luồng nhận dữ liệu của ESP32
        time.sleep(0.1) 
        
        # 2. Gửi PHÚT sau (Khi ESP32 nhận được cái này, nó sẽ tự kích hoạt đổi giờ)
        print(f"[Publish] Gửi PHÚT: '{minute_val}' tới topic '{TOPIC_MINUTE}'")
        res_min = client.publish(TOPIC_MINUTE, payload=str(minute_val), qos=1)
        res_min.wait_for_publish() # Chờ Broker xác nhận đã nhận được Phút
        
        print("[Success] ✅ Đã cập nhật toàn bộ lịch uống thuốc mới thành công!")
        
        client.loop_stop()
        client.disconnect()
        
    except Exception as e:
        print(f"[Error] ❌ Không thể kết nối tới Broker: {e}")
        print("Vui lòng đảm bảo dịch vụ Mosquitto trên máy tính vẫn đang ở trạng thái Running.")

if __name__ == "__main__":
    # Nhập dữ liệu dạng trực quan hằng ngày (Ví dụ: 08:30)
    user_input = input("Nhập giờ uống thuốc mới (Kiểu HH:MM): ").strip()
    
    # Kiểm tra tính hợp lệ của định dạng đầu vào trước khi xử lý
    if len(user_input) == 5 and ":" in user_input:
        try:
            # Server thực hiện việc bóc tách chuỗi thành số nguyên ở đây!
            parts = user_input.split(":")
            hour = int(parts[0])
            minute = int(parts[1])
            
            # Kiểm tra xem giờ phút nhập vào có hợp lệ trong thực tế không
            if 0 <= hour <= 23 and 0 <= minute <= 59:
                send_medicine_time(hour, minute)
            else:
                print("❌ Giờ không hợp lệ! (Giờ phải từ 00-23, Phút từ 00-59).")
        except ValueError:
            print("❌ Lỗi dữ liệu! Vui lòng chỉ nhập các ký tự số.")
    else:
        print("❌ Sai định dạng! Vui lòng nhập chính xác kiểu HH:MM (Ví dụ: 07:15).")