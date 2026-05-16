#ifndef CAN_DATA_H
#define CAN_DATA_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
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

    // SDO manager access — used by Immobilizer to queue high-priority writes
    SDOManager* getSDOManager() { return &sdoManager; }

    // Public SDO result dispatcher — called from main's onSDOResult() after
    // the immobilizer has claimed param 156 / spot 2124.
    // Forwards to the private updateParameterBySDOId() logic.
    void onSDOResult(const SDOResult& result);

    // -----------------------------------------------------------------------
    // Optional raw frame observer — called for every received CAN frame
    // before processing. Used by GVRETServer to stream frames to SavvyCAN.
    // Register once in setup(); zero overhead when no callback is set.
    // -----------------------------------------------------------------------
    using FrameObserver = std::function<void(uint32_t id, bool extd, const uint8_t* data, uint8_t dlc)>;
    void setFrameObserver(FrameObserver obs) { _frameObserver = obs; }

private:
    CANParameter parameters[MAX_PARAMETERS];
    uint16_t parameterCount;

    SDOManager sdoManager;

    static void sdoResultCallback(const SDOResult& result);
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

    FrameObserver _frameObserver;  // GVRET bridge callback

    // Low-level SDO helpers for segmented download (bypass SDOManager)
    bool sdoSendRaw(uint8_t* data8);
    bool sdoReceiveRaw(twai_message_t* frame, uint32_t timeoutMs = 500);
    FetchResult fetchParamsAttempt();

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
