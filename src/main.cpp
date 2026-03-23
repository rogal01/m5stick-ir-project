// ============================================================
//  ROGAL — IR Signal Manager for M5StickC Plus 2
//  https://github.com/oliwi  (private repo)
// ============================================================
//
//  HARDWARE REQUIRED:
//    1. M5StickC Plus 2
//    2. External IR receiver (VS1838B / TSOP38238 / M5Stack IR Unit)
//       Wiring: VCC→5V, GND→GND, OUT→GPIO 33 (Grove Port A)
//
//  The M5StickC Plus 2 has a built-in IR transmitter LED (GPIO 19)
//  but NO built-in IR receiver — you must add one externally.
//
//  Set WiFi credentials below for clock/date sync (optional).
// ============================================================

#include <M5StickCPlus2.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <WiFi.h>
#include <Preferences.h>

// ==================== USER CONFIG ====================
const char* WIFI_SSID = "";   // Your WiFi SSID (leave "" to skip time sync)
const char* WIFI_PASS = "";   // Your WiFi password

// ==================== HARDWARE PINS ====================
const uint16_t IR_RX = 33;    // External IR receiver on Grove Port A
const uint16_t IR_TX = 19;    // Built-in IR LED

// ==================== IR OBJECTS ====================
IRrecv irrecv(IR_RX, 1024, 50, true);
IRsend irsend(IR_TX);
decode_results results;

// ==================== SIGNAL STORAGE ====================
const int NUM_SLOTS = 20;
struct IrSlot {
  uint64_t   code;
  uint16_t   bits;
  decode_type_t proto;
  bool       occupied;
};
IrSlot slots[NUM_SLOTS] = {};
int  currentSlot = 0;
bool recording   = false;

// ==================== SETTINGS ====================
int repeatCount     = 50;  // strong default: 50 bursts per send
int autoOffMinutes  = 5;
int themeIdx        = 0;
int clockHourOffset = 0;   // adjustable hour offset for clock (-12 to +12)
int clockMinuteOffset = 0; // adjustable minute offset for clock (-59 to +59)

// ==================== APP STATE ====================
enum Screen { SCR_WELCOME, SCR_MENU, SCR_SLOTS, SCR_CLOCK, SCR_SETTINGS, SCR_PRESETS };
Screen screen       = SCR_WELCOME;
int    menuSel      = 0;          // which menu row is selected
int    settingSel   = 0;          // which setting row is selected
unsigned long lastActivity    = 0;
unsigned long welcomeStart    = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastClockDraw   = 0;
int    clockEditMode = 0;      // 0=hour, 1=minute editing on clock screen
bool   hasTime      = false;
Preferences prefs;

// ==================== BUTTON TRACKER ====================
// Fires SHORT on release (only if threshold not reached).
// Fires LONG once when threshold reached while held.
enum BtnEvt { BTN_NONE, BTN_SHORT, BTN_LONG };
struct BtnTrk {
  bool  down;
  bool  longFired;
  unsigned long t;
};
BtnTrk tA = {}, tB = {}, tP = {};

BtnEvt checkBtn(BtnTrk &s, bool pressed, unsigned long longMs) {
  BtnEvt r = BTN_NONE;
  if (pressed) {
    if (!s.down) { s.down = true; s.longFired = false; s.t = millis(); }
    else if (!s.longFired && millis() - s.t >= longMs) { s.longFired = true; r = BTN_LONG; }
  } else {
    if (s.down && !s.longFired) r = BTN_SHORT;
    s.down = false;
  }
  return r;
}

// ==================== COLOR THEMES ====================
struct Theme {
  const char* name;
  uint16_t primary;   // main accent
  uint16_t secondary; // softer accent
  uint16_t value;     // highlights / numbers
  uint16_t muted;     // separators / dim text
  uint16_t bar;       // status-bar background
};
const Theme THEMES[] = {
  {"Purple", 0xA01F, 0xD69A, 0xFE20, 0x4228, 0x2008},
  {"Green",  0x07E0, 0x4EC6, 0x07FF, 0x2124, 0x01E0},
  {"Blue",   0x34BF, 0x6B5F, 0xFFE0, 0x4228, 0x0010},
  {"Red",    0xF800, 0xFD20, 0xFFE0, 0x4228, 0x3000},
};
const int NUM_THEMES = 4;
inline const Theme& T() { return THEMES[themeIdx]; }

// ==================== UI HELPERS ====================
void drawCentered(int y, const char* txt, uint16_t color, int sz = 1) {
  M5.Lcd.setTextSize(sz);
  M5.Lcd.setTextColor(color);
  M5.Lcd.setCursor((240 - (int)strlen(txt) * 6 * sz) / 2, y);
  M5.Lcd.print(txt);
}

void drawProgressBar(int x, int y, int w, int h, float pct, uint16_t fg, uint16_t bg) {
  M5.Lcd.drawRect(x, y, w, h, fg);
  int fill = (int)((w - 2) * pct);
  if (fill > 0) M5.Lcd.fillRect(x + 1, y + 1, fill, h - 2, fg);
  if (fill < w - 2) M5.Lcd.fillRect(x + 1 + fill, y + 1, w - 2 - fill, h - 2, bg);
}

void drawGear(int cx, int cy, uint16_t color) {
  M5.Lcd.fillCircle(cx, cy, 3, color);
  M5.Lcd.fillCircle(cx, cy, 1, BLACK);
  M5.Lcd.fillRect(cx - 1, cy - 5, 3, 2, color);
  M5.Lcd.fillRect(cx - 1, cy + 4, 3, 2, color);
  M5.Lcd.fillRect(cx - 5, cy - 1, 2, 3, color);
  M5.Lcd.fillRect(cx + 4, cy - 1, 2, 3, color);
}

void drawButtonBar(const char* content, const char* right = nullptr) {
  M5.Lcd.drawFastHLine(0, 119, 240, T().muted);
  M5.Lcd.fillRect(0, 120, 240, 15, T().bar);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(T().secondary);
  M5.Lcd.setCursor(2, 124);
  M5.Lcd.print(content);
  if (right) {
    M5.Lcd.setCursor(240 - (int)strlen(right) * 6 - 2, 124);
    M5.Lcd.print(right);
  }
}

