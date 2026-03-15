#include "SDOManager.h"

// ============================================================================
// Construction
// ============================================================================

SDOManager::SDOManager()
    : taskHandle(nullptr),
      requestQueue(nullptr),
      rxFrameQueue(nullptr),
      statsMutex(nullptr),
      state(SDO_IDLE),
      stateEnteredMs(0),
      retryCount(0),
      lastTransactionMs(0),
      resultCallback(nullptr),
      successCount(0),
      failureCount(0),
      timeoutCount(0)
{
    memset(&currentRequest, 0, sizeof(currentRequest));
}

// ============================================================================
// init
// ============================================================================

bool SDOManager::init(SDOResultCallback callback) {
    resultCallback = callback;

    requestQueue = xQueueCreate(SDO_QUEUE_DEPTH, sizeof(SDORequest));
    rxFrameQueue = xQueueCreate(16, sizeof(twai_message_t));
    statsMutex   = xSemaphoreCreateMutex();

    if (!requestQueue || !rxFrameQueue || !statsMutex) {
        Serial.println("[SDO] ERROR: Failed to create FreeRTOS objects");
        return false;
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        sdoTaskEntry,
        "SDOTask",
        4096,
        this,
        5,
        &taskHandle,
        0
    );

    if (rc != pdPASS) {
        Serial.println("[SDO] ERROR: Failed to create task");
        return false;
    }

    Serial.println("[SDO] Initialized (async state machine)");
    Serial.printf("[SDO] TX: 0x%03X  RX: 0x%03X\n", SDO_TX_ID, SDO_RX_ID);
    return true;
}

// ============================================================================
// Public queue API
// ============================================================================

bool SDOManager::requestRead(uint16_t paramId, bool highPriority) {
    SDORequest req = { SDO_REQ_READ, paramId, 0, highPriority };
    return xQueueSend(requestQueue, &req, 0) == pdTRUE;
}

bool SDOManager::requestWrite(uint16_t paramId, int32_t value, bool highPriority) {
    SDORequest req = { SDO_REQ_WRITE, paramId, value, highPriority };
    if (highPriority)
        return xQueueSendToFront(requestQueue, &req, 0) == pdTRUE;
    return xQueueSend(requestQueue, &req, 0) == pdTRUE;
}

bool SDOManager::requestSaveFlash() {
    SDORequest req = { SDO_REQ_SAVE_FLASH, 0, 6, true };
    return xQueueSendToFront(requestQueue, &req, 0) == pdTRUE;
}

uint16_t SDOManager::getQueueDepth() {
    return (uint16_t)uxQueueMessagesWaiting(requestQueue);
}

// ============================================================================
// processIncomingFrame
// ============================================================================

void SDOManager::processIncomingFrame(const twai_message_t& msg) {
    if (msg.identifier == SDO_RX_ID) {
        xQueueSend(rxFrameQueue, &msg, 0);
    }
}

// ============================================================================
// FreeRTOS task entry
// ============================================================================

void SDOManager::sdoTaskEntry(void* arg) {
    static_cast<SDOManager*>(arg)->taskLoop();
}

// ============================================================================
// taskLoop
// ============================================================================

void SDOManager::taskLoop() {
    for (;;) {
        uint32_t now = millis();

        switch (state) {

        case SDO_IDLE: {
            if ((now - lastTransactionMs) < SDO_MIN_GAP_MS) {
                vTaskDelay(pdMS_TO_TICKS(1));
                break;
            }
            if (xQueueReceive(requestQueue, &currentRequest,
                              pdMS_TO_TICKS(10)) == pdTRUE) {
                retryCount = 0;
                state = SDO_SEND_REQUEST;
            }
            break;
        }

        case SDO_SEND_REQUEST: {
            uint8_t cmd;
            int32_t val = 0;

            switch (currentRequest.type) {
                case SDO_REQ_READ:
                    cmd = SDO_CMD_READ;
                    break;
                case SDO_REQ_WRITE:
                case SDO_REQ_SAVE_FLASH:
                    cmd = SDO_CMD_WRITE;
                    val = currentRequest.value;
                    break;
                default:
                    cmd = SDO_CMD_READ;
                    break;
            }

            if (sendFrame(cmd, currentRequest.paramId, val)) {
                stateEnteredMs = millis();
                state = SDO_WAIT_RESPONSE;
            } else {
                Serial.printf("[SDO] TX failed param %d\n", currentRequest.paramId);
                state = SDO_RETRY;
            }
            break;
        }

        case SDO_WAIT_RESPONSE: {
            twai_message_t frame;
            if (xQueueReceive(rxFrameQueue, &frame,
                              pdMS_TO_TICKS(5)) == pdTRUE) {
                handleFrame(frame);
                break;
            }
            if ((millis() - stateEnteredMs) >= SDO_TIMEOUT_MS) {
                Serial.printf("[SDO] Timeout param %d (attempt %d)\n",
                              currentRequest.paramId, retryCount + 1);
                if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
                    timeoutCount++;
                    xSemaphoreGive(statsMutex);
                }
                state = SDO_RETRY;
            }
            break;
        }

        case SDO_PROCESS_RESPONSE:
            lastTransactionMs = millis();
            state = SDO_IDLE;
            break;

        case SDO_RETRY: {
            retryCount++;
            if (retryCount <= SDO_MAX_RETRIES) {
                Serial.printf("[SDO] Retry %d/%d param %d\n",
                              retryCount, SDO_MAX_RETRIES,
                              currentRequest.paramId);
                vTaskDelay(pdMS_TO_TICKS(20));
                state = SDO_SEND_REQUEST;
            } else {
                state = SDO_FAIL;
            }
            break;
        }

        case SDO_FAIL: {
            Serial.printf("[SDO] FAILED param %d after %d retries\n",
                          currentRequest.paramId, SDO_MAX_RETRIES);
            deliverResult(false, currentRequest.paramId, 0,
                          currentRequest.type != SDO_REQ_READ,
                          SDO_ABORT_TIMEOUT);
            if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
                failureCount++;
                xSemaphoreGive(statsMutex);
            }
            lastTransactionMs = millis();
            state = SDO_IDLE;
            break;
        }

        } // switch

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ============================================================================
// handleFrame
// Reconstruct full 16-bit paramId from index and subindex in response frame.
// ZombieVerter format: index = 0x2100 | (paramId >> 8), subindex = paramId & 0xFF
// ============================================================================

