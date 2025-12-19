# Automatic Aerothermal Fan Controller

This project is an advanced cooling system powered by an **ESP32**. It monitors real-time temperature using a DS18B20 sensor and automatically adjusts a 4-wire PWM fan's speed based on thermal thresholds.

## 🚀 Features
* [cite_start]**Automated Thermal Control:** Fan speed scales dynamically with temperature[cite: 27, 31].
* [cite_start]**IoT Integration:** Live monitoring and manual override via the **Blynk App**[cite: 14, 15, 44].
* [cite_start]**OLED Dashboard:** Real-time display of RPM, Temperature, PWM Duty Cycle, and WiFi status[cite: 50, 53, 55].
* [cite_start]**RPM Feedback:** Uses the fan's tachometer signal for precise speed measurement[cite: 10, 78].

## 🛠️ Hardware Requirements
* ESP32 Development Board
* 4-Wire PWM Fan (12V)
* DS18B20 Temperature Sensor
* [cite_start]SSD1306 OLED Display (I2C) [cite: 6]
* Logic Level Converter (3.3V to 5V)
* MT3608 Boost Converter (if powering from Li-ion)

## 📌 Wiring Diagram
![Wiring Diagram](./images/wiring_diagram.jpg)

## 💻 Software Setup
1. Install the following libraries in Arduino IDE:
   - `Blynk`
   - `DallasTemperature`
   - `Adafruit_SSD1306`
2. [cite_start]Update the `wifi_ssid` and `wifi_password` in the code[cite: 2].
3. [cite_start]Input your `BLYNK_AUTH_TOKEN`.
4. Upload `src/perfected_areothermal.ino` to your ESP32.

## 📊 Pin Mapping
| Component | ESP32 Pin |
|-----------|-----------|
| Temp Sensor | [cite_start]GPIO 23 [cite: 3] |
| PWM Fan | [cite_start]GPIO 4 [cite: 3] |
| Tachometer | [cite_start]GPIO 5 [cite: 4] |
| OLED SDA | [cite_start]GPIO 21 [cite: 5] |
| OLED SCL | [cite_start]GPIO 22 [cite: 6] |