// ==================== NVS PERSISTENCE ====================
void saveSettings() {
  if (!prefs.begin("rogal", false)) {
    Serial.println("[NVS] ERROR: failed to open namespace");
    return;
  }
  prefs.putInt("repeat", repeatCount);
  prefs.putInt("autoOff", autoOffMinutes);
  prefs.putInt("theme", themeIdx);
  prefs.putInt("clockOff", clockHourOffset);
  prefs.putInt("clockMinOff", clockMinuteOffset);
  prefs.end();
  Serial.printf("[NVS] Saved: rep=%d off=%d theme=%d hOff=%d mOff=%d\n",
                repeatCount, autoOffMinutes, themeIdx, clockHourOffset, clockMinuteOffset);
}

void loadSettings() {
  if (!prefs.begin("rogal", false)) {
    Serial.println("[NVS] ERROR: failed to open for load");
    return;
  }
  repeatCount       = prefs.getInt("repeat", 50);
  autoOffMinutes    = prefs.getInt("autoOff", 5);
  themeIdx          = prefs.getInt("theme", 0);
  clockHourOffset   = prefs.getInt("clockOff", 0);
  clockMinuteOffset = prefs.getInt("clockMinOff", 0);
  prefs.end();
  repeatCount       = constrain(repeatCount, 1, 50);
  autoOffMinutes    = constrain(autoOffMinutes, 0, 30);
  themeIdx          = constrain(themeIdx, 0, NUM_THEMES - 1);
  clockHourOffset   = constrain(clockHourOffset, -12, 12);
  clockMinuteOffset = constrain(clockMinuteOffset, -59, 59);
  Serial.printf("[NVS] Loaded: rep=%d off=%d theme=%d hOff=%d mOff=%d\n",
                repeatCount, autoOffMinutes, themeIdx, clockHourOffset, clockMinuteOffset);
}

void saveSlot(int idx) {
  if (idx < 0 || idx >= NUM_SLOTS) return;
  prefs.begin("rogal", false);
  char key[12];
  snprintf(key, sizeof(key), "s%d_code", idx);
  prefs.putULong64(key, slots[idx].code);
  snprintf(key, sizeof(key), "s%d_bits", idx);
  prefs.putUShort(key, slots[idx].bits);
  snprintf(key, sizeof(key), "s%d_proto", idx);
  prefs.putInt(key, (int)slots[idx].proto);
  snprintf(key, sizeof(key), "s%d_occ", idx);
  prefs.putBool(key, slots[idx].occupied);
  prefs.end();
}

void loadSlots() {
  prefs.begin("rogal", true);
  for (int i = 0; i < NUM_SLOTS; i++) {
    char key[12];
    snprintf(key, sizeof(key), "s%d_occ", i);
    slots[i].occupied = prefs.getBool(key, false);
    if (slots[i].occupied) {
      snprintf(key, sizeof(key), "s%d_code", i);
      slots[i].code = prefs.getULong64(key, 0);
      snprintf(key, sizeof(key), "s%d_bits", i);
      slots[i].bits = prefs.getUShort(key, 0);
      snprintf(key, sizeof(key), "s%d_proto", i);
      slots[i].proto = (decode_type_t)prefs.getInt(key, 0);
    }
  }
  prefs.end();
  int loaded = 0;
  for (int i = 0; i < NUM_SLOTS; i++) if (slots[i].occupied) loaded++;
  Serial.printf("[NVS] Loaded %d/%d slots\n", loaded, NUM_SLOTS);
}

// ==================== TV PRESETS DATABASE ====================
struct TvPreset {
  const char* name;
  uint64_t    code;
  uint16_t    bits;
  decode_type_t proto;
};
struct TvBrand {
  const char* name;
  TvPreset    codes[5];   // Power, Vol+, Vol-, Ch+, Ch-
};
const TvBrand TV_BRANDS[] = {
  {"Samsung", {
    {"Power",  0xE0E040BF, 32, decode_type_t::NEC},
    {"Vol +",  0xE0E0E01F, 32, decode_type_t::NEC},
    {"Vol -",  0xE0E0D02F, 32, decode_type_t::NEC},
    {"Ch +",   0xE0E048B7, 32, decode_type_t::NEC},
    {"Ch -",   0xE0E008F7, 32, decode_type_t::NEC},
  }},
  {"LG", {
    {"Power",  0x20DF10EF, 32, decode_type_t::NEC},
    {"Vol +",  0x20DF40BF, 32, decode_type_t::NEC},
    {"Vol -",  0x20DFC03F, 32, decode_type_t::NEC},
    {"Ch +",   0x20DF00FF, 32, decode_type_t::NEC},
    {"Ch -",   0x20DF807F, 32, decode_type_t::NEC},
  }},
  {"Sony", {
    {"Power",  0xA90,  12, decode_type_t::SONY},
    {"Vol +",  0x490,  12, decode_type_t::SONY},
    {"Vol -",  0xC90,  12, decode_type_t::SONY},
    {"Ch +",   0x090,  12, decode_type_t::SONY},
    {"Ch -",   0x890,  12, decode_type_t::SONY},
  }},
  {"Philips", {
    {"Power",  0x0C, 12, decode_type_t::RC5},
    {"Vol +",  0x10, 12, decode_type_t::RC5},
    {"Vol -",  0x11, 12, decode_type_t::RC5},
    {"Ch +",   0x20, 12, decode_type_t::RC5},
    {"Ch -",   0x21, 12, decode_type_t::RC5},
  }},
  // ── Additional TVs ──
  {"Toshiba", {
    {"Power",  0x02FD48B7, 32, decode_type_t::NEC},
    {"Vol +",  0x02FD58A7, 32, decode_type_t::NEC},
    {"Vol -",  0x02FD7887, 32, decode_type_t::NEC},
    {"Ch +",   0x02FDD827, 32, decode_type_t::NEC},
    {"Ch -",   0x02FDF807, 32, decode_type_t::NEC},
  }},
  {"Sharp", {
    {"Power",  0xA55A38C7, 32, decode_type_t::NEC},
    {"Vol +",  0xA55A50AF, 32, decode_type_t::NEC},
    {"Vol -",  0xA55AD02F, 32, decode_type_t::NEC},
    {"Ch +",   0xA55A18E7, 32, decode_type_t::NEC},
    {"Ch -",   0xA55A9867, 32, decode_type_t::NEC},
  }},
  {"Xiaomi", {
    {"Power",  0x44BB807F, 32, decode_type_t::NEC},
    {"Vol +",  0x44BB40BF, 32, decode_type_t::NEC},
    {"Vol -",  0x44BBC03F, 32, decode_type_t::NEC},
    {"Ch +",   0x44BB00FF, 32, decode_type_t::NEC},
    {"Ch -",   0x44BB609F, 32, decode_type_t::NEC},
  }},
  {"TCL", {
    {"Power",  0x40BF12ED, 32, decode_type_t::NEC},
    {"Vol +",  0x40BF40BF, 32, decode_type_t::NEC},
    {"Vol -",  0x40BFC03F, 32, decode_type_t::NEC},
    {"Ch +",   0x40BF00FF, 32, decode_type_t::NEC},
    {"Ch -",   0x40BF807F, 32, decode_type_t::NEC},
  }},
  // ── Projectors / Interactive Boards ──
  {"Epson Proj", {
    {"Power",  0xC1AA09F6, 32, decode_type_t::NEC},
    {"Source", 0xC1AA8976, 32, decode_type_t::NEC},
    {"Menu",   0xC1AA7986, 32, decode_type_t::NEC},
    {"Vol +",  0xC1AA817E, 32, decode_type_t::NEC},
    {"Vol -",  0xC1AA41BE, 32, decode_type_t::NEC},
  }},
  {"BenQ Proj", {
    {"Power",  0xCF3000FF, 32, decode_type_t::NEC},
    {"Source", 0xCF3040BF, 32, decode_type_t::NEC},
    {"Menu",   0xCF30807F, 32, decode_type_t::NEC},
    {"Vol +",  0xCF30C03F, 32, decode_type_t::NEC},
    {"Vol -",  0xCF3020DF, 32, decode_type_t::NEC},
  }},
  {"Avtek", {
    {"Power",  0x01FE48B7, 32, decode_type_t::NEC},
    {"Source", 0x01FE807F, 32, decode_type_t::NEC},
    {"Menu",   0x01FE40BF, 32, decode_type_t::NEC},
    {"Vol +",  0x01FE20DF, 32, decode_type_t::NEC},
    {"Vol -",  0x01FEA05F, 32, decode_type_t::NEC},
  }},
};
const int NUM_BRANDS = 11;
int presetBrandIdx = 0;
int presetStartSlot = 0;
int presetScroll = 0;

