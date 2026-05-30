import paho.mqtt.client as mqtt
import time

# --- CẤU HÌNH ---
# Giữ nguyên IP hiện tại của bạn
MQTT_BROKER = "192.168.12.9" 
MQTT_PORT = 1883

# THAY ĐỔI: Chuyển sang 1 topic tích hợp duy nhất quản lý đa lịch trình
TOPIC_SCHEDULE = "pillbox/set_schedule"

def send_medicine_schedule(slot_val, hour_val, minute_val):
    """Hàm đóng gói dữ liệu thành dạng 'slot|hour|minute' và gửi qua một topic duy nhất"""
    # Khai báo chuẩn API VERSION2 để tránh lỗi cảnh báo của thư viện
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    
    try:
        print(f"[Connecting] Đang kết nối tới Broker {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        client.loop_start()
        time.sleep(0.2)  # Đợi một chút để thiết lập kết nối nền ổn định
        
        # Đóng gói payload theo định dạng ESP32 chờ đợi: "slot|hour|minute" (Ví dụ: "1|12|30")
        payload_str = f"{slot_val}|{hour_val}|{minute_val}"
        
        print(f"[Publish] Gửi payload: '{payload_str}' tới topic '{TOPIC_SCHEDULE}'")
        res = client.publish(TOPIC_SCHEDULE, payload=payload_str, qos=1)
        res.wait_for_publish()  # Chờ Broker xác nhận đã nhận được bản tin
        
        print("[Success] ✅ Đã gửi cấu hình cập nhật lịch uống thuốc mới thành công!")
        
        client.loop_stop()
        client.disconnect()
        
    except Exception as e:
        print(f"[Error] ❌ Không thể kết nối tới Broker: {e}")
        print("Vui lòng đảm bảo dịch vụ Mosquitto trên máy tính vẫn đang ở trạng thái Running.")

if __name__ == "__main__":
    print("=== TEST CẬP NHẬT ĐA LỊCH TRÌNH PILLBOX ===")
    
    try:
        # 1. Nhập slot lịch trình cần thay đổi
        slot_input = input("Nhập chỉ số Slot cần đặt (0, 1 hoặc 2): ").strip()
        slot = int(slot_input)
        
        if not (0 <= slot <= 2):
            print("❌ Chỉ số Slot phải nằm trong khoảng từ 0 đến 2 (tương ứng tối đa 3 lịch trình).")
            exit()
            
        # 2. Nhập giờ uống thuốc tương ứng cho slot đó
        time_input = input(f"Nhập giờ uống thuốc cho Slot [{slot}] (Kiểu HH:MM): ").strip()
        
        # Kiểm tra tính hợp lệ của định dạng giờ phút đầu vào
        if len(time_input) == 5 and ":" in time_input:
            parts = time_input.split(":")
            hour = int(parts[0])
            minute = int(parts[1])
            
            # Kiểm tra xem giờ phút nhập vào có hợp lệ thực tế không
            if 0 <= hour <= 23 and 0 <= minute <= 59:
                # Thực hiện gửi dữ liệu tích hợp lên Broker
                send_medicine_schedule(slot, hour, minute)
            else:
                print("❌ Giờ không hợp lệ! (Giờ phải từ 00-23, Phút từ 00-59).")
        else:
            print("❌ Sai định dạng! Vui lòng nhập chính xác kiểu HH:MM (Ví dụ: 08:30).")
            
    except ValueError:
        print("❌ Lỗi dữ liệu! Vui lòng chỉ nhập các ký tự số.")