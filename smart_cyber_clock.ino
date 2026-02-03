#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include "time.h"

#include "secrets.h"

// --- NEW LIBRARIES FOR MULTI-WIFI ---
#include <WiFiMulti.h>
#include <Preferences.h>

WiFiMulti wifiMulti;
Preferences prefs;
// ------------------------------------

#include <HTTPClient.h>
#include <ArduinoJson.h>

// ====== Weather Settings ======
String weatherKey = WEATHER_API_KEY;
String city       = WEATHER_API_CITY;       // YOUR CITY (e.g. "London,UK")

// Variables to store data
float outTemp = 0.0;
int   outHum  = 0;
String outDesc = "--";
unsigned long lastWeatherFetch = 0;
bool weatherLoaded = false;


// ====== WiFi & Time config ======

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec      = 1 * 3600;   // GMT+1
const int   daylightOffset_sec = 0;



// ====== Forecast Data ======
float fTemps[6];    // Temperatures for next 18h
bool  fRain[6];     // Rain flag for each interval
int   fHours[6];    // Hour of the day (0-23) for X-axis labels
bool  forecastLoaded = false;

// ====== Pins ======
#define TFT_CS   9
#define TFT_DC   8
#define TFT_RST  7
#define TFT_MOSI 6
#define TFT_SCLK 5

#define ENC_A_PIN    10
#define ENC_B_PIN    20
#define ENC_BTN_PIN  21
#define KEY0_PIN     0

#define SDA_PIN 1
#define SCL_PIN 2

#define LED_PIN 4
#define BUZZ_PIN 3

// ====== TFT & Sensors ======
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); // hardware SPI
Adafruit_AHTX0   aht;
ScioSense_ENS160 ens160(0x53);   // ENS160 I2C address 0x53

// ====== Colors ======
#define CYBER_BG      ST77XX_BLACK
#define CYBER_GREEN   0x07E0  
#define CYBER_ACCENT  0x07FF  
#define CYBER_LIGHT   0xFD20  
#define CYBER_BLUE    0x07FF  
#define CYBER_PINK    0xF81F

#define AQ_BAR_GREEN  0x07E0
#define AQ_BAR_YELLOW 0xFFE0
#define AQ_BAR_ORANGE 0xFD20
#define AQ_BAR_RED    0xF800
#define CYBER_DARK    0x4208
#define RAIN 0x8cfe


// --- NEW BACKGROUND COLORS (Dimmed for readability) ---
#define BG_CLEAR      0x0000  // Keep Black for Clear (or use 0x0010 for very dark blue)
#define BG_CLOUDS     0x2124  // Dark Grey
#define BG_RAIN       0x0010  // Deep Navy Blue
#define BG_SNOW       0x632C  // Cold Grey/Blue
#define BG_THUNDER    0x2004  // Dark Purple/Grey


#ifndef PI
#define PI 3.1415926
#endif

// ====== Modes ======
enum UIMode {
  MODE_MENU = 0,
  MODE_CLOCK,
  MODE_POMODORO,
  MODE_ALARM,
  MODE_DVD,
  MODE_DAY_COUNTER
};
UIMode currentMode = MODE_CLOCK;       // khởi động vào CLOCK luôn
int menuIndex = 0;
const int MENU_ITEMS = 5;       // Monitor, Pomodoro, Alarm, DVD, Day Counter

// ====== Pomodoro ======
enum PomodoroState {
  POMO_SELECT = 0,
  POMO_RUNNING,
  POMO_PAUSED,
  POMO_DONE
};
PomodoroState pomoState = POMO_SELECT;
int  pomoPresetIndex    = 0;                   // 0:5, 1:15, 2:30
const uint16_t pomoDurationsMin[3] = {5, 15, 30};
unsigned long pomoStartMillis = 0;
unsigned long pomoPausedMillis = 0;
int prevPomoRemainSec  = -1;
int prevPomoPreset     = -1;
int prevPomoStateInt   = -1;

// ====== Env values ======
float    curTemp = 0;
float    curHum  = 0;
uint16_t curTVOC = 0;
uint16_t curECO2 = 400;
unsigned long lastEnvRead = 0;

// ====== Clock vars ======
int    prevSecond  = -1;
String prevTimeStr = "";

// ====== Encoder & Buttons ======
int  lastEncA   = HIGH;
int  lastEncB   = HIGH;
bool lastEncBtn = HIGH;
bool lastKey0   = HIGH;
unsigned long lastBtnMs = 0;

// ====== Alarm ======
bool     alarmEnabled = false;
uint8_t  alarmHour    = 7;
uint8_t  alarmMinute  = 0;
bool     alarmRinging = false;
int      alarmSelectedField = 0; // 0 = hour, 1 = minute, 2 = enable
int      lastAlarmDayTriggered = -1;

// ====== Alert / LED Blink ======
enum AlertLevel {
  ALERT_NONE = 0,
  ALERT_CO2,
  ALERT_ALARM
};
AlertLevel currentAlertLevel = ALERT_NONE;
unsigned long lastLedToggleMs = 0;
bool ledState = false;

unsigned long lastCo2BlinkMs = 0;
bool co2BlinkOn = false;

// ========= Helper: encoder & button =========
int readEncoderStep() {
  int encA = digitalRead(ENC_A_PIN);
  int encB = digitalRead(ENC_B_PIN);
  int step = 0;
  if (encA != lastEncA) {
    if (encA == LOW) {
      if (encB == HIGH) step = +1;
      else              step = -1;
    }
  }
  lastEncA = encA;
  lastEncB = encB;
  return step;
}

bool checkButtonPressed(uint8_t pin, bool &lastState) {
  bool cur = digitalRead(pin);
  bool pressed = false;
  unsigned long now = millis();
  if (cur == LOW && lastState == HIGH && (now - lastBtnMs) > 150) {
    pressed = true;
    lastBtnMs = now;
  }
  lastState = cur;
  return pressed;
}

// ========= Alarm icon =========
/*
void drawAlarmIcon() {
  int x = 148;
  tft.fillRect(x - 10, 0, 12, 12, CYBER_BG);
  if (!alarmEnabled) return;

  uint16_t c = CYBER_LIGHT; // cam
  tft.drawRoundRect(x - 9, 2, 10, 7, 2, c);
  tft.drawFastHLine(x - 8, 8, 8, c);
  tft.fillCircle(x - 4, 10, 1, c);
}*/
// ========= Alarm icon =========
// Now accepts a background color (defaults to Black if not specified)
void drawAlarmIcon(uint16_t bgColor = CYBER_BG) {
  int x = 148;
  
  // Use 'bgColor' instead of CYBER_BG to clear the area
  tft.fillRect(x - 10, 0, 12, 12, bgColor); 

  if (!alarmEnabled) return;

  uint16_t c = CYBER_LIGHT; // orange
  tft.drawRoundRect(x - 9, 2, 10, 7, 2, c);
  tft.drawFastHLine(x - 8, 8, 8, c);
  tft.fillCircle(x - 4, 10, 1, c);
}




// This function runs ONLY if WiFi connection fails and AP mode starts
void configModeCallback(WiFiManager *myWiFiManager) {
  tft.fillScreen(CYBER_BG);
  tft.setTextColor(CYBER_LIGHT);
  tft.setTextSize(1);
  
  tft.setCursor(10, 40);
  tft.println("AutoConfig Mode:");
  
  // Line 1
  tft.setCursor(10, 55);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("1. Connect Phone to");
  
  // Line 2
  tft.setCursor(10, tft.getCursorY()); 
  tft.setTextColor(CYBER_ACCENT);
  tft.println("   WIFI: CyberClock");
  
  // Line 3
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, tft.getCursorY()); 
  tft.println("2. Wait for Pop-up");
  
  // Line 4
  tft.setCursor(10, tft.getCursorY());
  tft.println("   to input Pass");
}


// ========== HELPERS ==============


