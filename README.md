# ESP32-C3 Smart Cyber Clock

# IDE Setup:
Use Arduino IDE (VS Code with Platformio wasn't working due to incompatible esp32 core library version)

1) Install esp32 board manager by Espressif Systems
2) Install these libraries (from Arduino IDE):
  - Adafruit_GFX
  - Adafruit_ST7735
  - Adafruit_AHTX0
  - ScioSense_ENS160
  - ESPAsyncWebServer
  - AsyncTCP

## Possible Errors:
If you encounter errors during compilation or execution you can try some of those things:
- uninstall current esp32 board manager extension from the Arduino IDE and install exactly version 3.0.7 (some libraries may not work with newer core version) and remember to NOT UPDATE the extension if you get a prompt when launching Arduino IDE
- the ESPAsyncWebServer library may require manual patching! Look at [this thread](https://github.com/espressif/arduino-esp32/issues/9753#issuecomment-2196865805)

I encountered problems like those two mainly on Windows, while on Linux everything compiled smoothly.

# Project Setup:
Be sure to create a file named "**secrets.h**" (new tab in the Arduino IDE) and add your Wi-Fi credentials for the ESP32

```cpp
// put this in secrets.h file

#pragma once

// set your WiFi credentials here if you know them ahead of time
#define WIFI_SSID "your wifi ssid here"
#define WIFI_PASSWORD "your wifi password here"

// set your Access Point credentials here to be used when esp goes into "hotspot" mode to enable WiFi credentials setting from external device
#define AP_SSID "Smart_Cyber_Clock"
#define AP_PASSWORD "choose a password fo Access Point mode"
```

# Todo:

- [ ] Integrate Wi-Fi Manager library to setup wifi credentials at runtime
  - [x] rewrite the [AsyncWiFiManagerSimple](https://github.com/marinpopa/AsyncWiFiManagerSimple) library inside my program (and modify HTML etc. using english language) as it is just a couple of functions
  - [ ] integrate the library in a "better way" (maybe wait for WiFi configuration before entering in Monitor Mode); pass a pointer to tft display to print status info on the screen too (not only on serial monitor)
- [ ] Optimize software (low power mode, disable display and wifi when not used)
- [ ] Add settings tab
  - [ ] led mode (off, on, blink)
- [ ] expand on alarm functionality (add more than one alarm; save states in flash for when device reboots)
  - [ ] implement file system (see littlefs or similar libraries)
- [ ] Temperature value correction (the sensor seems to heat up over time leading to wrong readings)
- [ ] develop DayCounter tab (missing logic, expand on UI)
- [ ] develop Word of the Day tab
  - [ ] develop small Web Api to scrape contents and publish it (python or .NET Core)

## Crazy stuff:
- [ ] handle phone calls
- [ ] Spotify controller

# Credits:
Credits for the *original version* of the device and the software and for the Diagram image: **Huy Vector DIY** and his [smart_cyber_clock project](https://sites.google.com/view/huy-materials-used/smart-cyber-clock)

Credits for the wifi_manager class (that was rewritten and translated for this project) goes to [AsyncWiFiManagerSimple](https://github.com/marinpopa/AsyncWiFiManagerSimple)
