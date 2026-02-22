#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <WebServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "CANData.h"

/**
 * WebInterface - OpenInverter-compatible web API for M5Dial
 * 
 * Provides REST API endpoints compatible with jamiejones85's esp32-web-interface
 * Allows any OpenInverter web UI to control ZombieVerter via M5Dial
 * 
 * Features:
 * - JSON parameter dump (compatible with existing UIs)
 * - Get/Set parameters via SDO
 * - WebSocket for real-time updates
 * - Access Point mode (ESP-M5DIAL)
 * - Station mode (connect to existing WiFi)
 * - mDNS (zombieverter.local)
 */

class WebInterface {
public:
    WebInterface(CANDataManager* can);
    
    // Lifecycle
    bool init();
    void update();
    
    // WiFi Management
    void startAccessPoint(const char* ssid = "ESP-M5DIAL", const char* password = "");
    void connectToWiFi(const char* ssid, const char* password);
    String getIPAddress();
    bool isConnected();
    
    // Configuration
    void setHostname(const char* hostname = "zombieverter");
    void enableCORS(bool enable = true);
    
private:
    CANDataManager* canManager;
    WebServer server;
    bool apMode;
    bool corsEnabled;
    
    // CAN Message Logging
    struct CANLogMessage {
        uint32_t id;
        uint8_t data[8];
        uint8_t len;
        uint32_t timestamp;
        bool isRx;
    };
    static const int CAN_LOG_SIZE = 100;
    CANLogMessage canLogBuffer[CAN_LOG_SIZE];
    int canLogIndex;
    bool canLoggingEnabled;
    
    // HTTP Handlers
    void handleRoot();
    void handleJSON();
    void handleCmd();  // jamiejones85 compatibility
    void handleGet();
    void handleSet();
    void handleSave();
    void handleLoad();
    void handleSpot();
    void handleCanSend();
    void handleCanLog();
    void handleParamsUpload();
    void handleNotFound();
    void handleCORS();
    
    // Helper Functions
    String buildJSONResponse(bool includeHidden = false);
    String queryAllParametersFromZombieVerter();
    bool setParameterValue(int paramId, int32_t value);
    int32_t getParameterValue(int paramId);
    void logCanMessage(uint32_t id, uint8_t* data, uint8_t len, bool isRx);
    bool usesRawValue(const char* paramName);  // Check if parameter needs fixed-point encoding
    
    // CORS headers
    void addCORSHeaders();
};

#endif // WEB_INTERFACE_H