// Load up to 3 saved networks from memory into WiFiMulti
void loadWiFiList() {
  prefs.begin("wifi-creds", true); // Open in read-only mode
  
  // Try to load 3 slots
  for (int i = 0; i < 3; i++) {
    String keySsid = "ssid" + String(i);
    String keyPass = "pass" + String(i);
    
    String s = prefs.getString(keySsid.c_str(), "");
    String p = prefs.getString(keyPass.c_str(), "");
    
    if (s.length() > 0) {
      Serial.print("Loaded Saved WiFi: "); Serial.println(s);
      // conversion to char array needed for addAP
      char sArr[33]; s.toCharArray(sArr, 33);
      char pArr[65]; p.toCharArray(pArr, 65);
      wifiMulti.addAP(sArr, pArr);
    }
  }
  prefs.end();
}

// Save the CURRENT connected network to the list (Shift old ones out)
void saveCurrentNetwork() {
  String currentSSID = WiFi.SSID();
  String currentPass = WiFi.psk();
  
  if (currentSSID.length() == 0) return;

  prefs.begin("wifi-creds", false); // Open in read/write mode

  // 1. Check if it already exists (don't save duplicate)
  for (int i = 0; i < 3; i++) {
    String s = prefs.getString(("ssid" + String(i)).c_str(), "");
    if (s == currentSSID) {
      prefs.end();
      return; // Already saved
    }
  }

  // 2. Shift existing down (Slot 1->2, Slot 0->1) to make room at Slot 0
  String s1 = prefs.getString("ssid1", "");
  String p1 = prefs.getString("pass1", "");
  String s0 = prefs.getString("ssid0", "");
  String p0 = prefs.getString("pass0", "");

  // Move 1 to 2
  if (s1.length() > 0) {
    prefs.putString("ssid2", s1);
    prefs.putString("pass2", p1);
  }
  // Move 0 to 1
  if (s0.length() > 0) {
    prefs.putString("ssid1", s0);
    prefs.putString("pass1", p0);
  }

  // 3. Save NEW network to Slot 0 (The highest priority)
  prefs.putString("ssid0", currentSSID);
  prefs.putString("pass0", currentPass);
  
  Serial.println("New Network Saved to Slot 0");
  prefs.end();
}







// ========= WiFi & Time =========
void connectWiFiAndSyncTime() {
  tft.fillScreen(CYBER_BG);
  tft.setTextColor(CYBER_LIGHT);
  tft.setTextSize(1);
  
  // --- STEP 1: Try connecting to stored networks (Multi) ---
  tft.setCursor(10, 55);
  tft.print("Checking Memory...");
  
  WiFi.mode(WIFI_STA);
  loadWiFiList(); // Load the 3 slots
  
  // Try to connect for ~5-6 seconds
  bool connected = false;
  // We loop a few times; wifiMulti.run() picks the best available network
  for (int i = 0; i < 6; i++) { 
    if (wifiMulti.run() == WL_CONNECTED) {
      connected = true;
      break;
    }
    tft.print(".");
    delay(1000);
  }

  // --- STEP 2: If Multi failed, start WiFiManager (Pop-up) ---
  if (!connected) {
    tft.fillScreen(CYBER_BG);
    tft.setCursor(10, 55);
    tft.setTextColor(CYBER_LIGHT);
    tft.print("No known WiFi found");
    delay(1000);

    WiFiManager wm;
    // Inject your Custom CSS here (copy your working CSS string from before)
   const char* customCSS = 
   "<style>"
     "body {"
      "background-color: #000000;"
      "color: #FFFFFF;"
      "font-family: 'Courier New', Courier, monospace;" // Retro font
    "}"
    // --- FIX 1: HIDE THE EXTRA WHITE TEXT ---
    "h3 { display: none !important; }"
    "img {"
      "background-color: #00FFFF;"  /* Cyan background */
      "padding: 2px;"                /* Little bit of spacing */
      "vertical-align: middle;"      /* Aligns icon with text */
      "border: 1px solid #00FFFF;"   /* Optional: Green border for detail */
    "}"

  /* --- NEW: FIX FOR INVISIBLE WIFI NAMES --- */
    "a {"
      "color: #00FFFF;"       /* CYAN color for Wi-Fi Network names */
      "text-decoration: none;"
    "}"
    "a:hover {"
      "color: #FF00FF;"       /* PINK when you touch/hover them */
    "}"
    /* ---------------------------------------- */

    "h1 {"
      "color: #00FFFF;"       // Cyan Header
      "text-shadow: 2px 2px #FF00FF;" // Pink shadow for 'glitch' effect
    "}"
    "button {"
      "background-color: #000000;"
      "color: #00FFFF;"
      "border: 2px solid #00FFFF;"
      "border-radius: 0px;"   // Sharp corners
      "padding: 10px;"
      "font-weight: bold;"
      "text-transform: uppercase;"
    "}"
    "button:hover {"
      "background-color: #00FFFF;"
      "color: #000000;"
    "}"
    "input {"
      "background-color: #1a1a1a;"
      "color: #FFFFFF;"
      "border: 1px solid #00FFFF;"
      "border-radius: 0px;"
      "padding: 5px;"
    "}"
    "div, p, form { text-align: left; }" // Center align everything
    
    // Add this to hide the "No AP set" status box at the bottom
    ".mw { display: none; }"
    
    // ... end of style ...
    "</style>";
  
    wm.setCustomHeadElement(customCSS);
    wm.setTitle("CYBER CLOCK");
    wm.setAPCallback(configModeCallback);
    
    // Start Pop-up
    bool res = wm.autoConnect(AP_SSID, AP_PASSWORD);

    if (!res) {
      tft.fillScreen(CYBER_BG);
      tft.setCursor(10, 55);
      tft.setTextColor(ST77XX_RED);
      tft.print("WiFi FAILED!");
      delay(3000);
      ESP.restart();
    } else {
      // SUCCESS via Pop-up: Save this new network to our list
      saveCurrentNetwork();
    }
  }

  // --- STEP 3: Connected! Sync Time ---
  tft.fillScreen(CYBER_BG);
  tft.setCursor(10, 55);
  tft.setTextColor(CYBER_GREEN);
  tft.print("WiFi Connected!");
  tft.setCursor(10, 70);
  tft.setTextColor(CYBER_LIGHT);
  tft.print("SSID: ");
  tft.print(WiFi.SSID()); // Show which one we connected to

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    delay(500);
    retry++;
  }
}


String getTimeStr(char type) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--";
  char buf[8];
  if (type == 'H') strftime(buf, sizeof(buf), "%H", &timeinfo);
  else if (type == 'M') strftime(buf, sizeof(buf), "%M", &timeinfo);
  else if (type == 'S') strftime(buf, sizeof(buf), "%S", &timeinfo);
  return String(buf);
}

// ========= Env Sensors =========
void updateEnvSensors(bool force = false) {
  unsigned long now = millis();
  if (!force && (now - lastEnvRead) < 5000) return;
  lastEnvRead = now;

  sensors_event_t hum, temp;
  if (aht.getEvent(&hum, &temp)) {
    curTemp = temp.temperature;
    curHum  = hum.relative_humidity;
  }

  ens160.set_envdata(curTemp, curHum);
  ens160.measure();                      // blocking

  uint16_t newTVOC = ens160.getTVOC();
  uint16_t newCO2  = ens160.geteCO2();

  if (newTVOC != 0xFFFF) curTVOC = newTVOC;
  if (newCO2  != 0xFFFF) curECO2 = newCO2;

  Serial.print("ENS160: TVOC=");
  Serial.print(curTVOC);
  Serial.print(" eCO2=");
  Serial.println(curECO2);
}

// ========= Clock UI =========

// Grid layout
const int GRID_L   = 4;
const int GRID_R   = 156;
const int GRID_TOP = 56;
const int GRID_MID = 80;
const int GRID_BOT = 104;
const int GRID_MID_X = (GRID_L + GRID_R) / 2;

