"""
SmartPilBox Flask Server
- Lưu state xuống state.json (schedules + dose_log)
- Background MQTT subscriber lắng nghe pillbox/status và pillbox/weight
- SSE endpoint đẩy real-time event xuống browser
"""

from flask import Flask, request, jsonify, render_template, Response
import paho.mqtt.client as mqtt
import time, json, os, threading, queue
from datetime import datetime, date

app = Flask(__name__)

DEFAULT_BROKER = "localhost"
DEFAULT_PORT   = 1883
STATE_FILE     = os.path.join(os.path.dirname(__file__), "state.json")
state_lock     = threading.Lock()

# ── SSE client pool ───────────────────────────────────────────
_sse_clients: list[queue.Queue] = []
_sse_lock = threading.Lock()

def sse_push(data: dict):
    """Gửi event tới tất cả browser đang kết nối SSE."""
    line = "data: " + json.dumps(data, ensure_ascii=False) + "\n\n"
    with _sse_lock:
        dead = [q for q in _sse_clients if q.full()]
        for q in dead:
            _sse_clients.remove(q)
        for q in _sse_clients:
            q.put_nowait(line)


# ── Persistent state ──────────────────────────────────────────
def _default_state() -> dict:
    return {
        "schedules": {"0": None, "1": None, "2": None},
        "dose_log":  [],
        "broker":    DEFAULT_BROKER,
        "port":      DEFAULT_PORT,
    }

def load_state() -> dict:
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, encoding="utf-8") as f:
                s = json.load(f)
                # đảm bảo đủ key nếu file cũ thiếu
                for k, v in _default_state().items():
                    s.setdefault(k, v)
                return s
        except Exception:
            pass
    return _default_state()

def save_state():
    """Ghi state xuống file – gọi trong state_lock."""
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, ensure_ascii=False, indent=2)

state = load_state()


# ── MQTT Subscriber (background thread) ───────────────────────
class MQTTSubscriber:
    SUBSCRIBE_TOPICS = [
        ("pillbox/status", 1),
        ("pillbox/weight", 1),
    ]

    def __init__(self):
        self._client: mqtt.Client | None = None
        self._lock    = threading.Lock()
        self.connected = False
        self.broker:  str | None = None
        self.port:    int | None = None

    # ── public ──────────────────────────────────────────────
    def start(self, broker: str, port: int):
        with self._lock:
            self._teardown()
            self.broker = broker
            self.port   = int(port)
        threading.Thread(target=self._run, name="mqtt-sub", daemon=True).start()

    def stop(self):
        with self._lock:
            self._teardown()
        sse_push({"type": "mqtt_disconnected"})

    def status(self) -> dict:
        return {
            "connected": self.connected,
            "broker":    self.broker,
            "port":      self.port,
        }

    # ── internal ────────────────────────────────────────────
    def _teardown(self):
        if self._client:
            try:
                self._client.loop_stop()
                self._client.disconnect()
            except Exception:
                pass
            self._client   = None
            self.connected = False

    def _run(self):
        c = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
        c.on_connect    = self._on_connect
        c.on_disconnect = self._on_disconnect
        c.on_message    = self._on_message
        with self._lock:
            self._client = c
        try:
            c.connect(self.broker, self.port, keepalive=60)
            c.loop_forever()          # blocking, runs until disconnect()
        except Exception as e:
            self.connected = False
            sse_push({"type": "mqtt_error", "msg": str(e)})

    def _on_connect(self, client, _ud, _flags, reason_code, _props):
        if reason_code == 0:
            self.connected = True
            client.subscribe(self.SUBSCRIBE_TOPICS)
            sse_push({"type": "mqtt_connected", "broker": self.broker, "port": self.port})
        else:
            sse_push({"type": "mqtt_error", "msg": f"Connect refused: {reason_code}"})

    def _on_disconnect(self, client, _ud, _flags, reason_code, _props):
        self.connected = False
        sse_push({"type": "mqtt_disconnected"})

    def _on_message(self, _client, _ud, msg: mqtt.MQTTMessage):
        topic   = msg.topic
        payload = msg.payload.decode(errors="replace").strip()

        if topic == "pillbox/status":
            self._handle_status(payload)
        elif topic == "pillbox/weight":
            sse_push({"type": "weight", "value": payload})

    def _handle_status(self, payload: str):
        """
        Định dạng payload từ ESP32:  slot|status
          slot   : 0 / 1 / 2
          status : taken | 1    →  đã uống
                   missed | 0   →  bỏ lỡ
        Nếu chỉ có slot (không có |), mặc định là 'taken'.
        """
        today = date.today().isoformat()
        parts = payload.split("|")
        try:
            slot = int(parts[0])
            raw  = parts[1].strip().lower() if len(parts) >= 2 else "taken"
            status = "taken" if raw in ("taken", "1") else "missed"
        except (ValueError, IndexError):
            sse_push({"type": "mqtt_raw", "topic": "pillbox/status", "payload": payload})
            return

        with state_lock:
            sch = state["schedules"].get(str(slot))
            if sch is None:
                return
            # xóa entry cũ cùng ngày + slot rồi ghi mới
            state["dose_log"] = [
                d for d in state["dose_log"]
                if not (d["date"] == today and d["slot"] == slot)
            ]
            state["dose_log"].append({
                "date":      today,
                "slot":      slot,
                "hour":      sch["hour"],
                "minute":    sch["minute"],
                "status":    status,
                "marked_at": datetime.now().isoformat(),
                "source":    "mqtt",          # phân biệt tự động vs thủ công
            })
            save_state()

        sse_push({
            "type":   "state_updated",
            "slot":   slot,
            "status": status,
            "date":   today,
            "source": "mqtt",
        })


