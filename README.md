# ⌚ SMART WATCH
### AI-Powered Health Monitoring Wearable
`ESP8266` • `Node.js` • `Gemini AI` • `OLED Display`

---

## 📋 Project Overview

SMART WATCH is an embedded health monitoring wearable built on the ESP8266 microcontroller. It continuously tracks biometric and environmental data, performs on-device hydration risk analysis, and streams all readings to a Node.js backend powered by Google Gemini AI for intelligent health insights.

The project is split into two main components:
- **AI_watch.ino** — Arduino firmware running on the ESP8266 hardware
- **Backend (Node.js + Express)** — REST API server with Gemini AI integration and a web dashboard

---

## ✨ Key Features

### Hardware Sensors
- Heart Rate & SpO₂ monitoring via MAX30100 pulse oximeter
- Step counting via MPU6050 6-axis accelerometer/gyroscope
- Ambient temperature & humidity via DHT11/DHT22 sensor
- Wear detection via analog IR sensor (proximity-based on A0)
- 128×64 OLED display (SSD1306 SPI) for real-time metrics

### Hydration Risk Engine
An on-device multi-factor hydration assessment model calculates a live risk score:
- **Environmental Load Index** — derived from temperature, humidity, and pressure
- **Resting Heart Rate Analysis** — flags elevated HR only when the user is sedentary
- **Activity-Temperature Correlation** — penalises motion in hot conditions (sweating loss)
- **Wear Awareness** — suppresses stale sensor data when the device is not being worn

| Score | Advice               |
|-------|----------------------|
| 0–1   | Hydration OK         |
| 2     | Mild Risk: Hydrate   |
| 3+    | High Risk: Drink Now!|

### AI Health Assistant
- Gemini 2.0 Flash integration on the backend
- Context-aware prompts built from live sensor readings
- REST endpoint `/ask-ai` accepts natural language health queries
- Responses stored in memory and accessible via `/last-ai`

### Web Dashboard
- Served from the `/public` directory at `http://localhost:3000`
- Real-time display of all sensor values via polling of `/data`
- Interactive AI query panel powered by the `/ask-ai` endpoint

---

## 📁 Project Structure
```
SMART_WATCH/
├── AI_watch/
│   └── AI_watch.ino          # ESP8266 Arduino firmware
└── Backend/
    ├── node_modules/
    ├── public/
    │   ├── index.html        # Web dashboard
    │   ├── index.js          # Frontend JavaScript
    │   └── spectra.jpg       # Static asset
    ├── index.js              # Express server + Gemini AI
    ├── package.json
    └── package-lock.json
```

---

## 🔌 Hardware & Wiring

### Components Required

| Component              | Purpose                                      |
|------------------------|----------------------------------------------|
| NodeMCU ESP8266        | Main microcontroller with Wi-Fi              |
| MAX30100               | Heart rate & SpO₂ sensor (I2C)              |
| MPU6050                | Accelerometer / gyroscope for step counting  |
| SSD1306 OLED (128×64)  | Display (SPI)                                |
| DHT11 / DHT22          | Temperature & humidity                       |
| IR Proximity Sensor    | Wear detection (analog output → A0)          |

### Pin Mapping (ESP8266)

| Signal                    | ESP8266 Pin   |
|---------------------------|---------------|
| OLED MOSI (SDA)           | D7            |
| OLED CLK (SCL)            | D5            |
| OLED DC                   | D3            |
| OLED CS                   | D8            |
| OLED RESET                | D4            |
| DHT Data                  | D6            |
| IR Sensor (analog)        | A0            |
| MAX30100 / MPU6050 SDA    | D2 (I2C)      |
| MAX30100 / MPU6050 SCL    | D1 (I2C)      |

---

## ⚙️ Firmware Setup (Arduino IDE)

### Required Libraries
- `MAX30100_PulseOximeter`
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `MPU6050` (Electronic Cats or I2Cdevlib)
- `ESP8266WiFi` (built into ESP8266 board package)
- `ESP8266HTTPClient`
- `ArduinoJson`
- `DHT sensor library` (Adafruit)

### Configuration
Before flashing, update these constants in `AI_watch.ino`:
```cpp
const char *ssid     = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

const char *serverDataEndpoint = "http://<SERVER_IP>:3000/data";
const char *aiServer           = "http://<SERVER_IP>:3000/ask-ai";
```

> The firmware uses NTP to sync real-time (IST, GMT+5:30). Update `gmtOffset_sec` if your timezone differs.

---

## 🖥️ Backend Setup (Node.js)

### Installation
```bash
cd Backend
npm install
```

### Dependencies