// Y cho label / value (text size 1, cao ~8px)
const int TOP_LABEL_Y    = GRID_TOP + 4;   // 60
const int TOP_VALUE_Y    = GRID_TOP + 15;  // 71
const int BOTTOM_LABEL_Y = GRID_MID + 4;   // 84
const int BOTTOM_VALUE_Y = GRID_MID + 15;  // 95

// bar layout
const int BAR_MARGIN_X = 2;
const int BAR_GAP      = 2;
const int BAR_Y        = 110;
const int BAR_H        = 6;
const int BAR_W        = (160 - 2 * BAR_MARGIN_X - 3 * BAR_GAP) / 4; // 37

// In text căn giữa trong khoảng [x0, x1]
void printCenteredText(const String &txt,
                       int x0, int x1,
                       int y,
                       uint16_t color,
                       uint16_t bg,
                       uint8_t size = 1) {
  int16_t bx, by;
  uint16_t w, h;
  tft.setTextSize(size);
  tft.getTextBounds(txt, 0, 0, &bx, &by, &w, &h);
  int x = x0 + ((x1 - x0) - (int)w) / 2;
  tft.setTextColor(color, bg);
  tft.setCursor(x, y);
  tft.print(txt);
}

uint16_t colorForCO2(uint16_t eco2) {
  if (eco2 <= 800)  return AQ_BAR_GREEN;
  if (eco2 <= 1200) return AQ_BAR_YELLOW;
  if (eco2 <= 1800) return AQ_BAR_ORANGE;
  return AQ_BAR_RED;
}

