# 📡 Universal IR Remote (M5StickC Plus 2)

I built this because I wanted a tiny device that could control basically any TV or remote-controlled gadget I have. It's based on the **M5StickC Plus 2** and can record signals from a real remote and play them back whenever I want.

---

## 🚀 Cool Features

- **20 Slots**: You can save up to 20 different remote signals in its memory.
- **NVS Saving**: It saves everything to the flash memory, so even if the battery dies, it remembers your remotes.
- **TV Presets**: I added a built-in database of codes for common TV brands so you don't even have to record them yourself.
- **Clock Utility**: It's also a full-screen clock that syncs over WiFi using NTP.
- **Themes**: You can switch between Purple, Green, Blue, and Red themes in the settings.

---

## 🛠️ Hardware

- **Brain**: M5StickC Plus 2 (ESP32-S3)
- **IR Receiver**: I used the external M5Stack IR Unit plugged into Port A.
- **IR Transmitter**: Uses the one built right into the stick.

---

## 🏗️ How I made it

I used **PlatformIO** and C++ to write this. The code handles everything from low-level IR pulse timing to the paged menu system on the LCD screen. I spent a lot of time making sure the buttons don't "double-click" and that you can hold them down for extra functions.

---

## 📥 How to use it

1. Get **PlatformIO**.
2. Connect your M5Stick via USB-C.
3. Upload the code: `pio run --target upload`
