#include "SDOManager.h"

SDOManager::SDOManager() {
    responseReceived = false;
    responseSuccess = false;
    responseValue = 0;
    lastAbortCode = 0;
    memset(lastError, 0, sizeof(lastError));
    successCount = 0;
    failureCount = 0;
    timeoutCount = 0;
}

bool SDOManager::init() {
    Serial.println("[SDO] Initialized with ZombieVerter custom format");
    Serial.println("[SDO] TX: 0x603, RX: 0x583");
    Serial.println("[SDO] Format: [cmd, 0x01, 0x20, param_id, data...]");
    clearResponse();
    return true;
}

bool SDOManager::readParameter(uint8_t paramId, int32_t& value) {
    Serial.printf("[SDO] Reading param %d... ", paramId);
    
    for (int retry = 0; retry < SDO_MAX_RETRIES; retry++) {
        if (retry > 0) {
            Serial.printf("(retry %d) ", retry);
        }
        
        clearResponse();
        
        // Send SDO read request with ZombieVerter format
        if (!sendSDORequest(SDO_CMD_READ, paramId, 0)) {
            Serial.println("FAILED to send");
            continue;
        }
        
        // Wait for response
        if (waitForResponse(SDO_TIMEOUT_MS)) {
            if (responseSuccess) {
                value = responseValue;
                Serial.printf("✓ Got %d (0x%08X)\n", value, value);
                successCount++;
                return true;
            } else {
                Serial.printf("✗ Abort: %s\n", lastError);
                failureCount++;
                return false;
            }
        } else {
            Serial.printf("✗ Timeout ");
            timeoutCount++;
        }
    }
    
    Serial.println("FAILED after retries");
    strcpy(lastError, "Timeout after retries");
    failureCount++;
    return false;
}

bool SDOManager::writeParameter(uint8_t paramId, int32_t value) {
    Serial.printf("[SDO] Writing param %d = %d (0x%08X)... ", paramId, value, value);
    
    for (int retry = 0; retry < SDO_MAX_RETRIES; retry++) {
        if (retry > 0) {
            Serial.printf("(retry %d) ", retry);
        }
        
        clearResponse();
        
        // Send SDO write request with ZombieVerter format
        if (!sendSDORequest(SDO_CMD_WRITE, paramId, value)) {
            Serial.println("FAILED to send");
            continue;
        }
        
        // Wait for response
        if (waitForResponse(SDO_TIMEOUT_MS)) {
            if (responseSuccess) {
                Serial.println("✓ Success");
                successCount++;
                return true;
            } else {
                Serial.printf("✗ Abort: %s\n", lastError);
                failureCount++;
                return false;
            }
        } else {
            Serial.printf("✗ Timeout ");
            timeoutCount++;
        }
    }
    
    Serial.println("FAILED after retries");
    strcpy(lastError, "Timeout after retries");
    failureCount++;
    return false;
}

bool SDOManager::saveToFlash() {
    Serial.println("[SDO] ========================================");
    Serial.println("[SDO] Saving parameters to flash...");
    Serial.println("[SDO] ========================================");
    
    // Special command: param 0 with value 6 = save to flash
    bool success = writeParameter(0, 6);
    
    if (success) {
        Serial.println("[SDO] ✓ Parameters saved to flash!");
    } else {
        Serial.println("[SDO] ✗ Failed to save to flash");
    }
    
    Serial.println("[SDO] ========================================");
    return success;
}