void initClockStaticUI() {
  tft.fillScreen(CYBER_BG);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(4, 4);
  tft.print("Monitor");

  tft.setCursor(4, 44);
  tft.print("Air Quality:");

  tft.drawFastHLine(GRID_L, GRID_TOP, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastHLine(GRID_L, GRID_MID, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastHLine(GRID_L, GRID_BOT, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastVLine(GRID_MID_X, GRID_TOP, GRID_BOT - GRID_TOP, ST77XX_WHITE);

  // Label căn giữa trong từng ô
  printCenteredText("HUMI", GRID_L,     GRID_MID_X, TOP_LABEL_Y,    ST77XX_WHITE, CYBER_BG, 1);
  printCenteredText("TEMP", GRID_MID_X, GRID_R,     TOP_LABEL_Y,    ST77XX_WHITE, CYBER_BG, 1);
  printCenteredText("TVOC", GRID_L,     GRID_MID_X, BOTTOM_LABEL_Y, ST77XX_WHITE, CYBER_BG, 1);
  printCenteredText("CO2",  GRID_MID_X, GRID_R,     BOTTOM_LABEL_Y, ST77XX_WHITE, CYBER_BG, 1);

  int x = BAR_MARGIN_X;
  tft.fillRect(x,                          BAR_Y, BAR_W, BAR_H, AQ_BAR_GREEN);
  tft.fillRect(x + (BAR_W + BAR_GAP),      BAR_Y, BAR_W, BAR_H, AQ_BAR_YELLOW);
  tft.fillRect(x + 2 * (BAR_W + BAR_GAP),  BAR_Y, BAR_W, BAR_H, AQ_BAR_ORANGE);
  tft.fillRect(x + 3 * (BAR_W + BAR_GAP),  BAR_Y, BAR_W, BAR_H, AQ_BAR_RED);

  drawAlarmIcon();
}

void drawClockTime(String hourStr, String minStr, String secStr) {
  String cur = hourStr + ":" + minStr + ":" + secStr;
  if (cur == prevTimeStr) return;
  prevTimeStr = cur;

  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(3);
  tft.getTextBounds(cur, 0, 0, &x1, &y1, &w, &h);
  int x = (160 - w) / 2;
  int y = 18;

  tft.setTextColor(CYBER_LIGHT, CYBER_BG);
  tft.setCursor(x, y);
  tft.print(cur);

  tft.setTextColor(CYBER_ACCENT, CYBER_BG);
  tft.setCursor(x, y);
  tft.print(cur);
}

void drawCO2Value(uint16_t eco2, uint16_t col) {
  char co2Buf[12];
  sprintf(co2Buf, "%4uppm", eco2);
  printCenteredText(String(co2Buf),
                    GRID_MID_X, GRID_R,
                    BOTTOM_VALUE_Y,
                    col, CYBER_BG, 1);
}

void drawEnvDynamic(float temp, float hum, uint16_t tvoc, uint16_t eco2) {
  uint16_t colHUMI = CYBER_ACCENT;
  uint16_t colTEMP = CYBER_LIGHT;
  uint16_t colTVOC = CYBER_GREEN;
  uint16_t colCO2  = colorForCO2(eco2);

  // HUMI
  char humBuf[8];
  sprintf(humBuf, "%2.0f%%", hum);
  printCenteredText(String(humBuf),
                    GRID_L, GRID_MID_X,
                    TOP_VALUE_Y,
                    colHUMI, CYBER_BG, 1);

  // TEMP
  char tempBuf[10];
  sprintf(tempBuf, "%2.1fC", temp);
  printCenteredText(String(tempBuf),
                    GRID_MID_X, GRID_R,
                    TOP_VALUE_Y,
                    colTEMP, CYBER_BG, 1);

  // TVOC (mg/m3)
  float tvoc_mg = tvoc / 1000.0f;
  char tvocBuf[16];
  sprintf(tvocBuf, "%.3fmg/m3", tvoc_mg);
  printCenteredText(String(tvocBuf),
                    GRID_L, GRID_MID_X,
                    BOTTOM_VALUE_Y,
                    colTVOC, CYBER_BG, 1);

  // CO2 (ppm)
  drawCO2Value(eco2, colCO2);

  // Thanh màu + tam giác
  uint8_t level = 1;
  if (eco2 > 1800) level = 4;
  else if (eco2 > 1200) level = 3;
  else if (eco2 > 800)  level = 2;

  tft.fillRect(0, BAR_Y + BAR_H + 1, 160, 8, CYBER_BG);
  int centerX = BAR_MARGIN_X + (BAR_W / 2) + (level - 1) * (BAR_W + BAR_GAP);
  int tipY    = BAR_Y + BAR_H + 2;
  tft.fillTriangle(centerX,     tipY - 4,
                   centerX - 4, tipY + 2,
                   centerX + 4, tipY + 2,
                   ST77XX_WHITE);
}

// ========= Menu UI =========
void drawMenu() {
  tft.fillScreen(CYBER_BG);

  tft.setTextSize(1);
  tft.setTextColor(CYBER_LIGHT);
  tft.setCursor(10, 10);
  tft.print("MODE SELECT");

  const char* items[MENU_ITEMS] = {
    "Monitor",
    "Pomodoro",
    "Alarm",
    "DVD",
    "Day Counter"
  };

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 32 + i * 18;
    if (i == menuIndex) {
      tft.fillRect(6, y - 2, 148, 14, CYBER_ACCENT);
      tft.setTextColor(CYBER_BG);
    } else {
      tft.fillRect(6, y - 2, 148, 14, CYBER_BG);
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setCursor(12, y);
    tft.print(items[i]);
  }
  drawAlarmIcon();
}

// ========= Pomodoro =========
uint16_t pomoColorFromFrac(float f) {
  if (f < 0.33f) return AQ_BAR_GREEN;
  if (f < 0.66f) return AQ_BAR_YELLOW;
  if (f < 0.85f) return AQ_BAR_ORANGE;
  return AQ_BAR_RED;
}

// Vòng Pomodoro 270°
void drawPomodoroRing(float progress) {
  int cx = 80;
  int cy = 64;
  int rOuter = 55;
  int rInner = 44;

  // Vẽ cung 270°: từ -225° đến +45° (chừa hở đáy)
  const float startDeg = -225.0f;
  const float endDeg   =   45.0f;
  const float spanDeg  = endDeg - startDeg;   // 270°

  for (float deg = startDeg; deg <= endDeg; deg += 6.0f) {
    float frac = (deg - startDeg) / spanDeg;   // 0..1
    uint16_t col = (frac <= progress) ? pomoColorFromFrac(frac) : CYBER_DARK;

    float rad = deg * PI / 180.0f;
    int xOuter = cx + cos(rad) * rOuter;
    int yOuter = cy + sin(rad) * rOuter;
    int xInner = cx + cos(rad) * rInner;
    int yInner = cy + sin(rad) * rInner;
    tft.drawLine(xInner, yInner, xOuter, yOuter, col);
  }
}

void drawPomodoroScreen(bool forceStatic) {
  bool needStatic = forceStatic;
  if (prevPomoPreset != pomoPresetIndex || prevPomoStateInt != (int)pomoState) {
    needStatic = true;
    prevPomoPreset   = pomoPresetIndex;
    prevPomoStateInt = (int)pomoState;
  }

  if (needStatic) {
    tft.fillScreen(CYBER_BG);
    tft.fillRect(0, 0, 160, 16, CYBER_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 4);
    tft.setTextColor(CYBER_LIGHT);
    tft.print("POMODORO");
    drawAlarmIcon();
  }

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.fillRect(100, 0, 60, 16, CYBER_BG);
  tft.setCursor(108, 4);
  tft.printf("%2d min", pomoDurationsMin[pomoPresetIndex]);

  unsigned long durationMs = pomoDurationsMin[pomoPresetIndex] * 60UL * 1000UL;
  unsigned long elapsed = 0;
  if (pomoState == POMO_RUNNING)      elapsed = millis() - pomoStartMillis;
  else if (pomoState == POMO_PAUSED)  elapsed = pomoPausedMillis - pomoStartMillis;
  else if (pomoState == POMO_DONE)    elapsed = durationMs;
  if (elapsed > durationMs) elapsed = durationMs;

  float progress = (durationMs > 0) ? (float)elapsed / durationMs : 0.0f;
  drawPomodoroRing(progress);

  int cx = 80;
  int cy = 64;
  tft.fillCircle(cx, cy, 38, CYBER_BG);

  unsigned long remain = durationMs - elapsed;
  uint16_t rmMin = remain / 60000UL;
  uint8_t  rmSec = (remain / 1000UL) % 60;

  char buf[8];
  sprintf(buf, "%02d:%02d", rmMin, rmSec);

  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(3);
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w / 2, cy - 8);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.print(buf);

  // Footer: chỉ hiện Paused / Completed
  tft.fillRect(0, 96, 160, 32, CYBER_BG);
  tft.setTextSize(1);
  tft.setCursor(8, 100);
  tft.setTextColor(CYBER_GREEN, CYBER_BG);
  if (pomoState == POMO_PAUSED)      tft.print("Paused");
  else if (pomoState == POMO_DONE)   tft.print("Completed");
}

// ========= Alarm UI =========
void drawAlarmScreen(bool full) {
  if (full) {
    tft.fillScreen(CYBER_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 4);
    tft.setTextColor(CYBER_LIGHT);
    tft.print("ALARM");
    drawAlarmIcon();
  }

  char hBuf[3];
  char mBuf[3];
  sprintf(hBuf, "%02d", alarmHour);
  sprintf(mBuf, "%02d", alarmMinute);

  tft.setTextSize(3);

  String disp = String(hBuf) + ":" + String(mBuf);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(disp, 0, 0, &x1, &y1, &w, &h);
  int x = (160 - w) / 2;
  int y = 30;

  tft.setCursor(x, y);
  tft.setTextColor(alarmSelectedField == 0 ? CYBER_LIGHT : ST77XX_WHITE, CYBER_BG);
  tft.print(hBuf);

  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.print(":");

  tft.setTextColor(alarmSelectedField == 1 ? CYBER_LIGHT : ST77XX_WHITE, CYBER_BG);
  tft.print(mBuf);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.fillRect(20, 80, 120, 24, CYBER_BG);
  tft.setCursor(30, 84);
  tft.print("Alarm:");
  tft.setCursor(80, 84);
  uint16_t enColor = alarmSelectedField == 2
                     ? CYBER_LIGHT
                     : (alarmEnabled ? CYBER_GREEN : ST77XX_RED);
  tft.setTextColor(enColor, CYBER_BG);
  tft.print(alarmEnabled ? "ON " : "OFF");
}

void drawAlarmRingingScreen() {
  tft.fillScreen(ST77XX_RED);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(30, 40);
  tft.print("ALARM!");
}

// ========= Alarm logic =========
void checkAlarmTrigger() {
  if (!alarmEnabled || alarmRinging) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == alarmHour &&
      timeinfo.tm_min  == alarmMinute &&
      timeinfo.tm_sec  == 0) {

    alarmRinging = true;
    lastAlarmDayTriggered = timeinfo.tm_mday;
    currentMode = MODE_ALARM;
    drawAlarmRingingScreen();
  }
}

// ========= Alert visual + audio =========
void updateAlertStateAndLED() {
  if (alarmRinging) currentAlertLevel = ALERT_ALARM;
  else if (curECO2 > 1800) currentAlertLevel = ALERT_CO2;
  else currentAlertLevel = ALERT_NONE;

  unsigned long now = millis();

  unsigned long interval;
  if (currentAlertLevel == ALERT_ALARM)      interval = 120;
  else if (currentAlertLevel == ALERT_CO2)   interval = 250;
  else                                       interval = 1000;

  if (now - lastLedToggleMs > interval) {
    lastLedToggleMs = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  if (currentAlertLevel == ALERT_CO2) {
    if (now - lastCo2BlinkMs > 350) {
      lastCo2BlinkMs = now;
      co2BlinkOn = !co2BlinkOn;
      uint16_t baseCol = colorForCO2(curECO2);
      uint16_t col = co2BlinkOn ? baseCol : CYBER_DARK;
      drawCO2Value(curECO2, col);
      tone(BUZZ_PIN, 1800, 80);
    }
  }
}


// ========= DVD screensaver =========

bool dvdInited = false;
int  dvdX, dvdY;
int  dvdVX = 2;
int  dvdVY = 1;
int  dvdW  = 50;
int  dvdH  = 18;
unsigned long lastDvdMs = 0;
unsigned long dvdInterval = 35;

uint16_t dvdColors[] = {
  ST77XX_WHITE,
  CYBER_ACCENT,
  CYBER_LIGHT,
  CYBER_GREEN,
  CYBER_PINK,
  ST77XX_YELLOW
};
int dvdColorIndex = 0;

void drawDvdLogo(int x, int y, uint16_t c) {
  // x,y: top-left
  tft.fillRoundRect(x, y, dvdW, dvdH, 4, c);
  tft.drawRoundRect(x, y, dvdW, dvdH, 4, CYBER_BG);

  // chữ DVD ở giữa
  tft.setTextSize(1);
  tft.setTextColor(CYBER_BG, c);
  int16_t bx, by;
  uint16_t w, h;
  tft.getTextBounds("DVD", 0, 0, &bx, &by, &w, &h);
  int tx = x + (dvdW - (int)w) / 2;
  int ty = y + (dvdH - (int)h) / 2 + 1;
  tft.setCursor(tx, ty);
  tft.print("DVD");
}

void initDvd() {
  dvdInited = true;
  tft.fillScreen(CYBER_BG);

  // header
  tft.setTextSize(1);
  tft.setTextColor(CYBER_LIGHT, CYBER_BG);
  tft.setCursor(8, 4);
  tft.print("DVD");

  drawAlarmIcon();

  // vùng chạy logo: chừa header 16px
  dvdX = 40;
  dvdY = 40;
  dvdVX = 2;
  dvdVY = 1;
  lastDvdMs = millis();
  dvdColorIndex = 0;

  drawDvdLogo(dvdX, dvdY, dvdColors[dvdColorIndex]);
}

void updateDvd(int encStep, bool encPressed, bool backPressed) {
  (void)encPressed;

  if (backPressed) {
    dvdInited = false;
    currentMode = MODE_MENU;
    drawMenu();
    return;
  }

  // encoder chỉnh nhẹ tốc độ ngang
  if (encStep != 0) {
    int speed = abs(dvdVX) + encStep;
    if (speed < 1) speed = 1;
    if (speed > 4) speed = 4;
    dvdVX = (dvdVX >= 0 ? 1 : -1) * speed;
  }

  unsigned long now = millis();
  if (now - lastDvdMs < dvdInterval) return;
  lastDvdMs = now;

  // xóa logo cũ
  tft.fillRoundRect(dvdX, dvdY, dvdW, dvdH, 4, CYBER_BG);

  // update vị trí
  dvdX += dvdVX;
  dvdY += dvdVY;

  // biên trong vùng màn
  int left   = 0;
  int right  = 160 - dvdW;
  int top    = 16;
  int bottom = 128 - dvdH;

  bool hitX = false;
  bool hitY = false;

  if (dvdX <= left) {
    dvdX = left;
    dvdVX = -dvdVX;
    hitX = true;
  } else if (dvdX >= right) {
    dvdX = right;
    dvdVX = -dvdVX;
    hitX = true;
  }

  if (dvdY <= top) {
    dvdY = top;
    dvdVY = -dvdVY;
    hitY = true;
  } else if (dvdY >= bottom) {
    dvdY = bottom;
    dvdVY = -dvdVY;
    hitY = true;
  }

  // nếu đập vào góc (cả X & Y cùng va chạm) => đổi màu
  if (hitX && hitY) {
    dvdColorIndex++;
    if (dvdColorIndex >= (int)(sizeof(dvdColors) / sizeof(dvdColors[0]))) {
      dvdColorIndex = 0;
    }
    tone(BUZZ_PIN, 1500, 80);
  }

  drawDvdLogo(dvdX, dvdY, dvdColors[dvdColorIndex]);
}

//========= WHEATER STUF ===========
/*
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  
  // We still fetch the forecast
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "&appid=" + weatherKey + "&units=metric&cnt=6";
  
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Filter to save memory
    JsonDocument filter;
    filter["list"][0]["dt"] = true;
    filter["list"][0]["main"]["temp"] = true;
    filter["list"][0]["weather"][0]["main"] = true;
    filter["city"]["timezone"] = true;

    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (!error) {
      long timezone = doc["city"]["timezone"];
      
      // --- POINT 0: CURRENT WEATHER (NOW) ---
      // We use the global 'outTemp' we fetched earlier (or use 1st forecast slot as proxy if needed)
      // Note: Best practice is to assume fetchWeather updates outTemp first. 
      // If outTemp is 0 (first run), we'll trust the first forecast slot.
      if (outTemp == 0) outTemp = doc["list"][0]["main"]["temp"];
      
      fTemps[0] = outTemp;
      fRain[0]  = (outDesc.indexOf("Rain") >= 0 || outDesc.indexOf("Drizzle") >= 0 || outDesc.indexOf("Thunder") >= 0);
      
      // Get current local hour
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        fHours[0] = timeinfo.tm_hour;
      } else {
        fHours[0] = 0; 
      }

      // --- POINTS 1-5: FORECAST (Next 15 hours) ---
      for(int i=0; i<5; i++) {
        fTemps[i+1] = doc["list"][i]["main"]["temp"];
        
        const char* w = doc["list"][i]["weather"][0]["main"];
        String cond = String(w);
        fRain[i+1] = (cond.indexOf("Rain") >= 0 || cond.indexOf("Drizzle") >= 0 || cond.indexOf("Thunder") >= 0);

        // Calculate Hour
        unsigned long dt = doc["list"][i]["dt"];
        time_t rawTime = (time_t)(dt + timezone); 
        struct tm* ti = gmtime(&rawTime);   
        fHours[i+1] = ti->tm_hour;
      }
      
      weatherLoaded = true;
      forecastLoaded = true;
      Serial.println("Forecast Updated (Start=Now).");
    }
  }
  http.end();
}
*/
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  
  // URL: Get 5-day forecast (we use the first few slots)
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "&appid=" + weatherKey + "&units=metric&cnt=6";
  
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // --- MEMORY FILTER ---
    JsonDocument filter;
    filter["list"][0]["dt"] = true;
    filter["list"][0]["main"]["temp"] = true;
    filter["list"][0]["main"]["humidity"] = true; // <--- ADDED THIS!
    filter["list"][0]["weather"][0]["main"] = true;
    filter["city"]["timezone"] = true;

    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (!error) {
      long timezone = doc["city"]["timezone"];
      
      // --- 1. UPDATE CURRENT WEATHER VARIABLES ---
      // The first item in the list (index 0) is the forecast for "Now" (or very close to it)
      outTemp = doc["list"][0]["main"]["temp"];
      outHum  = doc["list"][0]["main"]["humidity"]; // <--- Fixes the "0%" issue
      const char* d = doc["list"][0]["weather"][0]["main"];
      outDesc = String(d);                            // <--- Fixes the "--" issue
      
      // --- 2. UPDATE GRAPH DATA (Points 0-5) ---
      
      // Point 0 is "Now"
      fTemps[0] = outTemp;
      fRain[0]  = (outDesc.indexOf("Rain") >= 0 || outDesc.indexOf("Drizzle") >= 0 || outDesc.indexOf("Thunder") >= 0);
      
      // Calculate "Now" Hour
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        fHours[0] = timeinfo.tm_hour;
      } else {
        fHours[0] = 0; 
      }

      // Points 1-5 (Next 15 hours)
      for(int i=0; i<5; i++) {
        fTemps[i+1] = doc["list"][i]["main"]["temp"]; // Note: list[i] is actually list[i+1] in full json but filter might compress.
                                                      // Actually, strictly speaking, filter["list"][0] applies to ALL list items in ArduinoJson 7 usually, 
                                                      // but safely: the API returns list[0], list[1] etc. 
                                                      // We iterate 0 to 4 from the JSON list to fill graph 1 to 5.
                                                      // Wait, we need to be careful with indices. 
                                                      // Let's rely on standard parsing:
        
        float t = doc["list"][i]["main"]["temp"]; 
        // If the filter worked correctly, it applies the pattern to all array elements.
        
        fTemps[i+1] = doc["list"][i]["main"]["temp"];
        const char* w = doc["list"][i]["weather"][0]["main"];
        String cond = String(w);
        fRain[i+1] = (cond.indexOf("Rain") >= 0 || cond.indexOf("Drizzle") >= 0 || cond.indexOf("Thunder") >= 0);

        unsigned long dt = doc["list"][i]["dt"];
        time_t rawTime = (time_t)(dt + timezone); 
        struct tm* ti = gmtime(&rawTime);   
        fHours[i+1] = ti->tm_hour;
      }
      
      weatherLoaded = true;
      forecastLoaded = true;
      Serial.println("Weather & Forecast Updated.");
    } else {
      Serial.print("JSON Error: "); Serial.println(error.c_str());
    }
  }
  http.end();
}