// ==================== STATUS BAR ====================
void drawStatusBar() {
  M5.Lcd.fillRect(0, 0, 240, 16, T().bar);
  M5.Lcd.setTextSize(1);
  // Brand
  M5.Lcd.setTextColor(T().primary);
  M5.Lcd.setCursor(2, 4);
  M5.Lcd.print("ROGAL");
  // Clock + date (real time if NTP synced, uptime otherwise)
  if (hasTime) {
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
      int totalMin = ti.tm_hour * 60 + ti.tm_min + clockHourOffset * 60 + clockMinuteOffset;
      while (totalMin < 0) totalMin += 1440;
      totalMin %= 1440;
      int adjH = totalMin / 60, adjM = totalMin % 60;
      char buf[18];
      snprintf(buf, sizeof(buf), "%02d:%02d %02d/%02d", adjH, adjM, ti.tm_mday, ti.tm_mon + 1);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setCursor(85, 4);
      M5.Lcd.print(buf);
    }
  } else {
    unsigned long sec = millis() / 1000;
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "UP %02d:%02d", h, m);
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.setCursor(90, 4);
    M5.Lcd.print(buf);
  }
  // Battery + charging indicator
  int pct = M5.Power.getBatteryLevel();
  bool charging = M5.Power.isCharging();
  char bp[10];
  if (charging)
    snprintf(bp, sizeof(bp), "%c%d%%", 0x11, pct);  // arrow + percent = plugged in
  else
    snprintf(bp, sizeof(bp), "%d%%", pct);
  int batX = 238 - (int)strlen(bp) * 6;
  // IR receiver status (idle receiver pulls HIGH; pull-down keeps LOW when absent)
  bool irOk = digitalRead(IR_RX) == HIGH;
  M5.Lcd.setTextColor(irOk ? 0x07E0 : 0xF800);
  M5.Lcd.setCursor(batX - 18, 4);
  M5.Lcd.print("IR");
  // Battery
  M5.Lcd.setTextColor(charging ? 0x07E0 : (pct > 20 ? T().value : 0xF800));
  M5.Lcd.setCursor(batX, 4);
  M5.Lcd.print(bp);
  M5.Lcd.drawFastHLine(0, 16, 240, T().muted);
}

// ==================== SCREENS ====================
void showWelcome() {
  M5.Lcd.fillScreen(BLACK);
  welcomeStart = millis();
  // Dotted decorative lines
  for (int i = 0; i < 240; i += 3)
    M5.Lcd.drawPixel(i, 2, T().muted);
  for (int i = 0; i < 240; i += 3)
    M5.Lcd.drawPixel(i, 132, T().muted);
  // Top accent line sweep
  for (int i = 0; i <= 120; i += 6) {
    M5.Lcd.drawFastHLine(120 - i, 8, i * 2, T().primary);
    delay(3);
  }
  // Title — letter by letter
  const char* title = "ROGAL";
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(T().primary);
  for (int c = 0; c < 5; c++) {
    M5.Lcd.setCursor(60 + c * 18, 18);
    M5.Lcd.print(title[c]);
    delay(50);
  }
  delay(80);
  // Tagline
  drawCentered(48, "IR Signal Manager", T().secondary);
  // Triple-line separator
  M5.Lcd.drawFastHLine(30, 61, 180, T().muted);
  M5.Lcd.drawFastHLine(50, 63, 140, T().primary);
  M5.Lcd.drawFastHLine(30, 65, 180, T().muted);
  // Battery + time
  delay(40);
  int pct = M5.Power.getBatteryLevel();
  char info[36];
  if (hasTime) {
    struct tm ti; getLocalTime(&ti, 0);
    int totalMin = ti.tm_hour * 60 + ti.tm_min + clockHourOffset * 60 + clockMinuteOffset;
    while (totalMin < 0) totalMin += 1440;
    totalMin %= 1440;
    int adjH = totalMin / 60, adjM = totalMin % 60;
    snprintf(info, sizeof(info), "%02d:%02d  %02d/%02d/%04d  %d%%",
             adjH, adjM, ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900, pct);
  } else {
    snprintf(info, sizeof(info), "BAT %d%%", pct);
  }
  drawCentered(74, info, T().value);
  // Version
  delay(30);
  drawCentered(92, "v3.1.0", T().muted);
  // Bottom accent sweep
  for (int i = 0; i <= 120; i += 6) {
    M5.Lcd.drawFastHLine(120 - i, 108, i * 2, T().primary);
    delay(2);
  }
  delay(60);
  // Hint
  drawCentered(118, "Press any button...", T().secondary);
}

