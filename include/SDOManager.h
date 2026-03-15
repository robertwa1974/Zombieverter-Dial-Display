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

// ZombieVerter base index 0x2100 — subindex and upper byte vary per param ID
// Index = 0x2100 | (paramId >> 8)
// Subindex = paramId & 0xFF
#define SDO_BASE_INDEX      0x2100

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
    uint16_t        paramId;    // Full 16-bit VCU param ID (supports 2000+ spot values)
    int32_t         value;      // Used for writes
    bool            highPriority;
};

struct SDOResult {
    uint16_t    paramId;        // Full 16-bit param ID
    int32_t     value;          // Valid on successful read
    bool        success;
    bool        isWrite;
    uint32_t    abortCode;
};

// Callback invoked when a transaction completes
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

    bool init(SDOResultCallback callback = nullptr);

    // Queue a read request — accepts full 16-bit param ID
    bool requestRead(uint16_t paramId, bool highPriority = false);

    // Queue a write request — accepts full 16-bit param ID
    bool requestWrite(uint16_t paramId, int32_t value, bool highPriority = false);

    bool requestSaveFlash();

    void processIncomingFrame(const twai_message_t& msg);

    uint32_t getSuccessCount()  { return successCount; }
    uint32_t getFailureCount()  { return failureCount; }
    uint32_t getTimeoutCount()  { return timeoutCount; }
    uint16_t getQueueDepth();

    void setResultCallback(SDOResultCallback cb) { resultCallback = cb; }

private:
    TaskHandle_t    taskHandle;
    QueueHandle_t   requestQueue;
    QueueHandle_t   rxFrameQueue;
    SemaphoreHandle_t statsMutex;

    SDOState        state;
    SDORequest      currentRequest;
    uint32_t        stateEnteredMs;
    uint8_t         retryCount;
    uint32_t        lastTransactionMs;

    SDOResultCallback resultCallback;

    uint32_t successCount;
    uint32_t failureCount;
    uint32_t timeoutCount;

    void        taskLoop();
    bool        sendFrame(uint8_t cmd, uint16_t paramId, int32_t value = 0);
    void        handleFrame(const twai_message_t& msg);
    void        deliverResult(bool success, uint16_t paramId,
                              int32_t value, bool isWrite,
                              uint32_t abortCode = 0);
    const char* abortDescription(uint32_t code);

    static void sdoTaskEntry(void* arg);
};

#endif // SDO_MANAGER_H