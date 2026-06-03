# Rogal — M5StickC Plus 2 IR Remote

> My first embedded project — small, scary at first, and surprisingly fun once
> the LED started blinking back on cue. Record IR signals from any remote and
> replay them on demand.

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

- **20 IR signal storage slots** — record and store up to 20 remote signals (paged view, 10 per page)
- **NVS persistence** — all settings and recorded IR signals survive reboots (saved to flash)
- **Main menu** — new hub screen with IR Remote, Clock, and Settings entries
- **Full-screen clock** — large time display with seconds, date, day of week, battery, and adjustable hour/minute offset
- **TV presets database** — load pre-built IR codes for popular TV brands straight into slots
- **Protocol-aware sending** — supports NEC, Sony, Samsung, RC5, RC6, LG, Panasonic, and 50+ more
- **Strong signal** — sends 50 repeat bursts by default (configurable 1–50×) for maximum reliability
- **4 color themes** — Purple, Green, Blue, Red — changeable in Settings
- **Clock & date** — syncs via WiFi/NTP on boot (optional), shown on status bar with adjustable offset
- **Battery indicator** — shown on status bar
- **Welcome screen** — "ROGAL" branded splash with battery, time and date
- **Auto power-off** — configurable timeout (0–30 min)
- **Clean button handling** — no double-fire, short press on release, long press on threshold

---

## Controls

### Main Menu

| Button        | Action                          |
|---------------|---------------------------------|
| A short       | Move selection up                |
| B short       | Select highlighted item          |
| P (power)     | Move selection down              |

### Slots Screen (IR Remote)

| Button        | Action                                     |
|---------------|--------------------------------------------|
| B short       | Previous slot                              |
| B hold        | Back to Main Menu                          |
| A short       | **Send** (if stored) / **Record** (if empty) |
| P short       | Next slot                                  |
| A or B (while recording) | Cancel recording                |

### Clock Screen

| Button        | Action                          |
|---------------|---------------------------------|
| B short       | Toggle hour/minute edit mode     |
| B hold        | Back to Main Menu                |
| A short       | Decrease offset (hour or minute) |
| P short       | Increase offset (hour or minute) |

### Settings Screen

| Button        | Action                          |
|---------------|---------------------------------|
| B short       | Decrease selected value          |
| A short       | Increase selected value          |
| A hold        | Back to Main Menu                |
| P short       | Next setting row                 |

**Settings available:**
- **Repeats** (1–50×) — how many times to send the signal per press
- **Auto-Off** (0–30 min) — idle timeout before power-off (0 = never)
- **Theme** — Purple / Green / Blue / Red
- **Clock Offset** — adjustable hour offset (−12 to +12) for the clock display
- **TV Presets** — load pre-built IR codes for popular TV brands

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

1. **Power on** — welcome screen shows battery %, time, and date (if WiFi configured)
2. **Press any button** to enter the Main Menu
3. Select **IR Remote** to open the slots view
4. **Navigate** to an empty slot with B (previous) / P (next)
5. **Press A** on an empty slot to start recording
6. **Aim your remote** at the IR receiver and press a button on the remote
7. The signal is captured, decoded, and stored in flash — screen shows "SAVED"
8. **Press A** on a stored slot to replay the signal (IR LED fires 50× by default)
9. Select **Clock** from the menu for a full-screen clock with adjustable offset
10. Select **Settings** to customize repeats, auto-off, theme, clock offset, or load TV presets

All settings and recorded IR signals are **saved to flash** and persist across reboots.

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
- **Storage:** ESP32 NVS (Preferences) — settings and IR signals persist across reboots
- **Timezone:** CET/CEST via POSIX TZ string + NTP

---

## License

Private project.

## Libraries Used
- M5StickCPlus2 — Device support
- IRremoteESP8266 — IR sending and receiving
- Preferences (ESP32 NVS) — Persistent settings and signal storage
