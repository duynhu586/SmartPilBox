import paho.mqtt.client as mqtt
import time

# --- CẤU HÌNH ---
MQTT_BROKER = "192.168.2.28" 
MQTT_PORT = 1883
TOPIC_TARE = "pillbox/tare"

def send_tare_command():
    """Hàm gửi lệnh yêu cầu ESP32 Tare (về 0) lại cân nặng từ xa"""
    # Khai báo chuẩn API VERSION2 tương thích bản paho-mqtt mới nhất
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    
    try:
        print(f"[Connecting] Đang kết nối tới Broker {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        client.loop_start()
        time.sleep(0.2)  # Chờ kết nối nền ổn định
        
        payload_str = "TARE"
        print(f"[Publish] Gửi lệnh: '{payload_str}' tới topic '{TOPIC_TARE}'")
        res = client.publish(TOPIC_TARE, payload=payload_str, qos=1)
        res.wait_for_publish()  # Đợi Broker xác nhận đã gửi xong
        
        print("[Success] ✅ Đã gửi lệnh TARE cân nặng thành công! Kiểm tra màn hình ESP32.")
        
        client.loop_stop()
        client.disconnect()
        
    except Exception as e:
        print(f"[Error] ❌ Không thể kết nối tới Broker: {e}")
        print("Vui lòng đảm bảo thiết bị chung mạng và phần mềm Mosquitto đang hoạt động.")

if __name__ == "__main__":
    print("=== LỆNH MQTT: TARE (RESET VỀ 0) CÂN NẶNG PILLBOX ===")
    confirm = input("Xác nhận gửi lệnh Tare cân từ xa? (y/n): ").strip().lower()
    
    if confirm == 'y':
        send_tare_command()
    else:
        print("❌ Đã hủy lệnh.")