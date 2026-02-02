#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include "time.h"

#include "secrets.h"
#include "src/wifi_manager/wifi_manager.hpp"

// ====== WiFi & Time config ======
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec      = 1 * 3600;   // GMT+0
const int   daylightOffset_sec = 0;
AsyncWiFiManagerSimple wifiManager;

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

#ifndef PI
#define PI 3.1415926
#endif

// ====== Modes ======
enum UIMode {
  MODE_MENU = 0,
  MODE_CLOCK,
};
UIMode currentMode = MODE_CLOCK;       // khởi động vào CLOCK luôn
int menuIndex = 0;
const int MENU_ITEMS = 1;       // Monitor

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

// ========= WiFi & Time =========
void connectWiFiAndSyncTime() {
  tft.fillScreen(CYBER_BG);
  tft.setTextColor(CYBER_LIGHT);
  tft.setTextSize(1);
  tft.setCursor(10, 55);
  tft.print("Connecting to WiFi: ");
  tft.print(WIFI_SSID);
  Serial.print("Connecting to wifi: ");
  Serial.println(WIFI_SSID);
  // TODO optimise this by calling WiFi.disconnect() to close the connection and power off the module ?!?
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(300);
    tft.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    tft.fillScreen(CYBER_BG);
    tft.setCursor(10, 55);
    tft.print("assigned ip: ");
    tft.println(WiFi.localIP());
    Serial.print("assigned ip: ");
    Serial.println(WiFi.localIP());
    tft.print("Syncing time...");
    delay(800);
  } else {
    tft.fillScreen(CYBER_BG);
    tft.setCursor(10, 55);
    tft.setTextColor(ST77XX_RED);
    tft.print("WiFi FAILED!");
    delay(1000);
  }
}

void syncTime() {
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    tft.fillScreen(CYBER_BG);
    tft.setCursor(10, 55);
    tft.print("assigned ip: ");
    tft.println(WiFi.localIP());
    Serial.print("assigned ip: ");
    Serial.println(WiFi.localIP());
    tft.print("Syncing time...");
    delay(800);
  } else {
    tft.fillScreen(CYBER_BG);
    tft.setCursor(10, 55);
    tft.setTextColor(ST77XX_RED);
    tft.print("WiFi FAILED!");
    delay(1000);
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
}

// ========= Alert visual + audio =========
void updateAlertStateAndLED() {
  if (curECO2 > 1800) currentAlertLevel = ALERT_CO2;
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

  wifiManager.Setup(AP_SSID, AP_PASSWORD);

  //connectWiFiAndSyncTime();
  syncTime();

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

  wifiManager.loop();

  /*static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }*/

  int encStep     = readEncoderStep();
  bool encPressed = checkButtonPressed(ENC_BTN_PIN, lastEncBtn);
  bool k0Pressed  = checkButtonPressed(KEY0_PIN,    lastKey0);

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
        // TODO rewrite with a switch OR compare with ENUM values to have it more "generic"
        switch (menuIndex + 1) {
          case MODE_CLOCK: {
            currentMode = MODE_CLOCK;
            initClockStaticUI();
            prevTimeStr = "";
            updateEnvSensors(true);
            drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
            drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
            break;
          }
        }
      }
      break;
    }

    case MODE_CLOCK: {
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
      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
      }
      break;
    }
  }
}