// ==================== MENU SCREEN ====================
void showMenu() {
  M5.Lcd.fillScreen(BLACK);
  drawStatusBar();
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(T().primary);
  M5.Lcd.setCursor(6, 22);
  M5.Lcd.print("MAIN MENU");
  M5.Lcd.drawFastHLine(0, 33, 240, T().muted);
  const char* items[] = {"IR Remote", "Clock", "Settings"};
  for (int i = 0; i < 3; i++) {
    int y = 38 + i * 26;
    bool sel = (i == menuSel);
    if (sel) {
      M5.Lcd.fillRect(0, y, 240, 24, T().bar);
      M5.Lcd.fillRect(0, y, 3, 24, T().primary);
    }
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(12, y + 4);
    M5.Lcd.setTextColor(sel ? WHITE : T().secondary);
    M5.Lcd.print(items[i]);
    M5.Lcd.setTextSize(1);
    if (i == 0) {
      int occ = 0;
      for (int j = 0; j < NUM_SLOTS; j++) if (slots[j].occupied) occ++;
      char info[8];
      snprintf(info, sizeof(info), "%d/%d", occ, NUM_SLOTS);
      M5.Lcd.setCursor(204, y + 8);
      M5.Lcd.setTextColor(sel ? T().value : T().muted);
      M5.Lcd.print(info);
    } else if (i == 1 && hasTime) {
      struct tm ti; getLocalTime(&ti, 0);
      int totalMin = ti.tm_hour * 60 + ti.tm_min + clockHourOffset * 60 + clockMinuteOffset;
      while (totalMin < 0) totalMin += 1440;
      totalMin %= 1440;
      int adjH = totalMin / 60, adjM = totalMin % 60;
      char tb[6];
      snprintf(tb, sizeof(tb), "%02d:%02d", adjH, adjM);
      M5.Lcd.setCursor(204, y + 8);
      M5.Lcd.setTextColor(sel ? T().value : T().muted);
      M5.Lcd.print(tb);
    } else if (i == 2) {
      M5.Lcd.setCursor(228, y + 8);
      M5.Lcd.setTextColor(sel ? T().value : T().muted);
      M5.Lcd.print(">");
    }
  }
  drawButtonBar("B:Up A:Select P:Down");
}

void showSlots() {
  M5.Lcd.fillScreen(BLACK);
  drawStatusBar();
  M5.Lcd.setTextSize(1);
  // ── Horizontal slot bar (paged, 10 per page) ──
  int page = currentSlot / 10;
  int pageStart = page * 10;
  int pageEnd = min(pageStart + 10, NUM_SLOTS);
  int pageCount = pageEnd - pageStart;
  const int slotW = 19, slotGap = 2;
  int totalW = pageCount * slotW + (pageCount - 1) * slotGap;
  int sX = (240 - totalW) / 2;
  for (int i = pageStart; i < pageEnd; i++) {
    int x = sX + (i - pageStart) * (slotW + slotGap);
    if (i == currentSlot) {
      M5.Lcd.fillRoundRect(x, 20, slotW, 10, 2, T().primary);
      M5.Lcd.setTextColor(BLACK);
      M5.Lcd.setCursor(x + (slotW - 6) / 2, 22);
      M5.Lcd.print(i);
    } else if (slots[i].occupied) {
      M5.Lcd.fillRoundRect(x, 20, slotW, 10, 2, T().value);
    } else {
      M5.Lcd.drawRoundRect(x, 20, slotW, 10, 2, T().muted);
    }
  }
  // Page indicator
  if (NUM_SLOTS > 10) {
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.setCursor(224, 22);
    M5.Lcd.print(page + 1);
    M5.Lcd.print("/");
    M5.Lcd.print((NUM_SLOTS + 9) / 10);
  }
  M5.Lcd.drawFastHLine(0, 34, 240, T().muted);
  // ── Content area ──
  if (recording) {
    M5.Lcd.drawRect(3, 37, 234, 78, 0xF800);
    M5.Lcd.drawRect(4, 38, 232, 76, 0xF800);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(0xF800);
    M5.Lcd.setCursor(60, 48);
    M5.Lcd.print("REC SLOT ");
    M5.Lcd.print(currentSlot);
    M5.Lcd.setTextSize(1);
    drawCentered(72, "Aim remote & press button", T().secondary);
    drawCentered(90, "Press A/B to cancel", T().muted);
  } else if (slots[currentSlot].occupied) {
    M5.Lcd.fillRect(4, 37, 232, 78, T().bar);
    M5.Lcd.drawRect(4, 37, 232, 78, T().muted);
    // Slot label + protocol
    M5.Lcd.setTextColor(T().primary);
    M5.Lcd.setCursor(10, 41);
    M5.Lcd.print("SLOT ");
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print(currentSlot);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(T().value);
    M5.Lcd.setCursor(62, 41);
    M5.Lcd.print(typeToString(slots[currentSlot].proto));
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.setCursor(160, 41);
    M5.Lcd.print(slots[currentSlot].bits);
    M5.Lcd.print(" bit");
    M5.Lcd.drawFastHLine(10, 55, 220, T().muted);
    // Code (large)
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 62);
    M5.Lcd.print("0x");
    char hx[17]; snprintf(hx, sizeof(hx), "%08lX", (unsigned long)slots[currentSlot].code);
    M5.Lcd.print(hx);
    M5.Lcd.setTextSize(1);
    // Repeat info
    M5.Lcd.setCursor(10, 86);
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.print("TX ");
    M5.Lcd.setTextColor(T().value);
    M5.Lcd.print(repeatCount);
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.print("x burst");
    M5.Lcd.setCursor(150, 86);
    M5.Lcd.setTextColor(T().secondary);
    M5.Lcd.print("Ready");
  } else {
    M5.Lcd.drawRect(4, 37, 232, 78, T().muted);
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.setCursor(10, 42);
    M5.Lcd.print("SLOT ");
    M5.Lcd.setTextSize(2);
    M5.Lcd.print(currentSlot);
    M5.Lcd.setTextSize(1);
    drawCentered(62, "- EMPTY -", T().muted, 2);
    drawCentered(90, "Press B to record", T().secondary);
  }
  // ── Button bar ──
  if (recording) {
    drawButtonBar("A/B: Cancel");
  } else {
    drawButtonBar(slots[currentSlot].occupied ? "B:Prev A:Send P:Next" : "B:Prev A:Rec P:Next", "hB:Menu");
  }
}

