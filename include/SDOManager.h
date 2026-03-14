#ifndef SDO_MANAGER_H
#define SDO_MANAGER_H

#include <Arduino.h>
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// ============================================================================
// ZombieVerter SDO Configuration
// ============================================================================
#define SDO_TX_ID           0x603   // M5Dial → ZombieVerter
#define SDO_RX_ID           0x583   // ZombieVerter → M5Dial
#define SDO_TIMEOUT_MS      500     // Per-transaction timeout
#define SDO_MAX_RETRIES     2       // Retries before giving up
#define SDO_MIN_GAP_MS      10      // Minimum ms between transactions
#define SDO_QUEUE_DEPTH     32      // Max pending requests

// SDO Command Codes (byte 0)
#define SDO_CMD_READ        0x40
#define SDO_CMD_WRITE       0x23

// SDO Response Codes (byte 0)
#define SDO_RESP_READ_4B    0x43    // 4-byte expedited upload response
#define SDO_RESP_READ_2B    0x4B    // 2-byte expedited upload response
#define SDO_RESP_WRITE      0x60    // Download confirmation
#define SDO_RESP_ABORT      0x80    // Abort

// ZombieVerter Index 0x2100 bytes
#define SDO_INDEX_LO        0x00    // 0x2100 & 0xFF
#define SDO_INDEX_HI        0x21    // 0x2100 >> 8

// SDO Abort Codes
#define SDO_ABORT_TOGGLE        0x05030000
#define SDO_ABORT_TIMEOUT       0x05040000
#define SDO_ABORT_CMD_INVALID   0x05040001
#define SDO_ABORT_PARAM_INVALID 0x06090011
#define SDO_ABORT_PARAM_RANGE   0x06090030
#define SDO_ABORT_GENERAL       0x08000000

// ============================================================================
// Request / result types
// ============================================================================

enum SDORequestType : uint8_t {
    SDO_REQ_READ = 0,
    SDO_REQ_WRITE,
    SDO_REQ_SAVE_FLASH
};

struct SDORequest {
    SDORequestType  type;
    uint8_t         paramId;
    int32_t         value;          // Used for writes
    bool            highPriority;   // Jumps ahead of bulk reads
};

struct SDOResult {
    uint8_t     paramId;
    int32_t     value;              // Valid on successful read
    bool        success;
    bool        isWrite;            // true = write confirmed, false = read result
    uint32_t    abortCode;
};

// Callback invoked from the SDO task when a transaction completes
typedef void (*SDOResultCallback)(const SDOResult& result);

// ============================================================================
// State machine states
// ============================================================================
enum SDOState : uint8_t {
    SDO_IDLE = 0,
    SDO_SEND_REQUEST,
    SDO_WAIT_RESPONSE,
    SDO_PROCESS_RESPONSE,
    SDO_RETRY,
    SDO_FAIL
};

// ============================================================================
// SDOManager
// ============================================================================
class SDOManager {
public:
    SDOManager();

    // Call after TWAI is initialised. Starts the background task on core 0.
    bool init(SDOResultCallback callback = nullptr);

    // Queue a read request. Returns false if queue is full.
    bool requestRead(uint8_t paramId, bool highPriority = false);

    // Queue a write request. Returns false if queue is full.
    bool requestWrite(uint8_t paramId, int32_t value, bool highPriority = false);

    // Queue a save-to-flash command.
    bool requestSaveFlash();

    // Feed incoming TWAI frames here (call from your CAN receive path).
    void processIncomingFrame(const twai_message_t& msg);

    // Statistics
    uint32_t getSuccessCount()  { return successCount; }
    uint32_t getFailureCount()  { return failureCount; }
    uint32_t getTimeoutCount()  { return timeoutCount; }
    uint16_t getQueueDepth();

    // Replace callback after init if needed
    void setResultCallback(SDOResultCallback cb) { resultCallback = cb; }

private:
    // FreeRTOS handles
    TaskHandle_t    taskHandle;
    QueueHandle_t   requestQueue;
    QueueHandle_t   rxFrameQueue;   // Incoming SDO frames fed from CAN task
    SemaphoreHandle_t statsMutex;

    // State machine
    SDOState        state;
    SDORequest      currentRequest;
    uint32_t        stateEnteredMs;
    uint8_t         retryCount;
    uint32_t        lastTransactionMs;

    // Callback
    SDOResultCallback resultCallback;

    // Statistics (protected by statsMutex)
    uint32_t successCount;
    uint32_t failureCount;
    uint32_t timeoutCount;

    // Internal helpers
    void        taskLoop();                 // Runs inside FreeRTOS task
    bool        sendFrame(uint8_t cmd, uint8_t paramId, int32_t value = 0);
    void        handleFrame(const twai_message_t& msg);
    void        deliverResult(bool success, uint8_t paramId,
                              int32_t value, bool isWrite,
                              uint32_t abortCode = 0);
    const char* abortDescription(uint32_t code);

    static void sdoTaskEntry(void* arg);
};

#endif // SDO_MANAGER_H