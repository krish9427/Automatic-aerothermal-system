# System Working Explanation

The Automatic Aerothermal System operates by continuously monitoring the ambient temperature and controlling airflow accordingly.

## Temperature Sensing
The DS18B20 sensor uses the OneWire protocol to transmit digital temperature data to the ESP32. This ensures high accuracy and noise immunity.

## Fan Speed Control
The ESP32 generates a PWM signal at 25 kHz. Based on temperature thresholds, the duty cycle is adjusted to control fan speed.

## RPM Measurement
The fan tachometer output generates pulses for each revolution. An interrupt service routine (ISR) counts these pulses, and RPM is calculated every second.

## Display
An OLED display connected via I2C shows:
- Temperature (°C)
- Fan PWM value
- Fan RPM

## Control Logic
The system uses predefined temperature ranges to control fan speed automatically without human intervention.
