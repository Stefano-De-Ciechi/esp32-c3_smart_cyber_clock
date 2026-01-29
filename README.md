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

## arduino-cli
Alternatively, you can use [arduino-cli](https://docs.arduino.cc/arduino-cli/) to compile and upload this sketch; the file *sketch.yaml* provides the necessary configuration (it should automatically download all the dependencies)

Be sure to modify the *port* or *default_port* values (I used the default /dev/ttyACM0 for linux systems, change accordingly to your OS!)

Also, use the command:

```bash
arduino-cli board details --fqbn esp32:esp32:adafruit_qtpy_esp32c3
```
to verify that all the properties are correctly set (most importantly verify that option **USB CDC On Boot** is Enabled, or else you may be unable to read serial data coming from the device)

for reference, my command output is:

```bash
Board name:            Adafruit QT Py ESP32-C3
FQBN:                  esp32:esp32:adafruit_qtpy_esp32c3
Board version:         3.3.5

Package name:          esp32
Package maintainer:    Espressif Systems
Package URL:           https://downloads.arduino.cc/packages/package_index.tar.bz2
Package website:       https://github.com/espressif/arduino-esp32
Package online help:   https://esp32.com

Platform name:         esp32
Platform category:     ESP32
Platform architecture: esp32
Platform URL:          https://github.com/espressif/arduino-esp32/releases/download/3.3.5/esp32-3.3.5.zip
Platform file name:    esp32-3.3.5.zip
Platform size (bytes): 27465930
Platform checksum:     SHA-256:294e754f8eb7a2fd0bb38b08bfc8738e12b4655055ac4529d8c57ac22085642c

Required tool: arduino:dfu-util                                0.11.0-arduino5
Required tool: esp32:esp-rv32                                  2511
Required tool: esp32:esp-x32                                   2511
Required tool: esp32:esp32-arduino-libs                        idf-release_v5.5-9bb7aa84-v2
Required tool: esp32:esptool_py                                5.1.0
Required tool: esp32:mklittlefs                                4.0.2-db0513a
Required tool: esp32:mkspiffs                                  0.2.3
Required tool: esp32:openocd-esp32                             v0.12.0-esp32-20250707
Required tool: esp32:riscv32-esp-elf-gdb                       16.3_20250913
Required tool: esp32:xtensa-esp-elf-gdb                        16.3_20250913

Option:        Upload Speed                                                                 UploadSpeed
               921600                                          ✔                            UploadSpeed=921600
               115200                                                                       UploadSpeed=115200
               230400                                                                       UploadSpeed=230400
               460800                                                                       UploadSpeed=460800
Option:        USB CDC On Boot                                                              CDCOnBoot
               Enabled                                         ✔                            CDCOnBoot=cdc
               Disabled                                                                     CDCOnBoot=default
Option:        CPU Frequency                                                                CPUFreq
               160MHz (WiFi)                                   ✔                            CPUFreq=160
               80MHz (WiFi)                                                                 CPUFreq=80
               40MHz                                                                        CPUFreq=40
               20MHz                                                                        CPUFreq=20
               10MHz                                                                        CPUFreq=10
Option:        Flash Frequency                                                              FlashFreq
               80MHz                                           ✔                            FlashFreq=80
               40MHz                                                                        FlashFreq=40
Option:        Flash Mode                                                                   FlashMode
               QIO                                             ✔                            FlashMode=qio
               DIO                                                                          FlashMode=dio
Option:        Flash Size                                                                   FlashSize
               4MB (32Mb)                                      ✔                            FlashSize=4M
Option:        Partition Scheme                                                             PartitionScheme
               Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) ✔                            PartitionScheme=default
               Default 4MB with ffat (1.2MB APP/1.5MB FATFS)                                PartitionScheme=defaultffat
               Minimal (1.3MB APP/700KB SPIFFS)                                             PartitionScheme=minimal
               No OTA (2MB APP/2MB SPIFFS)                                                  PartitionScheme=no_ota
               No OTA (1MB APP/3MB SPIFFS)                                                  PartitionScheme=noota_3g
               No OTA (2MB APP/2MB FATFS)                                                   PartitionScheme=noota_ffat
               No OTA (1MB APP/3MB FATFS)                                                   PartitionScheme=noota_3gffat
               Huge APP (3MB No OTA/1MB SPIFFS)                                             PartitionScheme=huge_app
               Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)                              PartitionScheme=min_spiffs
Option:        Core Debug Level                                                             DebugLevel
               None                                            ✔                            DebugLevel=none
               Error                                                                        DebugLevel=error
               Warn                                                                         DebugLevel=warn
               Info                                                                         DebugLevel=info
               Debug                                                                        DebugLevel=debug
               Verbose                                                                      DebugLevel=verbose
Option:        Erase All Flash Before Sketch Upload                                         EraseFlash
               Disabled                                        ✔                            EraseFlash=none
               Enabled                                                                      EraseFlash=all
Option:        Zigbee Mode                                                                  ZigbeeMode
               Disabled                                        ✔                            ZigbeeMode=default
               Zigbee ZCZR (coordinator/router)                                             ZigbeeMode=zczr
Programmers:   ID                                              Name
               esptool                                         Esptool
```

# Possible Setup Errors:
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
