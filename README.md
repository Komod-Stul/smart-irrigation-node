# 🪴 Smart Irrigation & Air Quality Monitoring Node (ESP32-C3)

An autonomous IoT embedded node built on **ESP32-C3** that automates plant irrigation based on soil moisture levels, monitors indoor air quality in real-time, and sends instant alert logs via a **Telegram Bot**.

---

## 🎯 Project Overview (What It Does)

This project is an automated, smart plant care and environmental monitoring station designed to solve two common indoor gardening problems: **over-watering / pump burnout** and **poor room ventilation**.

- **Automates Plant Watering**: Continuously monitors soil moisture via a capacitive sensor and triggers a 5V pump only when the plant actually needs water.
- **Prevents Flooding & Hardware Damage**: Employs a multi-stage watering logic with 60-second soak intervals and an automatic 30-minute lockout if the water tank runs dry.
- **Monitors Indoor Air Quality**: Tracks $eCO2$ and $TVOC$ levels in real-time to detect stagnant or poor air quality in the room.
- **Sends Instant Telegram Alerts**: Keeps you informed about watering events, low moisture levels, emergency lockouts, and air quality warnings directly on your phone.

---

---

## 📹 Video Demonstration

[![Smart Irrigation Node Demo](https://img.youtube.com/vi/jKkD71rhWAg/maxresdefault.jpg)](https://www.youtube.com/watch?v=jKkD71rhWAg)

> 📌 *Click the thumbnail above to watch the full hardware demonstration and Telegram integration on YouTube.*

## 🎬 How It Works (Demonstrated Features)

- **Smart Watering Cycle**: Activates the 5V pump for 3 seconds, then waits **60 seconds** for water to soak into the soil before taking another reading.
- **Fail-Safe Lockout**: After 3 unsuccessful attempts, locks the system for **30 minutes** and dispatches a critical Telegram alert to prevent pump damage.
- **Auto-Reset**: Instantly clears the emergency lockout as soon as moisture levels return to normal.
- **Real-Time Air Alerts**: Samples SGP30 gas sensor every 2 seconds and fires Telegram notifications if $eCO2$ or $TVOC$ exceed safety limits.

---

## 🛠 Hardware Components & Pinout Specifications

| Component | Exact Model / Module | Interface / Pin | Description |
| :--- | :--- | :--- | :--- |
| **Microcontroller** | ESP32-C3 DevKitM-1 (Espressif) | USB-C / WiFi | 32-bit RISC-V core @ 160MHz with Wi-Fi & BLE |
| **Air Quality Sensor** | Seeed Studio Grove - VOC and eCO2 Gas Sensor (SGP30 v1.1) | I2C (SDA: GPIO 8, SCL: GPIO 9) | Multi-pixel gas sensor tracking eCO2 (ppm) and TVOC (ppb) |
| **Soil Moisture Sensor**| Seeed Studio Grove - Capacitive Moisture Sensor v1.0 | Analog (GPIO 1 / ADC) | Corrosion-resistant capacitive soil moisture sensing |
| **Pump Driver Module** | Seeed Studio Grove - MOSFET v1.1 | Digital OUT (GPIO 3) | High-power MOSFET switch controlling the 5V power line |
| **Water Pump** | Generic 5V DC Micro Submersible Water Pump | Driven via MOSFET | Delivers targeted water pulses during irrigation cycles |

## 🔌 Detailed Wiring & Power Schematic

### 1. Pin-by-Pin Connection Matrix

| Module / Sensor | Module Pin | ESP32-C3 Pin | Cable / Signal Type |
| :--- | :--- | :--- | :--- |
| **SGP30 Air Quality Sensor** | **VCC** <br> **GND** <br> **SDA** <br> **SCL** | **3.3V** <br> **GND** <br> **GPIO 8** <br> **GPIO 9** | Power (3.3V) <br> Ground <br> I2C Data <br> I2C Clock |
| **Capacitive Soil Sensor** | **VCC** <br> **GND** <br> **AOUT (SIG)** | **3.3V** <br> **GND** <br> **GPIO 1** | Power (3.3V) <br> Ground <br> Analog Read (ADC) |
| **Seeed MOSFET Module v1.1**| **VCC** <br> **GND** <br> **SIG** | **5V** <br> **GND** <br> **GPIO 3** | Main Power Rail (5V) <br> Ground <br> Digital Output High/Low |
| **5V Water Pump** | **Red (+)** <br> **Black (-)** | **MOSFET OUT (+)** <br> **MOSFET OUT (-)** | Switched 5V Power <br> Switched Ground Line |

---


## Drivers & PC Connection Setup

To connect and flash the ESP32-C3 board via VS Code & PlatformIO on Windows:
- **USB-to-UART / Native USB Driver**: Installed **Silicon Labs CP210x VCP Driver** (or *Espressif USB JTAG/Serial Driver*) to recognize the board under `COM3`.
- **Serial Monitor Configuration**: Configured at **115200 baud rate** in `platformio.ini` (`monitor_speed = 115200`).

---

## ⚙️ Core System Logic & State Machine

1. **3-Stage Irrigation Cycle**:
   - Triggers when soil analog raw reading exceeds threshold (`ADC > 2600`).
   - Runs pump for **3 seconds**, then pauses for a **60-second soak-in interval** to allow water absorption before reading sensors again.
2. **Fail-Safe Emergency Lockout**:
   - If soil remains dry after **3 consecutive attempts**, system engages a **30-minute Emergency Lockout**.
   - Prevents pump burnout, water overflow, or flooding if the water reservoir is empty.
3. **Air Quality Threshold Monitoring**:
   - Continuously samples SGP30 every 2 seconds.
   - Sends Telegram alerts if $eCO2 > 1000\text{ ppm}$ or $TVOC > 300\text{ ppb}$ (cooldown-limited to 1 alert per 10 minutes).

---

## 🚧 Engineering Challenges & Troubleshooting

During the development and deployment of this project, several real-world technical issues were encountered and resolved:

### 1. PlatformIO Library Dependency Resolution
* **Issue**: Specifying custom Git URLs or non-canonical package strings for `UniversalTelegramBot` caused package resolution failures (`UnknownPackageError`) and triggered interactive GitHub authentication prompts during build execution.
* **Solution**: Cleaned `platformio.ini` to use direct canonical registry names (`UniversalTelegramBot` and `bblanchon/ArduinoJson @ ^6.21.5`), eliminating Git authorization popups and enabling automated build resolution.

### 2. C++ IntelliSense Header Warnings
* **Issue**: VS Code C/C++ extension flagged missing headers (`#include <UniversalTelegramBot.h>`) upon creating the project structure.
* **Solution**: Recognized that PlatformIO downloads remote dependencies upon first compilation task (`Build` / `Upload`), resolving headers into local `.pio` build environments.

### 3. Git Identity Configuration on Windows
* **Issue**: Initial repository commit failed with `Author identity unknown` error in Windows PowerShell / VS Code terminal.
* **Solution**: Configured global user identity attributes (`git config --global user.email` & `git config --global user.name`) to register proper author metadata prior to pushing commits.

### 4. Soil Moisture Sensor Calibration & False Triggers
* **Issue**: Raw analog values fluctuated drastically, risking continuous pump triggering.
* **Solution**: Implemented raw ADC threshold validation ($100 < ADC < 4095$) and dual thresholds (`DRY_THRESHOLD = 2600`, `WET_THRESHOLD = 2100`) to create a stable hysteresis loop.

---

## 🤖 Telegram Bot Setup Guide

1. **Create Bot**: Open Telegram, search for `@BotFather`, and send `/newbot` to create a new bot instance and retrieve your `BOT_TOKEN`.
2. **Obtain Chat ID**: Send a message to `@userinfobot` or `@raw_data_bot` to find your personal `CHAT_ID`.
3. **Configure Credentials**: Update the credentials header in `src/main.cpp`:
   ```cpp
   #define BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
   #define CHAT_ID "YOUR_TELEGRAM_CHAT_ID"

---

## 📁 Repository Structure

```text
smart-irrigation-node/
├── src/
│   └── main.cpp          # Full embedded firmware logic (C++)
├── platformio.ini        # PlatformIO configuration, monitor settings & dependencies
└── README.md             # Detailed documentation
