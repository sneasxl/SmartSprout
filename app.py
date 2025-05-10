from flask import Flask, request, jsonify
import requests

app = Flask(__name__)

# ─── Telegram Bot Settings ────────────────────────────────────────────────
BOT_TOKEN = "7790866934:AAEfJMiq3aVr5IvGkrZ8N_zEvUCdXlpYOZk"
CHAT_ID   = 7017384570    # your chat ID
THRESHOLD = 40            # moisture % below which to alert

TELEGRAM_URL = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"

def send_telegram(message: str):
    try:
        resp = requests.post(
            TELEGRAM_URL,
            data={"chat_id": CHAT_ID, "text": message},
            timeout=5
        )
        resp.raise_for_status()
        print("✅ Telegram sent")
    except Exception as e:
        print("❌ Telegram error:", e)

@app.route('/data', methods=['POST'])
def receive_data():
    data = request.get_json(force=True)
    moisture = data.get("moisture")
    raw      = data.get("raw")
    tempF    = data.get("tempF")
    hum      = data.get("hum")

    # Log incoming values
    print(f"Received → moisture={moisture}%  raw={raw}  temp={tempF}°F  hum={hum}%")

    # Build the alert message
    alert_msg = (
        f"🌱 SmartSprout Alert!\n"
        f"Soil moisture: {moisture}% (raw {raw})\n"
        f"Air Temp: {tempF}°F\n"
        f"Humidity: {hum}%"
    )

    # Send Telegram alert if below threshold
    if moisture is not None and moisture < THRESHOLD:
        send_telegram(alert_msg)

    return jsonify({"status": "ok"}), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