// ==================== FULL-SCREEN CLOCK ====================
void updateClockDisplay(bool full) {
  if (!hasTime) {
    if (full) {
      drawCentered(40, "NO TIME SET", T().muted, 2);
      drawButtonBar("B:Back");
    }
    return;
  }
  struct tm ti;
  getLocalTime(&ti, 0);
  int totalMin = ti.tm_hour * 60 + ti.tm_min + clockHourOffset * 60 + clockMinuteOffset;
  while (totalMin < 0) totalMin += 1440;
  totalMin %= 1440;
  int adjH = totalMin / 60, adjM = totalMin % 60;
  // Time area — clear and redraw
  M5.Lcd.fillRect(0, 8, 240, 44, BLACK);
  char timeBuf[6];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", adjH, adjM);
  drawCentered(12, timeBuf, T().primary, 4);
  // Seconds
  char secBuf[4];
  snprintf(secBuf, sizeof(secBuf), ":%02d", ti.tm_sec);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(T().muted);
  M5.Lcd.setCursor(182, 30);
  M5.Lcd.print(secBuf);
  if (full) {
    M5.Lcd.drawFastHLine(20, 56, 200, T().muted);
    const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    char dateBuf[24];
    snprintf(dateBuf, sizeof(dateBuf), "%s  %02d/%02d/%04d", days[ti.tm_wday], ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900);
    drawCentered(64, dateBuf, T().secondary);
    int pct = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    char batBuf[16];
    snprintf(batBuf, sizeof(batBuf), charging ? "CHG %d%%" : "BAT %d%%", pct);
    drawCentered(82, batBuf, pct > 20 ? T().value : 0xF800);
    char offBuf[32];
    snprintf(offBuf, sizeof(offBuf), "Offset: %+dh %+dm", clockHourOffset, clockMinuteOffset);
    drawCentered(92, offBuf, T().muted);
    drawCentered(104, clockEditMode == 0 ? "[HOUR]" : "[MIN]", T().value);
    drawButtonBar("B:-  A:H/M  P:+", "hA:Back");
  }
}

void showClock() {
  M5.Lcd.fillScreen(BLACK);
  lastClockDraw = millis();
  updateClockDisplay(true);
}

void showSettings() {
  M5.Lcd.fillScreen(BLACK);
  drawStatusBar();
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(T().primary);
  M5.Lcd.setCursor(6, 22);
  M5.Lcd.print("SETTINGS");
  M5.Lcd.drawFastHLine(0, 33, 240, T().muted);
  const int NUM_SETTINGS = 5;
  const char* labels[] = {"Repeats", "Auto-Off", "Theme", "Clock Offset", "TV Presets"};
  char val[20];
  for (int i = 0; i < NUM_SETTINGS; i++) {
    int y = 37 + i * 17;
    bool sel = (i == settingSel);
    if (sel) {
      M5.Lcd.fillRect(0, y, 240, 15, T().bar);
      M5.Lcd.fillRect(0, y, 3, 15, T().primary);
    }
    M5.Lcd.setCursor(8, y + 4);
    M5.Lcd.setTextColor(sel ? WHITE : T().muted);
    M5.Lcd.print(labels[i]);
    if (i == 0) snprintf(val, sizeof(val), "%dx", repeatCount);
    else if (i == 1) snprintf(val, sizeof(val), "%d min", autoOffMinutes);
    else if (i == 2) snprintf(val, sizeof(val), "%s", THEMES[themeIdx].name);
    else if (i == 3) snprintf(val, sizeof(val), "%+dh %+dm", clockHourOffset, clockMinuteOffset);
    else snprintf(val, sizeof(val), ">");
    int vx = 228 - (int)strlen(val) * 6;
    M5.Lcd.setCursor(vx, y + 4);
    M5.Lcd.setTextColor(sel ? T().value : T().secondary);
    M5.Lcd.print(val);
    if (sel && i < 4) {
      M5.Lcd.setTextColor(T().muted);
      M5.Lcd.setCursor(vx - 12, y + 4);
      M5.Lcd.print("<");
      M5.Lcd.setCursor(vx + (int)strlen(val) * 6 + 4, y + 4);
      M5.Lcd.print(">");
    }
  }
  drawButtonBar("B:- A:+ P:Row", "hA:Back");
}