| Package                  | Purpose                  |
|--------------------------|--------------------------|
| express                  | HTTP server framework    |
| cors                     | Cross-origin support     |
| @google/generative-ai    | Gemini AI SDK            |
| path                     | File path utilities      |

### Gemini API Key
Replace the placeholder in `index.js` with your key from [Google AI Studio](https://aistudio.google.com):
```js
const apikey = 'YOUR_GEMINI_API_KEY';
```

### Starting the Server
```bash
node index.js
# Server running at http://localhost:3000
```

---

## 🔗 API Reference

| Method + Route   | Description                                        |
|------------------|----------------------------------------------------|
| `POST /data`     | Receive sensor data from ESP8266                  |
| `GET  /data`     | Return the latest sensor snapshot as JSON         |
| `POST /ask-ai`   | Send a health query to Gemini AI                  |
| `GET  /last-ai`  | Retrieve the most recent AI response              |
| `GET  /`         | Serve the web dashboard (index.html)              |

### POST /data — Payload Schema
```json
{
  "heartRate":      75,
  "spo2":           98,
  "temperature":    28.5,
  "humidity":       65,
  "pressure":       1012,
  "steps":          1234,
  "hydrationScore": 2,
  "hydrationAdvice":"Mild Risk: Hydrate",
  "obstacle":       true,
  "time":           "14:32:05"
}
```

### POST /ask-ai — Payload Schema
```json
{
  "query": "Is my heart rate normal for my current activity?"
}
```

---

## 🔄 How It Works

1. ESP8266 reads all sensors every 1 second
2. Hydration risk score is computed on-device from the multi-factor model
3. OLED display is refreshed with current metrics, advice, and timestamp
4. Sensor payload is POSTed to `/data` on the backend
5. Backend stores the latest reading in memory; web dashboard polls `/data` to display it
6. User sends a natural language query from the dashboard → backend builds a context-rich Gemini prompt → AI response is returned and stored

---

## 👁️ Wear Detection & Data Integrity

The IR proximity sensor on pin `A0` determines whether the watch is actually being worn, preventing stale/invalid readings from being sent or displayed:

- IR value `< 500` → device is worn (`isWorn = true`)
- IR value `≥ 500` for 5+ consecutive readings → not worn (`notWornCounter > threshold`)
- When not worn: HR and SpO₂ are cleared, hydration score resets to 0, OLED shows `Status: Not Worn`
- Backend receives wear state via the `obstacle` field in the POST payload

---

##Real Time Results
https://github.com/SuryaThejas-07/AI_Smart_Watch/blob/e2e3c374b35f4b2c51dcb389fd58e03e5f874e63/Backend/public/assets/design.png
https://github.com/SuryaThejas-07/AI_Smart_Watch/blob/e2e3c374b35f4b2c51dcb389fd58e03e5f874e63/Backend/public/assets/demo.png
https://github.com/SuryaThejas-07/AI_Smart_Watch/blob/e2e3c374b35f4b2c51dcb389fd58e03e5f874e63/Backend/public/assets/out3.png
https://github.com/SuryaThejas-07/AI_Smart_Watch/blob/e2e3c374b35f4b2c51dcb389fd58e03e5f874e63/Backend/public/assets/out4.png
https://github.com/SuryaThejas-07/AI_Smart_Watch/blob/e2e3c374b35f4b2c51dcb389fd58e03e5f874e63/Backend/public/assets/out2.png
https://github.com/SuryaThejas-07/AI_Smart_Watch/blob/e2e3c374b35f4b2c51dcb389fd58e03e5f874e63/Backend/public/assets/out1.png

---

## 🛠️ Troubleshooting

| Issue                          | Solution                                                  |
|--------------------------------|-----------------------------------------------------------|
| SSD1306 allocation failed      | Check SPI wiring; ensure CS/DC/RESET pins match defines   |
| MAX30100 failed to initialize  | Confirm I2C pull-up resistors; check 3.3V power           |
| MPU6050 connection failed      | Verify I2C address (0x68 or 0x69); check AD0 pin          |
| DHT sensor read fails          | Fallback values used (25°C / 50% RH); check wiring        |
| WiFi won't connect             | Verify SSID/password; ESP8266 supports 2.4 GHz only       |
| POST returns non-200           | Confirm server IP and port 3000; check firewall rules      |
| Gemini API error               | Validate API key; check Google AI Studio quota limits      |

---

## 📄 License & Credits

This project is open-source for educational and personal use.

**Built with:**
- Arduino / ESP8266 ecosystem
- Adafruit sensor libraries
- Google Gemini 2.0 Flash
- Express.js

---

*SMART WATCH — Health Intelligence on Your Wrist*
