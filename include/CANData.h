#ifndef CAN_DATA_H
#define CAN_DATA_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "SDOManager.h"

// CAN Parameter structure
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

// Result of fetchParamsFromVCU
enum class FetchResult {
    SUCCESS,
    TIMEOUT,
    PARSE_ERROR,
    CAN_ERROR
};

// CAN Data Manager
class CANDataManager {
public:
    CANDataManager();

    bool init();       // Initializes TWAI only — call before fetchParamsFromVCU()
    bool initSDO();    // Starts SDOManager task — call after fetchParamsFromVCU()
    void update();

    // Fetch parameter schema directly from VCU via SDO segmented transfer.
    // Saves result to SPIFFS as params.json and loads into parameters[].
    // Call after init() but before sdoManager starts — runs synchronously.
    // Returns SUCCESS if download and parse succeeded.
    FetchResult fetchParamsFromVCU();

    // Parameter management
    bool loadParametersFromJSON(const char* jsonString);
    CANParameter* getParameter(uint16_t id);
    CANParameter* getParameterByName(const char* name);
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

    // BMS cell voltage access
    uint16_t getCellVoltage(uint8_t cellIndex);
    uint8_t getCellCount() { return bmsCellCount; }
    uint32_t getCellLastUpdate(uint8_t cellIndex);

    // SDO statistics
    uint32_t getSDOSuccessCount()  { return sdoManager.getSuccessCount(); }
    uint32_t getSDOFailureCount()  { return sdoManager.getFailureCount(); }
    uint32_t getSDOTimeoutCount()  { return sdoManager.getTimeoutCount(); }

private:
    CANParameter parameters[MAX_PARAMETERS];
    uint16_t parameterCount;

    SDOManager sdoManager;

    static void onSDOResult(const SDOResult& result);
    static CANDataManager* instance;

    CANMessage txQueue[TX_QUEUE_SIZE];
    CANMessage rxQueue[RX_QUEUE_SIZE];
    uint8_t txHead, txTail;
    uint8_t rxHead, rxTail;

    bool connected;
    uint32_t lastMessageTime;

    static const uint8_t MAX_BMS_CELLS = 96;
    uint16_t bmsCellVoltages[MAX_BMS_CELLS];
    uint32_t bmsCellUpdateTimes[MAX_BMS_CELLS];
    uint8_t bmsCellCount;

    // Low-level SDO helpers for segmented download (bypass SDOManager)
    bool sdoSendRaw(uint8_t* data8);
    bool sdoReceiveRaw(twai_message_t* frame, uint32_t timeoutMs = 500);

    void processReceivedMessage(CANMessage& msg);
    void handleSDOResponse(CANMessage& msg);
    void handlePDOMessage(CANMessage& msg);
    void handleGenericMessage(CANMessage& msg);
    void handleBMSCellVoltage(CANMessage& msg);

    void updateParameterBySDOId(uint16_t sdoId, int32_t value);
    void updateParameterIfExists(uint16_t paramId, int32_t value);

    bool enqueueTx(CANMessage& msg);
    bool dequeueTx(CANMessage& msg);
    bool enqueueRx(CANMessage& msg);
    bool dequeueRx(CANMessage& msg);
};

#endif // CAN_DATA_H