// ==================== TV PRESETS SCREEN ====================
void showPresets() {
  M5.Lcd.fillScreen(BLACK);
  drawStatusBar();
  M5.Lcd.setTextSize(1);
  // Header
  M5.Lcd.setTextColor(T().primary);
  M5.Lcd.setCursor(6, 22);
  M5.Lcd.print("TV PRESETS");
  // Start slot indicator
  char slotLbl[12];
  snprintf(slotLbl, sizeof(slotLbl), "Slot %d-%d", presetStartSlot, presetStartSlot + 4);
  M5.Lcd.setTextColor(T().value);
  M5.Lcd.setCursor(240 - (int)strlen(slotLbl) * 6 - 2, 22);
  M5.Lcd.print(slotLbl);
  M5.Lcd.drawFastHLine(0, 33, 240, T().muted);
  // Scroll window
  if (presetBrandIdx < presetScroll) presetScroll = presetBrandIdx;
  if (presetBrandIdx >= presetScroll + 4) presetScroll = presetBrandIdx - 3;
  // Scroll up indicator
  if (presetScroll > 0) {
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.setCursor(232, 35);
    M5.Lcd.print("^");
  }
  // Brand list (4 visible at a time)
  for (int v = 0; v < 4 && presetScroll + v < NUM_BRANDS; v++) {
    int i = presetScroll + v;
    int y = 37 + v * 19;
    bool sel = (i == presetBrandIdx);
    if (sel) {
      M5.Lcd.fillRect(0, y, 228, 17, T().bar);
      M5.Lcd.fillRect(0, y, 3, 17, T().primary);
    }
    M5.Lcd.setCursor(8, y + 5);
    M5.Lcd.setTextColor(sel ? WHITE : T().muted);
    M5.Lcd.print(TV_BRANDS[i].name);
    if (sel) {
      M5.Lcd.setTextColor(T().secondary);
      M5.Lcd.setCursor(168, y + 5);
      for (int j = 0; j < 5; j++) {
        M5.Lcd.print(TV_BRANDS[i].codes[j].name[0]);
        if (j < 4) M5.Lcd.print(' ');
      }
    }
  }
  // Scroll down indicator
  if (presetScroll + 4 < NUM_BRANDS) {
    M5.Lcd.setTextColor(T().muted);
    M5.Lcd.setCursor(232, 108);
    M5.Lcd.print("v");
  }
  // Info line
  char info[24];
  snprintf(info, sizeof(info), "%d/%d brands", presetBrandIdx + 1, NUM_BRANDS);
  M5.Lcd.setTextColor(T().muted);
  M5.Lcd.setCursor(6, 114);
  M5.Lcd.print(info);
  drawButtonBar("B/P:Brand A:Pos", "hA:Load");
}

// ==================== IR ACTIONS ====================
void sendSignal(int s) {
  if (!slots[s].occupied) return;
  // Transmit screen with live progress
  M5.Lcd.fillScreen(BLACK);
  drawCentered(4, "TRANSMITTING", T().primary);
  M5.Lcd.drawFastHLine(20, 16, 200, T().muted);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(T().secondary);
  M5.Lcd.setCursor(20, 22);
  M5.Lcd.print("Slot ");
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.print(s);
  M5.Lcd.setTextColor(T().muted);
  M5.Lcd.print("  ");
  M5.Lcd.setTextColor(T().value);
  M5.Lcd.print(typeToString(slots[s].proto));
  char hx[14]; snprintf(hx, sizeof(hx), "0x%08lX", (unsigned long)slots[s].code);
  drawCentered(38, hx, WHITE);
  drawProgressBar(20, 56, 200, 16, 0.0, T().primary, T().bar);
  char cnt[16];
  snprintf(cnt, sizeof(cnt), "0 / %d", repeatCount);
  drawCentered(80, cnt, T().muted);
  Serial.print("[TX] ");
  Serial.print(typeToString(slots[s].proto));
  Serial.print(" 0x");
  Serial.print((unsigned long)slots[s].code, HEX);
  Serial.print(" x");
  Serial.println(repeatCount);
  for (int r = 0; r < repeatCount; r++) {
    if (slots[s].proto == decode_type_t::UNKNOWN)
      irsend.sendNEC(slots[s].code, slots[s].bits > 0 ? slots[s].bits : 32);
    else
      irsend.send(slots[s].proto, slots[s].code, slots[s].bits);
    if (r % 3 == 0 || r == repeatCount - 1) {
      drawProgressBar(20, 56, 200, 16, (float)(r + 1) / (float)repeatCount, T().primary, T().bar);
      snprintf(cnt, sizeof(cnt), "%d / %d", r + 1, repeatCount);
      M5.Lcd.fillRect(0, 78, 240, 14, BLACK);
      drawCentered(80, cnt, T().value);
    }
    delay(40);
  }
  M5.Lcd.fillRect(0, 78, 240, 14, BLACK);
  drawCentered(80, "DONE!", T().primary);
  M5.Lcd.drawFastHLine(20, 98, 200, T().muted);
  drawCentered(106, "Signal sent successfully", T().secondary);
  delay(500);
}

void onSignalReceived() {
  slots[currentSlot].code     = results.value;
  slots[currentSlot].bits     = results.bits;
  slots[currentSlot].proto    = results.decode_type;
  slots[currentSlot].occupied = true;
  saveSlot(currentSlot);
  recording = false;
  Serial.print("[RX] ");
  Serial.print(typeToString(results.decode_type));
  Serial.print(" 0x");
  Serial.print((unsigned long)results.value, HEX);
  Serial.print(" ");
  Serial.print(results.bits);
  Serial.println("b");
  // Confirmation splash
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.drawRect(10, 5, 220, 92, T().primary);
  drawCentered(12, "SAVED!", T().value, 3);
  M5.Lcd.drawFastHLine(20, 40, 200, T().muted);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(T().secondary);
  M5.Lcd.setCursor(20, 48);
  M5.Lcd.print("Slot ");
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.print(currentSlot);
  M5.Lcd.setTextColor(T().muted);
  M5.Lcd.print("  ");
  M5.Lcd.setTextColor(T().value);
  M5.Lcd.print(typeToString(results.decode_type));
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(20, 64);
  M5.Lcd.print("0x");
  char hx[17]; snprintf(hx, sizeof(hx), "%08lX", (unsigned long)results.value);
  M5.Lcd.print(hx);
  M5.Lcd.setTextColor(T().muted);
  M5.Lcd.print("  ");
  M5.Lcd.print(results.bits);
  M5.Lcd.print("b");
  drawCentered(82, "Ready to transmit", T().secondary);
  drawCentered(108, "Press B on slot to send", T().muted);
  delay(1500);
  irrecv.resume();
  showSlots();
}

