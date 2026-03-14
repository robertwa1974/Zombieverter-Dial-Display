#pragma once
// =============================================================================
// WiFiManager.h — ZombieVerter Dial Display
//
// Serves the openinverter/jamiejones85 web interface (data/ folder assets)
// from SPIFFS and implements the /cmd API endpoint so the existing HTML/JS
// pages work without modification.
//
// API surface implemented:
//   GET  /cmd?cmd=json          → parameter + spot value list as JSON
//   GET  /cmd?cmd=get <names>   → spot value(s), one per line "name=value\n"
//   GET  /cmd?cmd=set <n> <v>   → write parameter via SDO, returns "1\n"
//   GET  /cmd?cmd=save          → no-op (values always live), returns "1\n"
//   GET  /cmd?cmd=load          → no-op, returns "1\n"
//   GET  /wifi                  → WiFi settings page (GET=show, POST=update)
//   POST /wifi                  → update AP or STA credentials
//   GET  /list                  → SPIFFS file list JSON
//   POST /edit                  → upload file to SPIFFS
//   DELETE /edit?f=<path>       → delete file from SPIFFS
//   GET  /version               → firmware version string
//   POST /update                → OTA firmware update (ESP32 HTTP update)
//
// The web assets (index.html, ui.js, inverter.js, style.css etc.) are served
// directly from SPIFFS by the catch-all onNotFound handler.
// Uses ESPAsyncWebServer for concurrent non-blocking file serving.
// =============================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Forward declaration — avoid including the full CANData.h here
class CANDataManager;

class WiFiManager {
public:
    WiFiManager();

    // Call once in setup() — mounts SPIFFS, prepares server
    bool init(CANDataManager* canManager);

    // Call in loop() — no-op with async server, kept for compatibility
    void update();

    // Toggle AP on/off from button press
    void startAP();
    void stopAP();

    // Returns current IP as string (AP or STA)
    String getIPAddress();

    bool isActive() const { return active; }

    // Public so lambdas in startServer() can call them
    void handleCmd(AsyncWebServerRequest* request);
    void handleWifiGet(AsyncWebServerRequest* request);
    void handleWifiPost(AsyncWebServerRequest* request);
    void handleFileList(AsyncWebServerRequest* request);
    void handleFileDelete(AsyncWebServerRequest* request);
    void handleFileUpload(AsyncWebServerRequest* request,
                          const String& filename,
                          size_t index, uint8_t* data,
                          size_t len, bool final);
    void handleOTA(AsyncWebServerRequest* request,
                   const String& filename,
                   size_t index, uint8_t* data,
                   size_t len, bool final);

    // Returns false if file not found (caller sends 404)
    // overridePath: optional path override (used by handleWifiPost redirect)
    bool serveFile(AsyncWebServerRequest* request, const String& overridePath = "");

private:
    CANDataManager* can;
    bool active;
    bool serverStarted;

    void startServer();
    void stopServer();

    // Command dispatch
    String cmdJson();
    String cmdGet(const String& names);
    String cmdSet(const String& name, const String& value);

    // Helpers
    String getContentType(const String& filename);
};