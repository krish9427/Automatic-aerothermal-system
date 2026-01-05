<div align="center">

# 🔥 Automatic Aerothermal System using ESP32

[![GitHub Stars](https://img.shields.io/github/stars/krish9427/Automatic-aerothermal-system?style=social)](https://github.com/krish9427/Automatic-aerothermal-system/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/krish9427/Automatic-aerothermal-system?style=social)](https://github.com/krish9427/Automatic-aerothermal-system/network/members)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](http://makeapullrequest.com)

**An intelligent ESP32-based temperature-controlled fan system with real-time monitoring and IoT integration**

[Features](#-features) • [Hardware](#-hardware-components) • [Installation](#-installation) • [Usage](#-usage) • [Documentation](#-documentation)

</div>

---

## 📌 Overview

The Automatic Aerothermal System is an embedded systems project that intelligently controls fan speed based on ambient temperature. It uses a DS18B20 digital temperature sensor for precise temperature monitoring and automatically adjusts airflow using PWM (Pulse Width Modulation) control. The system also measures fan RPM via a tachometer and displays real-time data on an OLED display.

### 🎯 Key Highlights

- **Smart Temperature Control**: Automatic fan speed adjustment based on temperature thresholds
- **Real-time Monitoring**: Live display of temperature, PWM value, and fan RPM
- **Interrupt-based RPM Measurement**: Efficient tachometer pulse counting using ISR
- **Low Power Design**: Optimized power management with battery support
- **IoT Ready**: Designed for future Blynk cloud integration

---

## ⚙️ Features

- 🌡️ **Accurate Temperature Sensing** - DS18B20 digital sensor with OneWire protocol
- 🌀 **Automatic PWM Fan Control** - Dynamic speed adjustment (0-100%)
- 📈 **RPM Measurement** - Real-time fan speed monitoring via tachometer
- 🖥️ **OLED Display** - 128x64 I2C display for system status
- 🔋 **Efficient Power Management** - Battery-powered with boost converter
- 🧠 **Interrupt-based Processing** - Precise RPM calculation using ISR
- 📡 **IoT Integration Ready** - Foundation for remote monitoring (Wi-Fi/Blynk)

---

## 🧰 Hardware Components

| Component | Description | Quantity |
|-----------|-------------|----------|
| **ESP32 Dev Board** | Main microcontroller | 1 |
| **DS18B20** | Digital temperature sensor | 1 |
| **OLED SSD1306** | 128x64 I2C display | 1 |
| **DC Fan (PWM)** | With tachometer output | 1 |
| **Logic Level Shifter** | 3.3V ↔ 5V conversion | 1 |
| **MT3608 Boost Converter** | Step-up voltage regulator | 1 |
| **Li-ion Battery (3.7V)** | Power source | 1 |
| **4.7kΩ Resistor** | DS18B20 pull-up | 1 |
| **Breadboard & Jumper Wires** | Prototyping | As required |

---

## 🔌 Pin Configuration

| Function | ESP32 GPIO | Notes |
|----------|------------|-------|
| **DS18B20 Data** | GPIO 23 | 4.7kΩ pull-up to 3.3V |
| **Fan PWM** | GPIO 4 | 25 kHz PWM signal |
| **Fan Tachometer** | GPIO 5 | Interrupt-enabled |
| **OLED SDA** | GPIO 21 | I2C data line |
| **OLED SCL** | GPIO 22 | I2C clock line |

---

## 🚀 Installation

### Prerequisites

- Arduino IDE (v1.8.19 or newer) or PlatformIO
- ESP32 board support package
- Required libraries (see below)

### Required Libraries

Install these libraries via Arduino Library Manager:

```
- OneWire by Paul Stoffregen
- DallasTemperature by Miles Burton
- Adafruit GFX Library
- Adafruit SSD1306
- Wire (built-in)
```

### Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/krish9427/Automatic-aerothermal-system.git
   cd Automatic-aerothermal-system
   ```

2. **Open the project**
   - Launch Arduino IDE
   - Open `src/Aerothermal.ino`

3. **Configure Wi-Fi credentials** (if using Blynk)
   ```cpp
   const char* wifi_ssid = "YOUR_SSID";
   const char* wifi_password = "YOUR_PASSWORD";
   ```

4. **Select board and port**
   - Tools → Board → ESP32 Dev Module
   - Tools → Port → Select your ESP32 port

5. **Upload the code**
   - Click Upload button or press Ctrl+U

---

## 🖼️ Hardware Setup

### Circuit Diagram
![Circuit Diagram](hardware/images/circuit.png)

### Breadboard Setup
![Breadboard](hardware/images/breadboard.jpg)

> **Note**: Ensure proper power supply connections and level shifting for 5V components.

---

## 💡 Usage

### System Operation

1. **Power On**: Connect battery or USB power
2. **Initialization**: OLED displays startup message
3. **Temperature Monitoring**: DS18B20 reads ambient temperature
4. **Auto Fan Control**: PWM adjusts based on temperature:
   - Below 25°C → Fan OFF
   - 25-30°C → Low speed (30-50%)
   - 30-35°C → Medium speed (50-75%)
   - Above 35°C → High speed (75-100%)
5. **Real-time Display**: OLED shows Temperature, PWM%, and RPM

### Temperature Thresholds

You can customize temperature ranges in the code:

```cpp
if (temperature < 25.0) {
  fanSpeed = 0;  // Fan OFF
} else if (temperature < 30.0) {
  fanSpeed = map(temperature, 25, 30, 77, 128);  // 30-50%
} else if (temperature < 35.0) {
  fanSpeed = map(temperature, 30, 35, 128, 192);  // 50-75%
} else {
  fanSpeed = 255;  // 100% speed
}
```

---

## 🧠 Working Principle

### System Flowchart

```
┌──────────────────┐
│  Power On        │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Initialize      │
│  - OLED          │
│  - DS18B20       │
│  - PWM Channel   │
│  - ISR           │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Read Temp       │◄────────┐
│  (DS18B20)       │         │
└────────┬─────────┘         │
         │                   │
         ▼                   │
┌──────────────────┐         │
│  Calculate PWM   │         │
│  (Based on temp) │         │
└────────┬─────────┘         │
         │                   │
         ▼                   │
┌──────────────────┐         │
│  Set Fan Speed   │         │
│  (PWM output)    │         │
└────────┬─────────┘         │
         │                   │
         ▼                   │
┌──────────────────┐         │
│  Count Pulses    │         │
│  (ISR)           │         │
└────────┬─────────┘         │
         │                   │
         ▼                   │
┌──────────────────┐         │
│  Calculate RPM   │         │
└────────┬─────────┘         │
         │                   │
         ▼                   │
┌──────────────────┐         │
│  Update OLED     │         │
│  Display         │         │
└────────┬─────────┘         │
         │                   │
         └───────────────────┘
```

### Temperature Sensing
The DS18B20 uses the OneWire protocol for digital temperature transmission, ensuring high accuracy (±0.5°C) and noise immunity.

### Fan Speed Control
The ESP32 generates a 25 kHz PWM signal. Duty cycle adjustments (0-255) control fan speed proportionally based on temperature thresholds.

### RPM Measurement
The fan's tachometer generates pulses (typically 2 pulses per revolution). An interrupt service routine (ISR) counts these pulses, and RPM is calculated every second:

```
RPM = (Pulse Count × 60) / (Pulses per Revolution × Time Interval)
```

### Display
An OLED display connected via I2C shows:
- **Temperature** (°C)
- **Fan PWM value** (%)
- **Fan RPM**

---

## 📂 Repository Structure

```
Automatic-aerothermal-system/
│
├── src/
│   └── Aerothermal.ino          # Main Arduino sketch
│
├── hardware/
│   ├── BOM.md                    # Bill of Materials
│   ├── images/                   # Circuit diagrams and photos
│   └── videos/                   # Demo videos
│
├── docs/
│   └── working.md                # Detailed working explanation
│
├── README.md                     # Project documentation
└── LICENSE                       # MIT License
```

---

## 📚 Documentation

- **[Bill of Materials (BOM)](hardware/BOM.md)** - Complete component list
- **[Working Explanation](docs/working.md)** - Detailed system operation
- **[Demo Video](hardware/videos)** - System demonstration

---

## 🚀 Applications

- 🖥️ **Computer Cooling Systems** - PC case temperature management
- 🏠 **Smart Home Ventilation** - Automated room cooling
- 🔬 **Laboratory Equipment** - Thermal control for sensitive instruments
- 🌐 **IoT Cooling Solutions** - Remote monitoring and control
- 📦 **Electronics Enclosures** - Equipment thermal management

---

## 🔮 Future Improvements

- [ ] **Wi-Fi/Blynk Integration** - Remote monitoring via mobile app
- [ ] **Data Logging** - Store temperature history to SD card
- [ ] **PID Control Algorithm** - More precise temperature regulation
- [ ] **Multiple Sensor Support** - Monitor different zones
- [ ] **Custom PCB Design** - Compact, professional board
- [ ] **Enclosure Design** - 3D-printed housing
- [ ] **Web Dashboard** - Real-time graphs and analytics
- [ ] **MQTT Integration** - Home Assistant compatibility

---

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## 👨‍💻 Author

**Krish Oza**  
📧 [GitHub](https://github.com/krish9427) | 💼 [LinkedIn](https://linkedin.com/in/krish-oza)

*Electronics & Communication Engineering Student*  
*Specialization: Embedded Systems | ESP32 | IoT*

---

## 🌟 Acknowledgments

- **OneWire & DallasTemperature Libraries** - Temperature sensor communication
- **Adafruit Libraries** - OLED display support
- **ESP32 Community** - Hardware and software resources

---

<div align="center">

**If you found this project useful, please give it a ⭐!**

[![Star History Chart](https://api.star-history.com/svg?repos=krish9427/Automatic-aerothermal-system&type=Date)](https://star-history.com/#krish9427/Automatic-aerothermal-system&Date)

</div>
