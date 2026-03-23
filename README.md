# first project kinda nervous

Record IR signals from any remote control and replay them on demand.  
Built for **M5StickC Plus 2**.

---

## What You Need to Buy

### 1. M5StickC Plus 2 (the main device)
- **Price:** ~$20
- **Where:** [M5Stack Official Store](https://shop.m5stack.com/products/m5stickc-plus2-esp32-mini-iot-development-kit), AliExpress, Amazon
- This is the brain — ESP32 microcontroller with a 240×135 LCD screen, two buttons, built-in battery, and a **built-in IR transmitter LED** (GPIO 19).

### 2. IR Receiver Module (required for recording signals)
The M5StickC Plus 2 can **send** IR but has **no built-in IR receiver**. You must add one externally.

**Option A — Plug & Play (recommended):**
- **M5Stack Mini Infrared Emitter & Receiver Unit (SKU: U002)** — ~$4
- Connects directly to the Grove Port A on the M5StickC Plus 2 with a Grove cable (included)
- No soldering, no wiring

**Option B — DIY (cheaper):**
- **VS1838B** or **TSOP38238** IR receiver module — ~$1
- 3× female-to-female jumper wires — ~$1

### 3. USB-C Cable (for flashing & charging)
- Any USB-C data cable (not charge-only)

---

## Wiring (Option B — DIY IR Receiver)

Connect the IR receiver to the **Grove Port A** connector on the M5StickC Plus 2:

| IR Receiver Pin | M5StickC Plus 2     | Wire Color (Grove) |
|-----------------|----------------------|---------------------|
| VCC             | 5V                   | Red                 |
| GND             | GND                  | Black               |
| OUT / Signal    | GPIO 33              | Yellow              |

> If using the M5Stack IR Unit (Option A), just plug the Grove cable in — no wiring needed.

---

## Features

- **10 IR signal storage slots** — record and store up to 10 remote signals
- **Protocol-aware sending** — supports NEC, Sony, Samsung, RC5, RC6, LG, Panasonic, and 50+ more
- **Strong signal** — sends 5 repeat bursts by default (configurable 1–10×) for maximum reliability
- **4 color themes** — Purple, Green, Blue, Red — changeable in Settings
- **Clock & date** — syncs via WiFi/NTP on boot (optional), shown on status bar
- **Battery indicator** — shown on status bar
- **Welcome screen** — "ROGAL" branded splash with battery, time and date
- **Auto power-off** — configurable timeout (0–30 min)
- **Clean button handling** — no double-fire, short press on release, long press on threshold

---

## Controls

### Slots Screen (main view)

| Button        | Action                                     |
|---------------|--------------------------------------------|
| A short       | Previous slot                              |
| A hold (2s)   | Open Settings                              |
| B short       | **Send** (if stored) / **Record** (if empty) |
| B hold (1s)   | Next slot                                  |
| A or B (while recording) | Cancel recording                |

### Settings Screen

| Button        | Action                          |
|---------------|---------------------------------|
| A short       | Decrease selected value         |
| A hold (1s)   | Select next setting row         |
| B short       | Increase selected value         |
| B hold (1.5s) | Back to Slots                   |

**Settings available:**
- **Repeats** (1–10×) — how many times to send the signal per press
- **Auto-Off** (0–30 min) — idle timeout before power-off (0 = never)
- **Theme** — Purple / Green / Blue / Red

---

## How to Flash

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- Or PlatformIO CLI: `pip install platformio`

### Steps
1. Clone this repo
2. Connect M5StickC Plus 2 via USB-C
3. *(Optional)* Edit `WIFI_SSID` and `WIFI_PASS` in `src/main.cpp` for clock/date sync
4. Flash:
   ```
   pio run --target upload
   ```
5. The device will reboot and show the ROGAL welcome screen

---

## How to Use

1. **Power on** — welcome screen shows battery % and time (if WiFi configured)
2. **Press any button** to enter Slots view
3. **Navigate** to an empty slot with A (previous) / hold B (next)
4. **Press B** on an empty slot to start recording
5. **Aim your remote** at the IR receiver and press a button on the remote
6. The signal is captured, decoded, and stored — screen shows "SAVED"
7. **Press B** on a stored slot to replay the signal (IR LED fires 5× by default)
8. **Hold A** to open Settings and customize repeats, auto-off, or theme color

---

## Project Structure

```
├── src/
│   └── main.cpp          # All firmware code
├── platformio.ini         # PlatformIO build config
├── .gitignore
└── README.md
```

---

## Technical Details

- **MCU:** ESP32-PICO-V3-02 (240 MHz dual-core, 320 KB RAM, 4 MB Flash)
- **Display:** 240×135 px TFT LCD (landscape mode)
- **IR TX:** GPIO 19 (built-in LED, 38 kHz carrier)
- **IR RX:** GPIO 33 (external receiver on Grove Port A)
- **IR Library:** IRremoteESP8266 v2.9.0
- **Timezone:** CET/CEST via POSIX TZ string + NTP

---

## License

Private project.

## Libraries Used
- M5StickCPlus2 - Device support
- IRremoteESP8266 - IR sending and receiving
