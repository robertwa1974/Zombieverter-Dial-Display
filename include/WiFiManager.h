#pragma once
// =============================================================================
// WiFiManager.h — ZombieVerter Dial Display
//
// API surface:
//   GET  /cmd?cmd=json          → parameter + spot value list as JSON
//   GET  /cmd?cmd=get <names>   → spot value(s), one per line "name=value\n"
//   GET  /cmd?cmd=set <n> <v>   → write parameter via SDO, returns "1\n"
//   GET  /cmd?cmd=save          → save to VCU flash, returns "1\n"
//   GET  /cmd?cmd=load          → no-op, returns "1\n"
//   GET  /wifi                  → WiFi settings page
//   POST /wifi                  → update AP or STA credentials
//   GET  /list                  → SPIFFS file list JSON
//   POST /edit                  → upload file to SPIFFS
//   DELETE /edit?f=<path>       → delete file from SPIFFS
//   GET  /version               → firmware version string
//   POST /update                → OTA firmware update
//   GET  /spot                  → live parameter values as compact JSON
//   WS   /ws/can                → WebSocket CAN frame stream
//   POST /can/transmit          → transmit a CAN frame
//   POST /can/log/start         → start CSV logging to /canlog.csv
//   POST /can/log/stop          → stop CSV logging
//   GET  /can/log/download      → download canlog.csv
//   GET  /can/stats             → per-ID frame statistics JSON
//
// Web assets (index.html, can.html, inverter.js etc.) served from SPIFFS.
// =============================================================================

#include <Arduino.h>
#include "driver/twai.h"

class CANDataManager;
class AsyncWebServerRequest;

class WiFiManager {
public:
    WiFiManager();

    bool init(CANDataManager* canManager);
    void update();

    void startAP();
    void stopAP();

    String getIPAddress();
    bool isActive() const { return active; }
    bool isRefetchRequested() const { return refetchRequested; }
    void clearRefetchRequest() { refetchRequested = false; }

    void handleCmd(AsyncWebServerRequest* request);
    void handleSpot(AsyncWebServerRequest* request);
    void handleValue(AsyncWebServerRequest* request);
    void handleTripLog(AsyncWebServerRequest* request);
    void handleTripLogDelete(AsyncWebServerRequest* request);
    void handleRefetch(AsyncWebServerRequest* request);
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

    bool serveFile(AsyncWebServerRequest* request, const String& overridePath = "");

private:
    CANDataManager* can;
    bool active;
    bool serverStarted;
    bool refetchRequested = false;

    void startServer();
    void stopServer();

    String cmdJson();
    String cmdGet(const String& names);
    String cmdGetRepeated(const String& names, int repeat);
    String cmdSet(const String& name, const String& value);

    String getContentType(const String& filename);
};
