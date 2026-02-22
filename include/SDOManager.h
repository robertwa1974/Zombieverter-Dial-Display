#ifndef SDO_MANAGER_H
#define SDO_MANAGER_H

#include <Arduino.h>
#include "driver/twai.h"

// ZombieVerter SDO Configuration
#define SDO_TX_ID 0x603  // M5Dial → ZombieVerter
#define SDO_RX_ID 0x583  // ZombieVerter → M5Dial
#define SDO_TIMEOUT_MS 100
#define SDO_MAX_RETRIES 3

// SDO Command Codes (byte 0)
#define SDO_CMD_READ    0x40  // Upload (read from device)
#define SDO_CMD_WRITE   0x23  // Download (write to device)
#define SDO_CMD_ABORT   0x80  // Abort transfer

// SDO Response Codes (byte 0)
#define SDO_RESP_READ   0x43  // Upload response (4 bytes)
#define SDO_RESP_WRITE  0x60  // Download confirmation
#define SDO_RESP_ABORT  0x80  // Abort code

// ZombieVerter Fixed Bytes (Index 0x2100)
#define SDO_FIXED_BYTE1 0x00  // Index low byte (0x2100 & 0xFF)
#define SDO_FIXED_BYTE2 0x21  // Index high byte (0x2100 >> 8)

// SDO Abort Codes
#define SDO_ABORT_TOGGLE        0x05030000
#define SDO_ABORT_TIMEOUT       0x05040000
#define SDO_ABORT_CMD_INVALID   0x05040001
#define SDO_ABORT_PARAM_INVALID 0x06090011
#define SDO_ABORT_PARAM_RANGE   0x06090030
#define SDO_ABORT_GENERAL       0x08000000

class SDOManager {
public:
    SDOManager();
    
    // Initialize SDO (call after CAN is initialized)
    bool init();
    
    // Read parameter from ZombieVerter
    bool readParameter(uint8_t paramId, int32_t& value);
    
    // Write parameter to ZombieVerter  
    bool writeParameter(uint8_t paramId, int32_t value);
    
    // Save all parameters to flash
    bool saveToFlash();
    
    // Process incoming SDO responses
    void processResponse(const twai_message_t& msg);
    
    // Get last error information
    uint32_t getLastAbortCode() { return lastAbortCode; }
    const char* getLastError() { return lastError; }
    
    // Statistics
    uint32_t getSuccessCount() { return successCount; }
    uint32_t getFailureCount() { return failureCount; }
    uint32_t getTimeoutCount() { return timeoutCount; }
    
private:
    // Response state
    bool responseReceived;
    bool responseSuccess;
    int32_t responseValue;
    uint32_t lastAbortCode;
    char lastError[64];
    
    // Statistics
    uint32_t successCount;
    uint32_t failureCount;
    uint32_t timeoutCount;
    
    // Helper functions
    bool sendSDORequest(uint8_t cmd, uint8_t paramId, int32_t value = 0);
    bool waitForResponse(uint32_t timeoutMs);
    void clearResponse();
    const char* getAbortCodeDescription(uint32_t abortCode);
};

#endif // SDO_MANAGER_H
