# ESP32-C3 Smart Cyber Clock

# Setup:
Be sure to create a file named "**secrets.h**" (new tab in the Arduino IDE) and add your Wi-Fi credentials for the ESP32

```cpp
// put this in secrets.h file

#pragma once

// credentials used when esp goes into "Hotspot Mode" (Accesso Point - AP)
#define AP_SSID ""
#define AP_PASSWORD ""

// API key from openweathermap.org
#define WEATHER_API_KEY ""
#define WEATHER_API_CITY ""     //ex. "London,UK"

```

# Todo:

- [x] Integrate Wi-Fi Manager library to setup wifi credentials at runtime
  - [ ] enable an option to re-enter Access Point mode to set/unset/reset WiFi credentials
- [x] Integrate Weather informations from OpenWeatherMap.org
  - [ ] optimize API calls (only when switching to Weather panels from Monitor tab)
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
