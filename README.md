# 📡 Universal IR Remote & Utility Hub

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)](https://platformio.org/)
[![Hardware](https://img.shields.io/badge/Hardware-M5StickC%20Plus%202-red.svg)](https://shop.m5stack.com/products/m5stickc-plus2-esp32-mini-iot-development-kit)
[![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20ESP32-blue.svg)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A high-performance, protocol-aware Infrared (IR) remote recorder and utility system designed for the **M5StickC Plus 2**. This project transforms an ESP32-based handheld into a robust tool capable of capturing, storing, and replaying IR signals for a wide variety of consumer electronics.

---

## 🚀 Key Features

### 📡 IR Command Management
- **Protocol-Aware Capture**: Supports 50+ IR protocols including NEC, Sony, Samsung, RC5, RC6, LG, and Panasonic.
- **20 Persistent Storage Slots**: Record and categorize signals across a paged interface (10 slots per page).
- **Non-Volatile Storage (NVS)**: All captured signals and user settings are stored in flash memory, persisting across reboots.
- **TV Preset Database**: Includes a built-in library of common IR codes for major television brands for instant deployment.
- **Signal Optimization**: Configurable repeat bursts (1–50×) ensuring maximum reliability across different receiver sensitivities.

### 🛠️ Multi-Utility Hub
- **Dynamic Main Menu**: Centralized navigation for IR Remote, Clock, and System Settings.
- **Feature-Rich Clock**: Large format time/date display with NTP synchronization, battery telemetry, and manual timezone offsets.
- **Advanced Power Management**: Configurable auto power-off (0–30 min) to preserve battery life during portable use.
- **Customizable UI**: Four built-in color themes (Purple, Green, Blue, Red) with high-contrast UI elements for the 240×135 LCD.

---

## ⚙️ Hardware Architecture

Built for the **M5StickC Plus 2** (ESP32-S3), leveraging:
- **Built-in IR Transmitter**: GPIO 19
- **LCD Screen**: ST7789v2 Driver
- **User Inputs**: Three-button interface with optimized debouncing and long-press detection.
- **Peripheral Support**: Compatible with the M5Stack IR Receiver Unit (Grove Port A / GPIO 33) or DIY VS1838B sensors.

---

## 🏗️ Technical Implementation

### Engineering Highlights
- **Interrupt-Driven Logic**: Clean button handling prevents double-firing and supports short-press/long-press thresholds.
- **Efficient Memory Usage**: Optimized NVS structures for storing RAW and protocol-based IR data.
- **Structured C++**: A modular `main.cpp` (40KB+) implementation managing complex state transitions between utilities.

---

## 📥 Getting Started

1. Install [PlatformIO](https://platformio.org/install).
2. Clone the repository.
3. Connect your M5StickC Plus 2 via USB-C.
4. Build and upload the project using the PlatformIO IDE or CLI:
   ```bash
   pio run --target upload
   ```

---

## 📜 License
This project is licensed under the MIT License.
