#ifndef CAN_DATA_H
#define CAN_DATA_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "SDOManager.h"

// CAN Parameter structure — minimal for RAM efficiency
// Metadata (unit, category, min, max, default) is served from params.json directly
struct CANParameter {
    uint16_t id;
    char name[32];
    bool editable;
    bool fromBroadcast;  // true = value comes from CAN broadcast, skip SDO polling

    int32_t valueInt;
    uint32_t lastUpdateTime;

    void setValue(int32_t val) { valueInt = val; lastUpdateTime = millis(); }
    int32_t getValueAsInt() { return valueInt; }
};

// CAN Message structure
struct CANMessage {
    uint32_t id;
    uint8_t data[8];
    uint8_t length;
    uint32_t timestamp;
};

// CAN Data Manager
class CANDataManager {
public:
    CANDataManager();
    
    bool init();
    void update();
    
    // Parameter management
    bool loadParametersFromJSON(const char* jsonString);
    CANParameter* getParameter(uint16_t id);          // lookup by VCU id
    CANParameter* getParameterByName(const char* name); // lookup by name
    CANParameter* getParameterByIndex(uint8_t index);
    uint16_t getParameterCount() { return parameterCount; }
    
    // CAN communication
    void requestParameter(uint16_t paramId);
    void setParameter(uint16_t paramId, int32_t value);
    bool sendMessage(uint32_t id, uint8_t* data, uint8_t length);
    
    // Save all parameters to VCU flash
    void saveToFlash() { sdoManager.requestSaveFlash(); }
    
    // Connection status
    bool isConnected() { return connected; }
    uint32_t getLastMessageTime() { return lastMessageTime; }

    // Build the params JSON cache in the main loop task (never call from async_tcp).
    // Reads params.json from SPIFFS, patches live values, stores in cachedParamsJson.
    // Call periodically from loop() — safe to call every 3 s.
    void buildParamsJsonCache();

    // Return a const reference to the pre-built JSON string.
    // Safe to read from async_tcp since writes use move-swap (atomic replace).
    const String& getParamsJsonCache() const { return cachedParamsJson; }

    // Legacy — kept for reference, NOT called from async_tcp anymore.
    // void streamParamsJson(AsyncResponseStream* stream);
    
    // BMS cell voltage access
    uint16_t getCellVoltage(uint8_t cellIndex);
    uint8_t getCellCount() { return bmsCellCount; }
    uint32_t getCellLastUpdate(uint8_t cellIndex);

    // SDO statistics (for settings screen)
    uint32_t getSDOSuccessCount()  { return sdoManager.getSuccessCount(); }
    uint32_t getSDOFailureCount()  { return sdoManager.getFailureCount(); }
    uint32_t getSDOTimeoutCount()  { return sdoManager.getTimeoutCount(); }

private:
    CANParameter parameters[MAX_PARAMETERS];
    uint16_t parameterCount;

    // Async SDO manager
    SDOManager sdoManager;

    // Static callback — SDOManager calls this when a read completes
    static void onSDOResult(const SDOResult& result);

    // Pointer to self so the static callback can reach instance data
    static CANDataManager* instance;

    CANMessage txQueue[TX_QUEUE_SIZE];
    CANMessage rxQueue[RX_QUEUE_SIZE];
    uint8_t txHead, txTail;
    uint8_t rxHead, rxTail;
    
    bool connected;
    uint32_t lastMessageTime;

    // Pre-built params JSON — rebuilt in the main loop, served from async handler
    String   cachedParamsJson;
    uint32_t lastJsonBuildMs;
    
    static const uint8_t MAX_BMS_CELLS = 96;
    uint16_t bmsCellVoltages[MAX_BMS_CELLS];
    uint32_t bmsCellUpdateTimes[MAX_BMS_CELLS];
    uint8_t bmsCellCount;
    
    void processReceivedMessage(CANMessage& msg);
    void handleSDOResponse(CANMessage& msg);
    void handlePDOMessage(CANMessage& msg);
    void handleGenericMessage(CANMessage& msg);
    void handleBMSCellVoltage(CANMessage& msg);

    // Update by SDO short id (1-999) or name mapping
    void updateParameterBySDOId(uint16_t sdoId, int32_t value);
    // Legacy alias
    void updateParameterIfExists(uint16_t paramId, int32_t value);
    
    bool enqueueTx(CANMessage& msg);
    bool dequeueTx(CANMessage& msg);
    bool enqueueRx(CANMessage& msg);
    bool dequeueRx(CANMessage& msg);
};

#endif // CAN_DATA_H