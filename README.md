# ESP32-C3 Smart Cyber Clock

# Setup:
Be sure to create a file named "**secrets.h**" (new tab in the Arduino IDE) and add your Wi-Fi credentials for the ESP32

```cpp
// put this in secrets.h file

#pragma once

#define AP_SSID "ex. Smart_Cyber_Clock"
#define AP_PASSWORD "yoursecurepassword"

#define WEATHER_API_KEY ""
#define WEATHER_API_CITY "ex. Zurich,CH"

// url used to download updated firmware binaries (directly from this GitHub page releases tab)
#define URL1 "https://github.com/Stefano-De-Ciechi/esp32-c3_smart_cyber_clock/releases/download/current/ladder.bin"
#define URL2 "https://github.com/Stefano-De-Ciechi/esp32-c3_smart_cyber_clock/releases/download/current/original.bin"
```

# Todo:

- [x] Integrate Wi-Fi Manager library to setup wifi credentials at runtime
  - [ ] rewrite Wi-Fi reset option to re-enter hotspot mode
- [ ] Optimize software (low power mode, disable display and wifi when not used)
- [ ] temperature value correction
  - [ ] sensor sleep (if possible)
- [x] Weather visualization
  - [ ] change weather API to open-meteo.com
- [x] Add settings tab
  - [x] led mode (off, on, blink)
  - [x] weather localization
  - [x] display brightness (only on supported hardware version) 
  - [x] display sleep (only on supported hardware version) 
- [ ] expand on alarm functionality (add more than one alarm; save states in flash for when device reboots)
- [ ] Temperature value correction (the sensor seems to heat up over time leading to wrong readings)
- [x] develop DayCounter tab (missing logic, expand on UI)
  - [ ] implement better colors
- [x] develop Word of the Day tab
  - [x] optimize to use API instead of page scraping

## Crazy stuff:
- [ ] handle phone calls
- [ ] Spotify controller

# Credits:
Credits for the *original version* of the device and the software and for the Diagram image: **Huy Vector DIY** and his [smart_cyber_clock project](https://sites.google.com/view/huy-materials-used/smart-cyber-clock)