void SDOManager::handleFrame(const twai_message_t& msg) {
    if (msg.data_length_code < 4) return;

    uint8_t  cmd      = msg.data[0];
    uint16_t index    = (uint16_t)(msg.data[1] | (msg.data[2] << 8));
    uint8_t  subindex = msg.data[3];

    // Reconstruct full 16-bit param ID from index and subindex
    uint16_t paramId = (uint16_t)(((index & 0xFF) << 8) | subindex);

    switch (cmd) {

        case SDO_RESP_READ_4B:
        case SDO_RESP_READ_2B: {
            int32_t value = (int32_t)(
                msg.data[4]         |
                (msg.data[5] << 8)  |
                (msg.data[6] << 16) |
                (msg.data[7] << 24)
            );
            Serial.printf("[SDO] RX Read OK param %d = %d\n", paramId, value);
            deliverResult(true, paramId, value, false);
            if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
                successCount++;
                xSemaphoreGive(statsMutex);
            }
            state = SDO_PROCESS_RESPONSE;
            break;
        }

        case SDO_RESP_WRITE: {
            Serial.printf("[SDO] RX Write OK param %d\n", paramId);
            deliverResult(true, paramId, currentRequest.value, true);
            if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
                successCount++;
                xSemaphoreGive(statsMutex);
            }
            state = SDO_PROCESS_RESPONSE;
            break;
        }

        case SDO_RESP_ABORT: {
            uint32_t abortCode = (uint32_t)(
                msg.data[4]         |
                (msg.data[5] << 8)  |
                (msg.data[6] << 16) |
                (msg.data[7] << 24)
            );
            Serial.printf("[SDO] RX Abort param %d code 0x%08X (%s)\n",
                          paramId, abortCode, abortDescription(abortCode));
            deliverResult(false, paramId, 0,
                          currentRequest.type != SDO_REQ_READ, abortCode);
            if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
                failureCount++;
                xSemaphoreGive(statsMutex);
            }
            lastTransactionMs = millis();
            state = SDO_IDLE;
            break;
        }

        default:
            Serial.printf("[SDO] RX Unknown cmd 0x%02X param %d\n", cmd, paramId);
            break;
    }
}

// ============================================================================
// sendFrame
// Calculates SDO index and subindex from full 16-bit param ID:
//   index    = 0x2100 | (paramId >> 8)
//   subindex = paramId & 0xFF
// For params 1-255: index=0x2100, subindex=paramId  (same as before)
// For spot 2006:    index=0x2107, subindex=0xD6
// ============================================================================

bool SDOManager::sendFrame(uint8_t cmd, uint16_t paramId, int32_t value) {
    twai_message_t tx = {};
    tx.identifier       = SDO_TX_ID;
    tx.data_length_code = 8;
    tx.extd             = 0;
    tx.rtr              = 0;

    uint16_t index    = SDO_BASE_INDEX | (paramId >> 8);
    uint8_t  subindex = paramId & 0xFF;

    tx.data[0] = cmd;
    tx.data[1] = (uint8_t)(index & 0xFF);
    tx.data[2] = (uint8_t)(index >> 8);
    tx.data[3] = subindex;
    tx.data[4] = (uint8_t)( value        & 0xFF);
    tx.data[5] = (uint8_t)((value >>  8) & 0xFF);
    tx.data[6] = (uint8_t)((value >> 16) & 0xFF);
    tx.data[7] = (uint8_t)((value >> 24) & 0xFF);

    esp_err_t rc = twai_transmit(&tx, pdMS_TO_TICKS(10));
    if (rc != ESP_OK) {
        Serial.printf("[SDO] twai_transmit failed: %s\n", esp_err_to_name(rc));
        return false;
    }

    Serial.printf("[SDO] TX [%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                  tx.data[0], tx.data[1], tx.data[2], tx.data[3],
                  tx.data[4], tx.data[5], tx.data[6], tx.data[7]);
    return true;
}

// ============================================================================
// deliverResult
// ============================================================================

void SDOManager::deliverResult(bool success, uint16_t paramId,
                                int32_t value, bool isWrite,
                                uint32_t abortCode) {
    if (!resultCallback) return;
    SDOResult result = { paramId, value, success, isWrite, abortCode };
    resultCallback(result);
}

// ============================================================================
// abortDescription
// ============================================================================

const char* SDOManager::abortDescription(uint32_t code) {
    switch (code) {
        case SDO_ABORT_TOGGLE:        return "Toggle bit not alternated";
        case SDO_ABORT_TIMEOUT:       return "SDO protocol timed out";
        case SDO_ABORT_CMD_INVALID:   return "Invalid command";
        case SDO_ABORT_PARAM_INVALID: return "Object does not exist";
        case SDO_ABORT_PARAM_RANGE:   return "Value out of range";
        case SDO_ABORT_GENERAL:       return "General error";
        default:                      return "Unknown";
    }
}