/*
void drawWeatherScreen() {
  tft.fillScreen(CYBER_BG);
  
  // Header
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 5);
  tft.print("METEO for "); 
  tft.print(city);
  
  if (!weatherLoaded) {
    tft.setCursor(20, 60);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Loading...");
    return;
  }

  // Draw Big Temperature
  tft.setTextSize(3);
  tft.setTextColor(CYBER_ACCENT);
  tft.setCursor(30, 40);
  tft.print(outTemp, 1);
  tft.setTextSize(1);
  tft.print(" C");

  // Draw Condition (Rain/Clear/etc)
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  int descW = outDesc.length() * 12;
  tft.setCursor((160 - descW) / 2, 80);
  tft.print(outDesc);

  // Draw Humidity
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(40, 110);
  tft.print("Humidity: ");
  tft.print(outHum);
  tft.print("%");
  
  drawAlarmIcon();
}
*/ 

void drawWeatherScreen() {
  // 1. Determine Background based on Condition
  uint16_t bgCol = CYBER_BG; // Default Black
  uint16_t txtCol = ST77XX_WHITE;
  
  if (outDesc.indexOf("Rain") >= 0 || outDesc.indexOf("Drizzle") >= 0) {
    bgCol = BG_RAIN; 
  } else if (outDesc.indexOf("Cloud") >= 0 || outDesc.indexOf("Mist") >= 0 || outDesc.indexOf("Fog") >= 0) {
    bgCol = BG_CLOUDS;
  } else if (outDesc.indexOf("Snow") >= 0) {
    bgCol = ST77XX_WHITE;
    txtCol = ST77XX_BLACK;
  } else if (outDesc.indexOf("Thunder") >= 0) {
    bgCol = BG_THUNDER;
  } else if (outDesc.indexOf("Clear") >= 0) {
    bgCol = BG_CLEAR; 
  }
  
  tft.fillScreen(bgCol);
  
  // 2. Header (Top Left)
  tft.setTextColor(txtCol, bgCol);
  tft.setTextSize(1);
  tft.setCursor(10, 5);
  tft.print("METEO: "); 
  tft.print(city);
  
  if (!weatherLoaded) {
    tft.setCursor(10, 60);
    tft.setTextColor(ST77XX_WHITE, bgCol);
    tft.print("Loading...");
    return;
  }

  // 3. Big Temperature (Left Aligned)
  tft.setTextSize(3);
  tft.setTextColor(txtCol, bgCol); // Cyan text on colored BG
  tft.setCursor(10, 35);                 // Moved to Left
  tft.print(outTemp, 1);
  tft.setTextSize(1);
  tft.print(" C");

  // 4. Weather Condition Text (Left Aligned)
  tft.setTextSize(2);
  tft.setTextColor(txtCol, bgCol); // White text is safest on colors
  tft.setCursor(10, 70);                 // Moved to Left
  tft.print(outDesc);

  // 5. Humidity (Left Aligned)
  tft.setTextSize(1);
  tft.setTextColor(txtCol, bgCol);
  tft.setCursor(10, 100);                // Moved to Left
  tft.print("Humidity: ");
  tft.print(outHum);
  tft.print("%");
  
  // Optional: Redraw Alarm Icon since fillScreen wiped it
  // (We need to update drawAlarmIcon to accept a bg color if we want it perfect, 
  // but for now, this works if the icon area is black or transparent)
  drawAlarmIcon(bgCol); 
}


