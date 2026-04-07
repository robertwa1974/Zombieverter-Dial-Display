#pragma once
// =============================================================================
// CANMonitor.h — ZombieVerter Dial Display
//
// Real-time CAN bus monitor with WebSocket streaming, CSV logging,
// filtering, and frame transmission.
//
// Usage:
//   1. CANMonitor::instance().init(canDataManager)  — call once in setup()
//   2. CANMonitor::instance().pushFrame(frame)       — call for every CAN frame
//   3. CANMonitor::instance().registerEndpoints(server) — registers WS + HTTP
// =============================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "driver/twai.h"

// Forward declaration
class CANDataManager;

// ---- Captured frame ----
struct CANFrame {
    uint32_t timestamp;   // millis()
    uint32_t id;
    uint8_t  len;
    uint8_t  data[8];
};

// ---- Per-ID statistics ----
struct CANIDStat {
    uint32_t id;
    uint32_t count;
    uint32_t lastSeen;    // millis()
};

// ---- Logging state ----
enum class LogState { IDLE, LOGGING, STOPPED };

// =============================================================================

class CANMonitor {
public:
    static CANMonitor& instance() {
        static CANMonitor inst;
        return inst;
    }

    // Call once during setup — AFTER WiFiManager and CANDataManager are ready
    void init(CANDataManager* canMgr);

    // Register WebSocket + HTTP endpoints on the async server
    void registerEndpoints(AsyncWebServer* server);

    // Called by CANData::update() for every received frame
    void pushFrame(const twai_message_t& msg);

    // Called by CANData for transmit requests from web UI
    bool transmitFrame(uint32_t id, uint8_t* data, uint8_t len);

    // Monitoring active only when browser client connected
    bool isActive() const { return clientCount > 0 || logState == LogState::LOGGING; }

    // Logging controls
    void startLogging();
    void stopLogging();
    LogState getLogState() const { return logState; }
    uint32_t getLogFrameCount() const { return logFrameCount; }

private:
    CANMonitor() = default;

    // ---- Circular frame buffer ----
    static const uint16_t BUF_SIZE = 200;
    CANFrame frameBuf[BUF_SIZE];
    uint16_t bufHead = 0;   // next write position
    uint16_t bufCount = 0;
    SemaphoreHandle_t bufMutex = nullptr;

    // ---- Per-ID statistics ----
    static const uint8_t MAX_ID_STATS = 64;
    CANIDStat idStats[MAX_ID_STATS];
    uint8_t idStatCount = 0;
    void updateIDStat(uint32_t id);

    // ---- WebSocket ----
    AsyncWebSocket* ws = nullptr;
    volatile uint8_t clientCount = 0;

    // Thread-safe WS send queue — pushFrame (main loop) enqueues,
    // a periodic flush in pushFrame drains it via ws->textAll which
    // is safe to call from the main loop only.
    static const uint8_t WS_QUEUE_SIZE = 16;
    struct WsMsg { char json[300]; };
    WsMsg wsQueue[WS_QUEUE_SIZE];
    uint8_t wsQHead = 0;
    uint8_t wsQTail = 0;
    SemaphoreHandle_t wsQMutex = nullptr;
    void enqueueWsMsg(const String& json);
    void flushWsQueue();  // call from main loop only

    // Throttle: only push a given ID at most once per 50ms
    static const uint8_t THROTTLE_SLOTS = 32;
    struct ThrottleEntry { uint32_t id; uint32_t lastSentMs; };
    ThrottleEntry throttle[THROTTLE_SLOTS];
    bool shouldThrottle(uint32_t id);
    void markSent(uint32_t id);

    // ---- Logging ----
    LogState logState = LogState::IDLE;
    File logFile;
    uint32_t logFrameCount = 0;
    uint32_t sessionFrameCount = 0;
    uint32_t sessionStartMs = 0;

    void writeCSVRow(const CANFrame& f, const String& decoded);

    // ---- Decoder ----
    CANDataManager* canMgr = nullptr;
    String decodeFrame(const CANFrame& f);

    // ---- Frame → JSON string ----
    String frameToJson(const CANFrame& f, const String& decoded,
                       uint32_t countForID);

    // ---- WebSocket event handler (static trampoline) ----
    static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);
    void handleWsMessage(AsyncWebSocketClient* client,
                         uint8_t* data, size_t len);

    // ---- HTTP handlers ----
    void handleTransmit(AsyncWebServerRequest* request);
    void handleLogStart(AsyncWebServerRequest* request);
    void handleLogStop(AsyncWebServerRequest* request);
    void handleLogDownload(AsyncWebServerRequest* request);
    void handleStats(AsyncWebServerRequest* request);

    // Friendly name lookup for known CAN IDs
    static const char* knownIDName(uint32_t id);
};