// ==================== WIFI / NTP ====================
void setTimeFromCompile() {
  // Parse __DATE__ ("Mar 22 2026") and __TIME__ ("14:30:00")
  const char* date = __DATE__;  // e.g. "Mar 22 2026"
  const char* time = __TIME__;  // e.g. "14:30:00"
  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {date[0], date[1], date[2], 0};
  int month = 0;
  for (int i = 0; i < 12; i++) {
    if (mon[0] == months[i*3] && mon[1] == months[i*3+1] && mon[2] == months[i*3+2]) {
      month = i; break;
    }
  }
  struct tm ti = {};
  ti.tm_year = atoi(date + 7) - 1900;
  ti.tm_mon  = month;
  ti.tm_mday = atoi(date + 4);
  ti.tm_hour = atoi(time);
  ti.tm_min  = atoi(time + 3);
  ti.tm_sec  = atoi(time + 6);
  time_t epoch = mktime(&ti);
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", nullptr, nullptr);
  hasTime = true;
  Serial.print("[CLK] Compile time: ");
  Serial.println(asctime(&ti));
}

void syncTime() {
  if (strlen(WIFI_SSID) == 0) {
    Serial.println("[NTP] No WiFi configured, using compile time");
    setTimeFromCompile();
    return;
  }
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(T().secondary);
  M5.Lcd.setCursor(45, 58);
  M5.Lcd.print("Syncing time...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    delay(250); M5.Lcd.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    struct tm ti;
    hasTime = getLocalTime(&ti, 5000);
    if (hasTime) { Serial.print("[NTP] "); Serial.println(&ti, "%H:%M:%S"); }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    Serial.println("[NTP] WiFi failed, using compile time");
    setTimeFromCompile();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

// ==================== SETUP ====================
void setup() {
  M5.begin();
  Serial.begin(115200);
  delay(300);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextWrap(false);
  M5.Lcd.setBrightness(60);
  M5.Lcd.fillScreen(BLACK);
  Serial.println("\n=== ROGAL IR Signal Manager ===");
  for (int i = 0; i < NUM_SLOTS; i++) slots[i] = {};
  loadSettings();
  loadSlots();
  syncTime();
  irrecv.enableIRIn();
  pinMode(IR_RX, INPUT_PULLDOWN);  // pull-down for receiver detection
  irrecv.setUnknownThreshold(12);
  irsend.begin();
  Serial.println("[OK] IR ready (RX:G33 TX:G19)");
  lastActivity = millis();
  screen = SCR_WELCOME;
  showWelcome();
}

// ==================== INPUT HANDLERS ====================
void handleWelcome() {
  if (millis() - welcomeStart > 5000 || M5.BtnA.isPressed() || M5.BtnB.isPressed()) {
    lastActivity = millis();
    screen = SCR_MENU;
    menuSel = 0;
    showMenu();
  }
}

void handleMenu(BtnEvt eA, BtnEvt eB, BtnEvt eP) {
  if (eA == BTN_SHORT) {
    lastActivity = millis();
    menuSel = (menuSel - 1 + 3) % 3;
    showMenu();
    return;
  }
  if (eP == BTN_SHORT) {
    lastActivity = millis();
    menuSel = (menuSel + 1) % 3;
    showMenu();
    return;
  }
  if (eB == BTN_SHORT) {
    lastActivity = millis();
    if (menuSel == 0)      { screen = SCR_SLOTS; showSlots(); }
    else if (menuSel == 1) { screen = SCR_CLOCK; showClock(); }
    else                   { screen = SCR_SETTINGS; settingSel = 0; showSettings(); }
    return;
  }
}

void handleClock(BtnEvt eA, BtnEvt eB, BtnEvt eP) {
  if (eB == BTN_LONG || (eB == BTN_SHORT && !hasTime)) {
    lastActivity = millis();
    clockEditMode = 0;
    screen = SCR_MENU;
    showMenu();
    return;
  }
  if (eB == BTN_SHORT) {
    lastActivity = millis();
    clockEditMode = 1 - clockEditMode;
    showClock();
    return;
  }
  if (eA == BTN_SHORT) {
    lastActivity = millis();
    if (clockEditMode == 0) clockHourOffset = max(-12, clockHourOffset - 1);
    else clockMinuteOffset = max(-59, clockMinuteOffset - 1);
    saveSettings();
    showClock();
    return;
  }
  if (eP == BTN_SHORT) {
    lastActivity = millis();
    if (clockEditMode == 0) clockHourOffset = min(12, clockHourOffset + 1);
    else clockMinuteOffset = min(59, clockMinuteOffset + 1);
    saveSettings();
    showClock();
    return;
  }
}

void handleSlots(BtnEvt eA, BtnEvt eB, BtnEvt eP) {
  // --- Power button: next slot ---
  if (eP == BTN_SHORT) {
    lastActivity = millis();
    if (recording) { recording = false; }
    else { currentSlot = (currentSlot + 1) % NUM_SLOTS; }
    showSlots();
    return;
  }
  // --- Button B ---
  if (eB == BTN_LONG) {
    // Long B: next slot (or enter record on stored slot if not already recording)
    if (recording) { recording = false; }
    else { currentSlot = (currentSlot + 1) % NUM_SLOTS; }
    lastActivity = millis();
    showSlots();
    return;
  }
  if (eB == BTN_SHORT) {
    lastActivity = millis();
    if (recording) {
      recording = false;
    } else if (slots[currentSlot].occupied) {
      sendSignal(currentSlot);
    } else {
      recording = true;
      irrecv.resume();
    }
    showSlots();
    return;
  }
  // --- Button A ---
  if (eA == BTN_LONG) {
    recording = false;
    lastActivity = millis();
    screen = SCR_MENU;
    menuSel = 0;
    showMenu();
    return;
  }
  if (eA == BTN_SHORT) {
    lastActivity = millis();
    if (recording) { recording = false; }
    else { currentSlot = (currentSlot - 1 + NUM_SLOTS) % NUM_SLOTS; }
    showSlots();
    return;
  }
}

void handleSettings(BtnEvt eA, BtnEvt eB, BtnEvt eP) {
  const int NUM_SETTINGS = 5;
  // --- Power button: next row ---
  if (eP == BTN_SHORT) {
    lastActivity = millis();
    settingSel = (settingSel + 1) % NUM_SETTINGS;
    showSettings();
    return;
  }
  if (eB == BTN_LONG) {
    lastActivity = millis();
    saveSettings();
    screen = SCR_MENU;
    showMenu();
    return;
  }
  if (eB == BTN_SHORT) {
    lastActivity = millis();
    if (settingSel == 0)      repeatCount = min(50, repeatCount + 1);
    else if (settingSel == 1) autoOffMinutes = min(30, autoOffMinutes + 1);
    else if (settingSel == 2) themeIdx = (themeIdx + 1) % NUM_THEMES;
    else if (settingSel == 3) clockHourOffset = min(12, clockHourOffset + 1);
    else { screen = SCR_PRESETS; presetBrandIdx = 0; presetScroll = 0; showPresets(); return; }
    showSettings();
    return;
  }
  if (eA == BTN_LONG) {
    lastActivity = millis();
    settingSel = (settingSel + 1) % NUM_SETTINGS;
    showSettings();
    return;
  }
  if (eA == BTN_SHORT) {
    lastActivity = millis();
    if (settingSel == 0)      repeatCount = max(1, repeatCount - 1);
    else if (settingSel == 1) autoOffMinutes = max(0, autoOffMinutes - 1);
    else if (settingSel == 2) themeIdx = (themeIdx - 1 + NUM_THEMES) % NUM_THEMES;
    else if (settingSel == 3) clockHourOffset = max(-12, clockHourOffset - 1);
    showSettings();
    return;
  }
}

void handlePresets(BtnEvt eA, BtnEvt eB, BtnEvt eP) {
  // --- Power button: next brand ---
  if (eP == BTN_SHORT) {
    lastActivity = millis();
    presetBrandIdx = (presetBrandIdx + 1) % NUM_BRANDS;
    showPresets();
    return;
  }
  // --- A long: back to settings ---
  if (eA == BTN_LONG) {
    lastActivity = millis();
    screen = SCR_SETTINGS;
    showSettings();
    return;
  }
  // --- B long: LOAD presets ---
  if (eB == BTN_LONG) {
    lastActivity = millis();
    const TvBrand& brand = TV_BRANDS[presetBrandIdx];
    for (int i = 0; i < 5; i++) {
      int si = presetStartSlot + i;
      slots[si].code     = brand.codes[i].code;
      slots[si].bits     = brand.codes[i].bits;
      slots[si].proto    = brand.codes[i].proto;
      slots[si].occupied = true;
      saveSlot(si);
    }
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.drawRect(10, 3, 220, 114, T().primary);
    drawCentered(10, "LOADED!", T().value, 2);
    M5.Lcd.setTextSize(1);
    int nx = (240 - (int)strlen(brand.name) * 6) / 2;
    M5.Lcd.setTextColor(T().primary);
    M5.Lcd.setCursor(nx, 30);
    M5.Lcd.print(brand.name);
    M5.Lcd.drawFastHLine(20, 42, 200, T().muted);
    for (int i = 0; i < 5; i++) {
      int ly = 48 + i * 14;
      M5.Lcd.setTextColor(T().muted);
      M5.Lcd.setCursor(20, ly);
      M5.Lcd.print("Slot ");
      M5.Lcd.setTextColor(T().value);
      M5.Lcd.print(presetStartSlot + i);
      M5.Lcd.setTextColor(T().muted);
      M5.Lcd.print(": ");
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.print(brand.codes[i].name);
    }
    char slotInfo[24];
    snprintf(slotInfo, sizeof(slotInfo), "Slots %d-%d ready", presetStartSlot, presetStartSlot + 4);
    drawCentered(122, slotInfo, T().secondary);
    delay(1800);
    currentSlot = presetStartSlot;
    screen = SCR_SLOTS;
    showSlots();
    return;
  }
  // --- B short: cycle start slot ---
  if (eB == BTN_SHORT) {
    lastActivity = millis();
    presetStartSlot = (presetStartSlot + 1) % 6;
    showPresets();
    return;
  }
  // --- A short: prev brand ---
  if (eA == BTN_SHORT) {
    lastActivity = millis();
    presetBrandIdx = (presetBrandIdx - 1 + NUM_BRANDS) % NUM_BRANDS;
    showPresets();
    return;
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  M5.update();

  // Auto power-off
  if (autoOffMinutes > 0 && (millis() - lastActivity) / 60000 >= (unsigned long)autoOffMinutes) {
    M5.Lcd.fillScreen(BLACK);
    drawCentered(35, "SLEEP", T().primary, 3);
    drawCentered(68, "Powering off...", T().muted);
    delay(800);
    M5.Power.powerOff();
  }

  // Refresh clock every 30s on screens with status bar
  if ((screen == SCR_MENU || screen == SCR_SLOTS || screen == SCR_SETTINGS || screen == SCR_PRESETS) && millis() - lastClockUpdate >= 30000) {
    lastClockUpdate = millis();
    drawStatusBar();
  }

  // Live-update full-screen clock every second
  if (screen == SCR_CLOCK && millis() - lastClockDraw >= 1000) {
    lastClockDraw = millis();
    updateClockDisplay(false);
  }

  // Read buttons (clean: short fires on release, long fires once on threshold)
  BtnEvt eA = checkBtn(tA, M5.BtnA.isPressed(), (screen == SCR_SETTINGS || screen == SCR_PRESETS) ? 1500 : 1000);
  BtnEvt eB = checkBtn(tB, M5.BtnB.isPressed(), 3000);
  BtnEvt eP = checkBtn(tP, M5.BtnPWR.isPressed(), (screen == SCR_SETTINGS || screen == SCR_PRESETS) ? 1000 : 2000);

  switch (screen) {
    case SCR_WELCOME:  handleWelcome(); break;
    case SCR_MENU:     handleMenu(eB, eA, eP); break;
    case SCR_SLOTS:    handleSlots(eB, eA, eP); break;
    case SCR_CLOCK:    handleClock(eB, eA, eP); break;
    case SCR_SETTINGS: handleSettings(eB, eA, eP); break;
    case SCR_PRESETS:  handlePresets(eB, eA, eP); break;
  }

  // *** IR CAPTURE — only resume() after a successful decode ***
  if (recording && screen == SCR_SLOTS) {
    if (irrecv.decode(&results)) {
      lastActivity = millis();
      onSignalReceived();
      // irrecv.resume() is called inside onSignalReceived()
    }
    // Do NOT call irrecv.resume() here — it resets partial captures!
  }

  delay(30);
}