void drawGraphScreen() {
  tft.fillScreen(CYBER_BG);

  // Title
  tft.setTextSize(1);
  //tft.setTextColor(CYBER_LIGHT);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 4);
  //tft.print("FORECAST (Now -> +15h)");
  tft.print("Forecast Now -> +15h");

  if (!forecastLoaded) {
    tft.setCursor(20, 60);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Loading Data...");
    return;
  }

  // --- 1. Find Min/Max & Their Indices ---
  float minT = 100, maxT = -100;
  int minIdx = 0, maxIdx = 0;

  for(int i=0; i<6; i++) {
    if(fTemps[i] < minT) { minT = fTemps[i]; minIdx = i; }
    if(fTemps[i] > maxT) { maxT = fTemps[i]; maxIdx = i; }
  }
  
  // Padding
  float rangeMin = minT - 1.0;
  float rangeMax = maxT + 1.0;
  if (rangeMax - rangeMin < 2.0) rangeMax = rangeMin + 2.0;

  // --- 2. Graph Dimensions (TALLER) ---
  int graphX = 16;       // Left margin
  int graphY = 25;       // Top margin
  int graphW = 128;      // Width
  int graphH = 75;       // INCREASED HEIGHT (was 60)
  int graphBot = graphY + graphH;

  // Draw Grid Lines (Horizontal)
  tft.drawFastHLine(graphX - 4, graphY, graphW + 8, CYBER_DARK); // Top
  tft.drawFastHLine(graphX - 4, graphBot, graphW + 8, CYBER_DARK); // Bottom

  // --- 3. Draw "NOW" Line (Red Dotted) at Index 0 ---
  for (int y = graphY; y < graphBot; y += 4) {
    tft.drawPixel(graphX, y, ST77XX_RED);
    tft.drawPixel(graphX, y+1, ST77XX_RED);
  }
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(graphX - 6, graphY - 10);
  //tft.print("NOW");

  // --- 4. Draw Plot ---
  int stepX = graphW / 5; // 5 segments between 6 points
  
  // Store coordinates to draw labels later
  int ptX[6];
  int ptY[6];

  for (int i = 0; i < 6; i++) {
    // Calculate Coordinates
    ptX[i] = graphX + i * stepX;
    ptY[i] = map(fTemps[i] * 10, rangeMin * 10, rangeMax * 10, graphBot, graphY);
    
    // Draw Line to next point
    if (i < 5) {
      int nextY = map(fTemps[i+1] * 10, rangeMin * 10, rangeMax * 10, graphBot, graphY);
      int nextX = graphX + (i + 1) * stepX;
      
      //uint16_t color = CYBER_GREEN;
      uint16_t color = ST77XX_WHITE;
      //if (fRain[i] || fRain[i+1]) color = CYBER_BLUE;
      if (fRain[i] || fRain[i+1]) color = RAIN; 

      tft.drawLine(ptX[i], ptY[i], nextX, nextY, color);
      tft.drawLine(ptX[i], ptY[i]+1, nextX, nextY+1, color); // Thicker line
    }

    // Draw Circle Point
    tft.fillCircle(ptX[i], ptY[i], 2, ST77XX_WHITE);

    // Draw Hour Label (X-axis)
    tft.setTextColor(CYBER_LIGHT);
    // Adjust X to center the number
    int labelOffset = (fHours[i] < 10) ? -3 : -6; 
    tft.setCursor(ptX[i] + labelOffset, graphBot + 6);
    tft.print(fHours[i]);
  }

  // --- 5. Draw Dynamic Min/Max Labels ---
  // Max Label (Above the point)
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(ptX[maxIdx] - 6, ptY[maxIdx] - 12);
  tft.print((int)maxT);

  // Min Label (Below the point)
  tft.setTextColor(ST77XX_WHITE); // Coldest color
  tft.setCursor(ptX[minIdx] - 6, ptY[minIdx] + 8);
  tft.print((int)minT);
}

//=========

// ========= Day Counter / Year Progression (grid and circle) =========

