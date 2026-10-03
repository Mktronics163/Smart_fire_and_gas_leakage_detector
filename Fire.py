import threading
import time
import statistics
import smtplib
from collections import deque
from email.mime.text import MIMEText

from arduino.app_utils import *
from arduino.app_bricks.web_ui import WebUI

SENDER_EMAIL = "feranmimalik2@gmail.com"
SENDER_APP_PASSWORD = "fvljvbiclaoytdpo"
RECEIVER_EMAIL = "malikwale52@gmail.com"

POLL_INTERVAL_SEC = 1.0

# ---------- Rolling history (for the dashboard graph) ----------
HISTORY_MAX_POINTS = 300  # last 5 minutes at 1 reading/sec
history = deque(maxlen=HISTORY_MAX_POINTS)
history_lock = threading.Lock()

# ---------- AI anomaly detection ----------
AI_WINDOW_SIZE = 30       # ~30 seconds of history informs the baseline
AI_MIN_SAMPLES = 15       # need at least this many readings before judging
AI_Z_THRESHOLD = 3.0      # how many standard deviations counts as "unusual"
gas_window = deque(maxlen=AI_WINDOW_SIZE)


def detect_anomaly(gas_value):
    """Returns (is_anomaly, z_score) for the current reading, based on
    the recent rolling baseline (computed BEFORE this reading is added,
    so the current point can't skew its own baseline)."""
    anomaly = False
    z_score = 0.0

    if len(gas_window) >= AI_MIN_SAMPLES:
        mean = statistics.mean(gas_window)
        stdev = statistics.pstdev(gas_window) or 1.0  # avoid div-by-zero on a flat signal
        z_score = (gas_value - mean) / stdev
        anomaly = z_score > AI_Z_THRESHOLD

    gas_window.append(gas_value)
    return anomaly, round(z_score, 2)


def send_alert_email(subject_prefix, flame, gas, extra_line=""):
    subject = f"{subject_prefix}: Fire/Gas Alert"
    body = (
        f"{subject_prefix} detected by your Smart Fire & Gas Leakage Detector.\n\n"
        f"Flame detected: {'YES' if flame else 'no'}\n"
        f"Gas sensor reading: {gas}\n"
        f"{extra_line}\n\n"
        f"Please check your home."
    )
    msg = MIMEText(body)
    msg["Subject"] = subject
    msg["From"] = SENDER_EMAIL
    msg["To"] = RECEIVER_EMAIL

    try:
        with smtplib.SMTP_SSL("smtp.gmail.com", 465) as server:
            server.login(SENDER_EMAIL, SENDER_APP_PASSWORD)
            server.sendmail(SENDER_EMAIL, RECEIVER_EMAIL, msg.as_string())
        print("Alert email sent.")
    except Exception as e:
        print(f"Failed to send alert email: {e}")


def read_status():
    gas_value = int(Bridge.call("get_gas_value"))
    flame_state = int(Bridge.call("get_flame_state"))
    danger = int(Bridge.call("get_danger_state")) == 1

    anomaly, z_score = detect_anomaly(gas_value)

    return {
        "gas": gas_value,
        "flame": flame_state,
        "danger": danger,
        "status": "DANGER" if danger else "SAFE",
        "ai_anomaly": anomaly,
        "ai_z_score": z_score,
        "timestamp": time.time(),
    }


# ---------- API endpoints for the dashboard ----------
def api_status():
    try:
        return read_status()
    except Exception as e:
        return {"error": str(e)}


def api_history():
    with history_lock:
        return {"points": list(history)}


def api_test_alarm():
    try:
        Bridge.call("set_buzzer", 1)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def api_silence_alarm():
    try:
        Bridge.call("set_buzzer", 0)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def api_set_threshold(value: int):
    try:
        Bridge.call("set_gas_threshold", int(value))
        return {"ok": True, "threshold": int(value)}
    except Exception as e:
        return {"ok": False, "error": str(e)}


# ---------- Background monitoring thread ----------
def monitor_loop():
    last_danger = False
    last_anomaly = False

    while True:
        try:
            data = read_status()
            print(
                f"Flame={data['flame']}  Gas={data['gas']}  Status={data['status']}"
                f"  AI_anomaly={data['ai_anomaly']} (z={data['ai_z_score']})"
            )

            with history_lock:
                history.append({"gas": data["gas"], "timestamp": data["timestamp"]})

            if data["danger"] and not last_danger:
                send_alert_email("DANGER", data["flame"], data["gas"])
            elif data["ai_anomaly"] and not last_anomaly and not data["danger"]:
                send_alert_email(
                    "Unusual Pattern",
                    data["flame"],
                    data["gas"],
                    extra_line=f"AI anomaly score: {data['ai_z_score']} std deviations above baseline "
                               f"(below the hard safety threshold, but rising unusually fast).",
                )

            last_danger = data["danger"]
            last_anomaly = data["ai_anomaly"]

        except Exception as e:
            print(f"Bridge error: {e}")

        time.sleep(POLL_INTERVAL_SEC)


# ---------- App entry point ----------
web_ui = WebUI()
web_ui.expose_api("GET", "/api/status", api_status)
web_ui.expose_api("GET", "/api/history", api_history)
web_ui.expose_api("GET", "/api/test_alarm", api_test_alarm)
web_ui.expose_api("GET", "/api/silence_alarm", api_silence_alarm)
web_ui.expose_api("GET", "/api/set_threshold", api_set_threshold)

if __name__ == "__main__":
    monitor_thread = threading.Thread(target=monitor_loop, daemon=True)
    monitor_thread.start()

    print("Fire & Gas Detector dashboard starting on port 7000...")
    App.run()
void loop() {
  // put your main code here, to run repeatedly:

}