void SDOManager::processResponse(const twai_message_t& msg) {
    // Only process SDO responses
    if (msg.identifier != SDO_RX_ID) {
        return;
    }
    
    if (msg.data_length_code < 4) {
        return; // Invalid SDO message
    }
    
    uint8_t cmd = msg.data[0];
    uint8_t byte1 = msg.data[1];
    uint8_t byte2 = msg.data[2];
    uint8_t paramId = msg.data[3];
    
    // Verify fixed bytes
    if (byte1 != SDO_FIXED_BYTE1 || byte2 != SDO_FIXED_BYTE2) {
        Serial.printf("[SDO] Warning: Unexpected format [%02X %02X %02X %02X...]\n",
                     cmd, byte1, byte2, paramId);
        // Continue processing anyway
    }
    
    switch (cmd) {
        case SDO_RESP_READ: {
            // Read response: extract 32-bit value
            responseValue = msg.data[4] | 
                          (msg.data[5] << 8) | 
                          (msg.data[6] << 16) | 
                          (msg.data[7] << 24);
            responseSuccess = true;
            responseReceived = true;
            
            Serial.printf("[SDO] RX Read OK: param %d = %d (0x%08X)\n", 
                         paramId, responseValue, responseValue);
            break;
        }
        
        case SDO_RESP_WRITE: {
            // Write confirmation
            responseSuccess = true;
            responseReceived = true;
            
            Serial.printf("[SDO] RX Write OK: param %d confirmed\n", paramId);
            break;
        }
        
        case SDO_RESP_ABORT: {
            // Abort response: extract 32-bit abort code
            lastAbortCode = msg.data[4] | 
                          (msg.data[5] << 8) | 
                          (msg.data[6] << 16) | 
                          (msg.data[7] << 24);
            
            const char* desc = getAbortCodeDescription(lastAbortCode);
            snprintf(lastError, sizeof(lastError), "Abort 0x%08X: %s", 
                    lastAbortCode, desc);
            
            responseSuccess = false;
            responseReceived = true;
            
            Serial.printf("[SDO] RX Abort: param %d, code 0x%08X (%s)\n", 
                         paramId, lastAbortCode, desc);
            break;
        }
        
        default:
            Serial.printf("[SDO] RX Unknown command: 0x%02X\n", cmd);
            break;
    }
}

bool SDOManager::sendSDORequest(uint8_t cmd, uint8_t paramId, int32_t value) {
    twai_message_t txMsg;
    txMsg.identifier = SDO_TX_ID;
    txMsg.data_length_code = 8;
    txMsg.extd = 0;
    txMsg.rtr = 0;
    txMsg.ss = 0;
    txMsg.self = 0;
    txMsg.dlc_non_comp = 0;
    
    // ZombieVerter custom SDO format
    txMsg.data[0] = cmd;                    // Command (0x40=read, 0x23=write)
    txMsg.data[1] = SDO_FIXED_BYTE1;        // Fixed: 0x01
    txMsg.data[2] = SDO_FIXED_BYTE2;        // Fixed: 0x20
    txMsg.data[3] = paramId;                // Parameter ID (NOT split!)
    txMsg.data[4] = value & 0xFF;           // Value byte 0 (LSB)
    txMsg.data[5] = (value >> 8) & 0xFF;    // Value byte 1
    txMsg.data[6] = (value >> 16) & 0xFF;   // Value byte 2
    txMsg.data[7] = (value >> 24) & 0xFF;   // Value byte 3 (MSB)
    
    // Send CAN message
    esp_err_t result = twai_transmit(&txMsg, pdMS_TO_TICKS(10));
    
    if (result == ESP_OK) {
        Serial.printf("[SDO] TX [%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                     txMsg.data[0], txMsg.data[1], txMsg.data[2], txMsg.data[3],
                     txMsg.data[4], txMsg.data[5], txMsg.data[6], txMsg.data[7]);
        return true;
    } else {
        Serial.printf("[SDO] TX FAILED: %s\n", esp_err_to_name(result));
        return false;
    }
}

bool SDOManager::waitForResponse(uint32_t timeoutMs) {
    uint32_t startTime = millis();
    int messageCount = 0;
    
    // Loop through messages like jamiejones does
    while (millis() - startTime < timeoutMs && messageCount < 100) {
        twai_message_t rxMsg;
        
        // Try to receive a message (short timeout)
        if (twai_receive(&rxMsg, pdMS_TO_TICKS(10)) == ESP_OK) {
            messageCount++;
            
            // Check if it's an SDO response (0x583)
            if (rxMsg.identifier == SDO_RX_ID) {
                // Process this SDO response
                processResponse(rxMsg);
                
                // If we got a response (success or abort), we're done
                if (responseReceived) {
                    return true;
                }
            }
            // Otherwise it's a broadcast message, ignore and keep looking
        }
    }
    
    return false;
}

void SDOManager::clearResponse() {
    responseReceived = false;
    responseSuccess = false;
    responseValue = 0;
}

const char* SDOManager::getAbortCodeDescription(uint32_t abortCode) {
    switch (abortCode) {
        case SDO_ABORT_TOGGLE:
            return "Toggle bit not alternated";
        case SDO_ABORT_TIMEOUT:
            return "SDO protocol timed out";
        case SDO_ABORT_CMD_INVALID:
            return "Invalid or unknown command";
        case SDO_ABORT_PARAM_INVALID:
            return "Object does not exist";
        case SDO_ABORT_PARAM_RANGE:
            return "Value out of range";
        case SDO_ABORT_GENERAL:
            return "General error";
        default:
            return "Unknown abort code";
    }
}
