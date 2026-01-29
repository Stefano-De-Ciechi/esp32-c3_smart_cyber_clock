#ifndef WIFI_MANAGER
#define WIFI_MANAGER

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "secrets.h"

#define MAX_NETWORKS 4
#define CONFIG_TIMEOUT 120
#define RECONNECT_ATTEMPTS 10

extern const char WIFI_CONFIG_PAGE[] PROGMEM;

class AsyncWiFiManagerSimple {
public:
    void Setup();
    void Setup(const char* ap_ssid, const char* ap_pass);
    void loop();

private:
    Preferences preferences;
    AsyncWebServer server{80};
    DNSServer dnsServer;

    const char* apSSID = AP_SSID;
    const char* apPASS = AP_PASSWORD;

    unsigned long configStartTime = 0;
    bool inConfigMode = false;
    int reconnectAttempts = 0;

    struct WiFiNetwork {
        String ssid;
        String pass;
    };

    WiFiNetwork savedNetworks[MAX_NETWORKS];
    int networkCount = 0;

    void loadSavedNetworks();
    bool connectToSavedNetwork();
    void startConfigMode();
    void handleSave(AsyncWebServerRequest *request);
    void handleDelete(AsyncWebServerRequest *request);
    void checkWiFiConnection();
};

#endif