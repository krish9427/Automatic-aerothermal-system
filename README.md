# 🔥 Automatic Aerothermal System using ESP32

An embedded systems project that automatically controls airflow (fan speed) based on temperature, providing efficient thermal management using an ESP32 microcontroller.

---

## 📌 Project Overview

The Automatic Aerothermal System monitors temperature using a DS18B20 digital temperature sensor and dynamically adjusts the fan speed using PWM control. The system also measures fan RPM using a tachometer signal and displays real-time data on an OLED display.

This project demonstrates concepts of:
- Embedded C/C++
- Sensors & Actuators
- PWM Control
- Interrupts
- I2C Communication
- Real-time monitoring

---

## ⚙️ Features

- 🌡️ Accurate temperature sensing using DS18B20
- 🌀 Automatic fan speed control using PWM
- 📈 Fan RPM measurement using tachometer feedback
- 🖥️ OLED display for real-time data
- 🔋 Efficient power management
- 🧠 Interrupt-based RPM calculation

---

## 🧰 Hardware Components

| Component | Description |
|---------|------------|
| ESP32 | Main controller |
| DS18B20 | Digital temperature sensor |
| OLED SSD1306 | 128x64 I2C display |
| DC Fan | With PWM and tachometer |
| Level Shifter | 3.3V ↔ 5V logic |
| MT3608 | Boost converter |
| Li-ion Battery | 3.7V power source |

---

## 🔌 Pin Configuration

| Function | ESP32 GPIO |
|--------|-----------|
| DS18B20 Data | GPIO 23 |
| Fan PWM | GPIO 4 |
| Fan Tachometer | GPIO 5 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |

---

## 🧠 Working Principle

1. The DS18B20 continuously measures temperature.
2. ESP32 reads temperature data using OneWire protocol.
3. Based on temperature range, PWM duty cycle is adjusted.
4. Fan speed increases or decreases automatically.
5. Tachometer pulses are counted using interrupts.
6. RPM is calculated and displayed on OLED.
7. System runs in real-time with efficient control.

---

## 🖼️ Hardware Setup

![Circuit Diagram](hardware/images/circuit.png)
![Breadboard Setup](hardware/images/breadboard.jpg)

---

## 🚀 Applications

- Electronics cooling systems
- Smart ventilation
- Thermal management in embedded systems
- IoT-based cooling solutions

---

## 🔮 Future Improvements

- Wi-Fi / Blynk cloud monitoring
- Mobile app dashboard
- Data logging to SD card
- PID-based fan control
- Enclosure and PCB design

---

## 👨‍💻 Author

**Krish Oza**  
Electronics & Communication Engineering  
ESP32 | Embedded Systems | IoT  

---

## 📜 License

This project is licensed under the MIT License.