bool isLeap(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// TAB 1: The Matrix Grid
void drawYearGrid(int dayIdx, int totalDays) {
  tft.fillScreen(CYBER_BG);

  // 2. Header
  tft.setTextSize(1);
  tft.setTextColor(CYBER_LIGHT);
  tft.setCursor(6, 4);
  tft.print("YEAR PROGRESS");
  
  // Draw percentage
  float progress = ((float)(dayIdx + 1) / totalDays) * 100.0;
  tft.setCursor(110, 4);
  tft.setTextColor(CYBER_ACCENT);
  tft.print((int)progress); tft.print("%");

  drawAlarmIcon(); // Keep UI consistent

  // 3. Grid Settings
  // We have ~160px width. Let's do 25 columns.
  // 160 / 25 = ~6.4 pixels per cell. 
  // We will use 3x3 px box + 3px gap.
  
  int cols = 25;
  int startX = 5; 
  int startY = 20;
  int cellStep = 6; // 3px box + 3px gap
  int boxSize = 3;

  for (int i = 0; i < totalDays; i++) {
    int row = i / cols;
    int col = i % cols;

    int x = startX + col * cellStep;
    int y = startY + row * cellStep;

    uint16_t color;

    if (i < dayIdx) {
      // Past days: Dim Green or Darker Accent
      color = 0x03E0; // A darker green (custom) or use CYBER_DARK
    } else if (i == dayIdx) {
      // Today: Bright White or Pink
      color = ST77XX_WHITE; 
    } else {
      // Future: Very dark gray (placeholder)
      color = 0x2104; // Very dark grey
    }

    tft.fillRect(x, y, boxSize, boxSize, color);
    
    // Optional: Highlight "Today" with an extra outline or color
    if (i == dayIdx) {
      tft.drawRect(x-1, y-1, boxSize+2, boxSize+2, CYBER_PINK);
    }
  }

  // 4. Footer Info
  tft.setCursor(6, 118);
  tft.setTextColor(ST77XX_WHITE);
  tft.printf("Day %d / %d", dayIdx + 1, totalDays);
}

// ==========================================
//      YEAR PROGRESS: 12-Month "Sausage" Ring
// ==========================================

// Helper to draw a thick arc segment with rounded ends ("Sausage")
// cx, cy: Center of screen
// r: Mid-radius (center of the sausage thickness)
// w: Thickness (width of the sausage)
// startAngle, endAngle: Degrees (0 is right, -90 is up)
// color: Fill color
void drawSausageSegment(int cx, int cy, float r, float w, float startAngle, float endAngle, uint16_t color) {
  // 1. Draw the Arc Body (using radial lines for "solid" look)
  // We step by 1 degree (or 2 for speed) to fill the arc
  float halfW = w / 2.0;
  float rIn  = r - halfW;
  float rOut = r + halfW;
  
  // Convert to radians
  float degStep = 1.0; 
  
  for (float d = startAngle; d <= endAngle; d += degStep) {
    float rad = d * PI / 180.0;
    float cosA = cos(rad);
    float sinA = sin(rad);
    
    int x1 = cx + cosA * rIn;
    int y1 = cy + sinA * rIn;
    int x2 = cx + cosA * rOut;
    int y2 = cy + sinA * rOut;
    
    tft.drawLine(x1, y1, x2, y2, color);
  }

  // 2. Draw Rounded Caps (Circles at start and end)
  // Start Cap
  /*float radStart = startAngle * PI / 180.0;
  int capX1 = cx + cos(radStart) * r;
  int capY1 = cy + sin(radStart) * r;
  tft.fillCircle(capX1, capY1, halfW, color);

  // End Cap
  float radEnd = endAngle * PI / 180.0;
  int capX2 = cx + cos(radEnd) * r;
  int capY2 = cy + sin(radEnd) * r;
  tft.fillCircle(capX2, capY2, halfW, color);*/
}

void drawYearCircle(int dayIdx, int totalDays) {
  tft.fillScreen(CYBER_BG);

  // --- HEADER ---
  tft.setTextSize(1);
  tft.setTextColor(CYBER_LIGHT);
  tft.setCursor(6, 4);
  tft.print("YEAR PROGRESS"); // Shortened to fit nicely

  // Stats Text
  int percent = (int)(((float)(dayIdx + 1) / totalDays) * 100);
  tft.setCursor(120, 4);
  tft.setTextColor(CYBER_ACCENT);
  tft.print(percent); tft.print("%");

  // --- LAYOUT ---
  int cx = 80;
  int cy = 72;        // Moved down to leave space for header
  float radius = 38;  // Radius of the ring center
  float thickness = 10; // Thickness of the sausage
  
  // Month Data
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
  // Leap Year Check (Global 'totalDays' passed in is already 366 if leap)
  if (totalDays == 366) daysInMonth[1] = 29;

  // Calculate current month and day-of-month
  int currentMonth = 0;
  int tempDays = dayIdx; // Copy of day index
  for (int i = 0; i < 12; i++) {
    if (tempDays < daysInMonth[i]) {
      currentMonth = i;
      break;
    }
    tempDays -= daysInMonth[i];
  }
  int dayOfMonth = tempDays; // Remaining days is the day in current month

  // --- DRAWING LOOP ---
  // We go from -90 (Top) clockwise
  float anglePerMonth = 360.0 / 12.0; 
  float gap = 6.0; // Degrees of gap between sausages
  
  for (int m = 0; m < 12; m++) {
    // Math: -90 is 12 o'clock. 
    float startDeg = -90.0 + (m * anglePerMonth) + (gap / 2.0);
    float endDeg   = startDeg + (anglePerMonth - gap);

    // Render Logic
    if (m < currentMonth) {
      // 1. PAST MONTH: Full Green Sausage
      drawSausageSegment(cx, cy, radius, thickness, startDeg, endDeg, CYBER_GREEN);
    } 
    else if (m > currentMonth) {
      // 2. FUTURE MONTH: Dark "Empty" Sausage
      drawSausageSegment(cx, cy, radius, thickness, startDeg, endDeg, CYBER_DARK);
    } 
    else {
      // 3. CURRENT MONTH: Partial Fill
      // First draw the dark background for the whole month
      drawSausageSegment(cx, cy, radius, thickness, startDeg, endDeg, CYBER_DARK);
      
      // Calculate how much to fill
      float progress = (float)(dayOfMonth + 1) / daysInMonth[m];
      float span = endDeg - startDeg;
      float fillEnd = startDeg + (span * progress);
      
      // Draw the filled portion in Accent Color (Pink/Blue)
      // Note: If day is 0, we might draw a tiny dot, which is fine.
      drawSausageSegment(cx, cy, radius, thickness, startDeg, fillEnd, CYBER_PINK);
    }
  }

  // --- CENTER INFO ---
  // Show Month Name or Date in center
  const char* monthNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  
  // Center text alignment logic
  String mName = String(monthNames[currentMonth]);
  int16_t x1, y1; 
  uint16_t w, h;
  tft.getTextBounds(mName, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w/2, cy - h/2 - 2);
  tft.print(mName);
  
  // Small day number below
  tft.setTextSize(1);
  tft.setTextColor(CYBER_LIGHT);
  tft.setCursor(cx - 6, cy + 10);
  tft.print(dayOfMonth + 1);
}


// ========= SETUP =========
void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(ENC_A_PIN,   INPUT_PULLUP);
  pinMode(ENC_B_PIN,   INPUT_PULLUP);
  pinMode(ENC_BTN_PIN, INPUT_PULLUP);
  pinMode(KEY0_PIN,    INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(CYBER_BG);

  connectWiFiAndSyncTime();

  if (!aht.begin()) Serial.println("AHT21 not found");
  if (!ens160.begin()) Serial.println("ENS160 begin FAIL");
  else ens160.setMode(ENS160_OPMODE_STD);

  updateEnvSensors(true);

  // khởi động vào Monitor (clock)
  initClockStaticUI();
  prevTimeStr = "";
  drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
  drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
}

// ========= LOOP =========
void loop() {

  int encStep     = readEncoderStep();
  bool encPressed = checkButtonPressed(ENC_BTN_PIN, lastEncBtn);
  bool k0Pressed  = checkButtonPressed(KEY0_PIN,    lastKey0);

  checkAlarmTrigger();
  updateAlertStateAndLED();

  switch (currentMode) {
    case MODE_MENU: {
      if (encStep != 0) {
        menuIndex += encStep;
        if (menuIndex < 0) menuIndex = MENU_ITEMS - 1;
        if (menuIndex >= MENU_ITEMS) menuIndex = 0;
        drawMenu();
      }
      if (encPressed) {
        if (menuIndex == 0) {
          currentMode = MODE_CLOCK;
          initClockStaticUI();
          prevTimeStr = "";
          updateEnvSensors(true);
          drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
          drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
        } else if (menuIndex == 1) {
          currentMode = MODE_POMODORO;
          pomoState = POMO_SELECT;
          prevPomoRemainSec = -1;
          prevPomoPreset = -1;
          prevPomoStateInt = -1;
          drawPomodoroScreen(true);
        } else if (menuIndex == 2) {
          currentMode = MODE_ALARM;
          alarmSelectedField = 0;
          drawAlarmScreen(true);
        } else if (menuIndex == 3) {
          currentMode = MODE_DVD;
          dvdInited = false;
        } else if (menuIndex == 4) {
          currentMode = MODE_DAY_COUNTER;
        }
      }
      break;
    }
 
    case MODE_CLOCK: {
      // 0 = Monitor, 1 = Weather, 2 = Forecast Graph
      static int clockPage = 0; 
      static int prevClockPage = -1;

      // 1. Handle Encoder (Switch Pages)
      if (encStep != 0) {
        clockPage += encStep;
        if (clockPage < 0) clockPage = 0; 
        if (clockPage > 2) clockPage = 2; 
      }

      // 2. Page Change Logic (Draw static UI)
      if (clockPage != prevClockPage) {
        prevClockPage = clockPage;
        tft.fillScreen(CYBER_BG);
        
        if (clockPage == 0) {
          // --- PAGE 0: MONITOR (INSTANT LOAD) ---
          // We do NOT fetch weather here.
          initClockStaticUI();
          prevTimeStr = ""; 
          updateEnvSensors(true); 
          drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
          drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
        } 
        else if (clockPage == 1) {
          // --- PAGE 1: WEATHER (FETCH ONLY HERE) ---
          // Only fetch if we haven't yet, or if data is old (>15 mins)
          if (!weatherLoaded || millis() - lastWeatherFetch > 900000) {
             tft.setCursor(10, 60); tft.setTextColor(CYBER_LIGHT); tft.print("Fetching Weather...");
             fetchWeather(); // <--- This takes 1-2 seconds
             lastWeatherFetch = millis();
             tft.fillScreen(CYBER_BG); // Clear "Fetching" text
          }
          drawWeatherScreen();
        }
        else {
          // --- PAGE 2: GRAPH (FETCH ONLY HERE) ---
          if (!forecastLoaded || millis() - lastWeatherFetch > 900000) { 
             tft.setCursor(10, 60); tft.setTextColor(CYBER_LIGHT); tft.print("Fetching Forecast...");
             fetchWeather(); 
             lastWeatherFetch = millis();
             tft.fillScreen(CYBER_BG);
          }
          drawGraphScreen();
        }
      }

      // 3. Update Loop (Refreshes data while staying on the page)
      if (clockPage == 0) {
        // --- Monitor Update ---
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          int sec = timeinfo.tm_sec;
          if (sec != prevSecond) {
            prevSecond = sec;
            drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
            if (sec % 5 == 0) {
              updateEnvSensors(true);
              drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
            }
          }
        }
      } 
      else {
        // --- Weather/Graph Auto-Refresh (every 15 mins) ---
        if (millis() - lastWeatherFetch > 900000) { 
          fetchWeather();
          lastWeatherFetch = millis();
          if (clockPage == 1) drawWeatherScreen();
          if (clockPage == 2) drawGraphScreen();
        }
      }

      // 4. Exit to Menu
      if (k0Pressed) {
        currentMode = MODE_MENU;
        clockPage = 0; // Reset to Monitor for next time
        prevClockPage = -1; 
        drawMenu();
      }
      break;
    }

    case MODE_POMODORO: {
      if (pomoState == POMO_SELECT && encStep != 0) {
        pomoPresetIndex += encStep;
        if (pomoPresetIndex < 0) pomoPresetIndex = 2;
        if (pomoPresetIndex > 2) pomoPresetIndex = 0;
        prevPomoRemainSec = -1;
        drawPomodoroScreen(true);
      }

      if (encPressed) {
        if (pomoState == POMO_SELECT || pomoState == POMO_DONE) {
          pomoState = POMO_RUNNING;
          pomoStartMillis = millis();
          prevPomoRemainSec = -1;
          drawPomodoroScreen(true);
        } else if (pomoState == POMO_RUNNING) {
          pomoState = POMO_PAUSED;
          pomoPausedMillis = millis();
          drawPomodoroScreen(true);
        } else if (pomoState == POMO_PAUSED) {
          unsigned long pauseDur = millis() - pomoPausedMillis;
          pomoStartMillis += pauseDur;
          pomoState = POMO_RUNNING;
          prevPomoRemainSec = -1;
          drawPomodoroScreen(true);
        }
      }

      if (pomoState == POMO_RUNNING || pomoState == POMO_PAUSED || pomoState == POMO_DONE) {
        unsigned long durationMs = pomoDurationsMin[pomoPresetIndex] * 60UL * 1000UL;
        unsigned long elapsed = 0;
        if (pomoState == POMO_RUNNING) elapsed = millis() - pomoStartMillis;
        else if (pomoState == POMO_PAUSED) elapsed = pomoPausedMillis - pomoStartMillis;
        else if (pomoState == POMO_DONE) elapsed = durationMs;

        if (elapsed > durationMs) {
          elapsed = durationMs;
          if (pomoState != POMO_DONE) {
            pomoState = POMO_DONE;
            tone(BUZZ_PIN, 2000, 500);
            drawPomodoroScreen(true);
          }
        }

        int remainSec = (durationMs - elapsed) / 1000UL;
        if (remainSec != prevPomoRemainSec) {
          prevPomoRemainSec = remainSec;
          drawPomodoroScreen(false);
        }
      }

      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
      }
      break;
    }

    case MODE_ALARM: {
      if (alarmRinging) {
        static unsigned long lastBeep = 0;
        if (millis() - lastBeep > 1000) {
          lastBeep = millis();
          tone(BUZZ_PIN, 2000, 400);
        }
        if (encPressed || k0Pressed) {
          alarmRinging = false;
          lastAlarmDayTriggered = -1;
          noTone(BUZZ_PIN);
          drawAlarmScreen(true);
        }
        break;
      }

      bool changed = false;
      if (encStep != 0) {
        if (alarmSelectedField == 0) {
          if (encStep > 0) alarmHour = (alarmHour + 1) % 24;
          else             alarmHour = (alarmHour + 23) % 24;
          changed = true;
        } else if (alarmSelectedField == 1) {
          if (encStep > 0) alarmMinute = (alarmMinute + 1) % 60;
          else             alarmMinute = (alarmMinute + 59) % 60;
          changed = true;
        } else if (alarmSelectedField == 2) {
          alarmEnabled = !alarmEnabled;
          changed = true;
        }

        if (changed) {
          lastAlarmDayTriggered = -1;
        }
      }

      if (encPressed) {
        alarmSelectedField = (alarmSelectedField + 1) % 3;
        changed = true;
      }

      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
        break;
      }

      if (changed) {
        drawAlarmScreen(false);
        drawAlarmIcon();
      }

      break;
    }

    case MODE_DVD: {
      if (!dvdInited) {
        initDvd();
      }
      updateDvd(encStep, encPressed, k0Pressed);
      break;
    }

    case MODE_DAY_COUNTER: {
      static int viewPage = 0;      // 0 = Grid, 1 = Circle
      static int prevViewPage = -1; // Force initial draw

      // 1. Handle Encoder for Tab Switching
      if (encStep != 0) {
        viewPage += encStep;
        if (viewPage < 0) viewPage = 0;
        if (viewPage > 1) viewPage = 1; // Only 2 pages for now
      }

      // 2. Draw ONLY if page changed (Efficient!)
      if (viewPage != prevViewPage) {
        prevViewPage = viewPage;

        // Get Time Data
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
          tft.fillScreen(CYBER_BG);
          tft.setCursor(10, 50);
          tft.setTextColor(ST77XX_RED);
          tft.print("Sync Time First!");
        } else {
          int year = timeinfo.tm_year + 1900;
          int dayIdx = timeinfo.tm_yday; // 0..365
          int total = isLeap(year) ? 366 : 365;

          if (viewPage == 0) drawYearGrid(dayIdx, total);
          else               drawYearCircle(dayIdx, total);
        }
      }

      // 3. Exit to Menu
      if (k0Pressed) {
        currentMode = MODE_MENU;
        viewPage = 0;     // Reset to default tab
        prevViewPage = -1; // Reset dirty flag
        drawMenu();
      }
      break;
    }

  }
}