subscriber = MQTTSubscriber()


# ── MQTT publish (one-shot) ───────────────────────────────────
def publish_mqtt(broker: str, port: int, topic: str, payload: str):
    c = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    try:
        c.connect(broker, int(port), 60)
        c.loop_start()
        time.sleep(0.2)
        r = c.publish(topic, payload=payload, qos=1)
        r.wait_for_publish()
        c.loop_stop()
        c.disconnect()
        return True, None
    except Exception as e:
        return False, str(e)


# ════════════════════════════════════════════════════════════
#  Flask routes
# ════════════════════════════════════════════════════════════

@app.route("/")
def index():
    return render_template("index.html")


# ── SSE stream ───────────────────────────────────────────────
@app.route("/api/events")
def sse_stream():
    q: queue.Queue = queue.Queue(maxsize=100)
    with _sse_lock:
        _sse_clients.append(q)

    def generate():
        # kimu tra ngay khi client kết nối
        yield f"data: {json.dumps({'type': 'hello', 'mqtt': subscriber.status()})}\n\n"
        try:
            while True:
                try:
                    yield q.get(timeout=28)   # timeout < nginx 30s proxy timeout
                except queue.Empty:
                    yield ": ping\n\n"        # giữ kết nối
        except GeneratorExit:
            with _sse_lock:
                try:
                    _sse_clients.remove(q)
                except ValueError:
                    pass

    return Response(
        generate(),
        mimetype="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


# ── State ────────────────────────────────────────────────────
@app.route("/api/state")
def get_state():
    with state_lock:
        return jsonify({**state, "mqtt": subscriber.status()})


# ── MQTT subscriber control ──────────────────────────────────
@app.route("/api/mqtt/connect", methods=["POST"])
def mqtt_connect():
    data   = request.json or {}
    broker = data.get("broker", state.get("broker", DEFAULT_BROKER))
    port   = int(data.get("port", state.get("port", DEFAULT_PORT)))
    with state_lock:
        state["broker"] = broker
        state["port"]   = port
        save_state()
    subscriber.start(broker, port)
    return jsonify({"success": True, "broker": broker, "port": port})

@app.route("/api/mqtt/disconnect", methods=["POST"])
def mqtt_disconnect():
    subscriber.stop()
    return jsonify({"success": True})

@app.route("/api/mqtt/status")
def mqtt_status():
    return jsonify(subscriber.status())


# ── Device commands ──────────────────────────────────────────
@app.route("/api/rtc", methods=["POST"])
def set_rtc():
    data   = request.json
    broker = data.get("broker", state.get("broker", DEFAULT_BROKER))
    port   = data.get("port",   state.get("port",   DEFAULT_PORT))

    if data.get("auto"):
        now = datetime.now()
        h, m, s = now.hour, now.minute, now.second
    else:
        h = int(data["hour"])
        m = int(data["minute"])
        s = int(data.get("second", 0))

    if not (0 <= h <= 23 and 0 <= m <= 59 and 0 <= s <= 59):
        return jsonify({"success": False, "error": "Giá trị thời gian không hợp lệ"}), 400

    payload = f"{h}|{m}|{s}"
    ok, err = publish_mqtt(broker, port, "pillbox/set_rtc", payload)
    if ok:
        return jsonify({"success": True, "payload": payload})
    return jsonify({"success": False, "error": err}), 500


@app.route("/api/schedule", methods=["POST"])
def set_schedule():
    data   = request.json
    broker = data.get("broker", state.get("broker", DEFAULT_BROKER))
    port   = data.get("port",   state.get("port",   DEFAULT_PORT))
    slot   = int(data["slot"])
    h      = int(data["hour"])
    m      = int(data["minute"])

    if not (0 <= slot <= 2 and 0 <= h <= 23 and 0 <= m <= 59):
        return jsonify({"success": False, "error": "Giá trị không hợp lệ"}), 400

    payload = f"{slot}|{h}|{m}"
    ok, err = publish_mqtt(broker, port, "pillbox/schedule", payload)
    if ok:
        with state_lock:
            state["schedules"][str(slot)] = {"hour": h, "minute": m}
            save_state()
        sse_push({"type": "state_updated"})
        return jsonify({"success": True, "payload": payload})
    return jsonify({"success": False, "error": err}), 500


@app.route("/api/tare", methods=["POST"])
def tare():
    data   = request.json
    broker = data.get("broker", state.get("broker", DEFAULT_BROKER))
    port   = data.get("port",   state.get("port",   DEFAULT_PORT))
    ok, err = publish_mqtt(broker, port, "pillbox/tare", "TARE")
    if ok:
        return jsonify({"success": True})
    return jsonify({"success": False, "error": err}), 500


# ── Manual dose marking ──────────────────────────────────────
@app.route("/api/dose/mark", methods=["POST"])
def mark_dose():
    data     = request.json
    slot     = int(data["slot"])
    date_str = data.get("date", date.today().isoformat())
    status   = data.get("status", "taken")   # "taken" | "missed" | "clear"

    with state_lock:
        sch = state["schedules"].get(str(slot))
        if sch is None:
            return jsonify({"success": False, "error": "Slot chưa được đặt lịch"}), 400

        state["dose_log"] = [
            d for d in state["dose_log"]
            if not (d["date"] == date_str and d["slot"] == slot)
        ]
        if status != "clear":
            state["dose_log"].append({
                "date":      date_str,
                "slot":      slot,
                "hour":      sch["hour"],
                "minute":    sch["minute"],
                "status":    status,
                "marked_at": datetime.now().isoformat(),
                "source":    "manual",
            })
        save_state()

    sse_push({"type": "state_updated", "slot": slot, "status": status, "date": date_str})
    return jsonify({"success": True})


# ════════════════════════════════════════════════════════════
if __name__ == "__main__":
    # Tự động kết nối subscriber khi khởi động (dùng broker đã lưu)
    subscriber.start(
        state.get("broker", DEFAULT_BROKER),
        state.get("port",   DEFAULT_PORT),
    )
    # use_reloader=False bắt buộc để tránh MQTT thread bị duplicate
    app.run(debug=True, host="0.0.0.0", port=5000, use_reloader=False)
