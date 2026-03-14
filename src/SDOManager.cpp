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
// init — create queues and start background task on core 0
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

    // Pin to core 0 alongside CAN driver; 4 KB stack is plenty
    BaseType_t rc = xTaskCreatePinnedToCore(
        sdoTaskEntry,
        "SDOTask",
        4096,
        this,
        5,              // priority — above CAN receive, below system
        &taskHandle,
        0               // core 0
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

bool SDOManager::requestRead(uint8_t paramId, bool highPriority) {
    SDORequest req = { SDO_REQ_READ, paramId, 0, highPriority };
    return xQueueSend(requestQueue, &req, 0) == pdTRUE;
}

bool SDOManager::requestWrite(uint8_t paramId, int32_t value, bool highPriority) {
    SDORequest req = { SDO_REQ_WRITE, paramId, value, highPriority };
    // Writes go to front of queue when high priority
    if (highPriority) {
        return xQueueSendToFront(requestQueue, &req, 0) == pdTRUE;
    }
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
// processIncomingFrame — called from the CAN receive path (any task/core)
// ============================================================================

void SDOManager::processIncomingFrame(const twai_message_t& msg) {
    if (msg.identifier == SDO_RX_ID) {
        // Non-blocking — drop frame if queue is full rather than stall caller
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
// taskLoop — the async state machine
// ============================================================================

void SDOManager::taskLoop() {
    for (;;) {
        uint32_t now = millis();

        switch (state) {

        // ----------------------------------------------------------------
        case SDO_IDLE: {
            // Enforce minimum inter-transaction gap
            if ((now - lastTransactionMs) < SDO_MIN_GAP_MS) {
                vTaskDelay(pdMS_TO_TICKS(1));
                break;
            }

            // Pull next request from queue (block up to 10 ms to yield CPU)
            if (xQueueReceive(requestQueue, &currentRequest,
                              pdMS_TO_TICKS(10)) == pdTRUE) {
                retryCount = 0;
                state = SDO_SEND_REQUEST;
            }
            break;
        }

        // ----------------------------------------------------------------
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
                // TX failed — treat as a retry-able failure
                Serial.printf("[SDO] TX failed param %d\n", currentRequest.paramId);
                state = SDO_RETRY;
            }
            break;
        }

        // ----------------------------------------------------------------
        case SDO_WAIT_RESPONSE: {
            twai_message_t frame;

            // Poll rx queue with a short wait so we don't spin-burn core 0
            if (xQueueReceive(rxFrameQueue, &frame,
                              pdMS_TO_TICKS(5)) == pdTRUE) {
                handleFrame(frame);
                // handleFrame() transitions state to SDO_PROCESS_RESPONSE or
                // SDO_FAIL on abort — nothing more to do here this tick
                break;
            }

            // Check timeout
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

        // ----------------------------------------------------------------
        case SDO_PROCESS_RESPONSE:
            // Arrived here from handleFrame() — nothing extra to do,
            // transition back to IDLE and note completion time
            lastTransactionMs = millis();
            state = SDO_IDLE;
            break;

        // ----------------------------------------------------------------
        case SDO_RETRY: {
            retryCount++;
            if (retryCount <= SDO_MAX_RETRIES) {
                Serial.printf("[SDO] Retry %d/%d param %d\n",
                              retryCount, SDO_MAX_RETRIES,
                              currentRequest.paramId);
                vTaskDelay(pdMS_TO_TICKS(20));  // Brief back-off
                state = SDO_SEND_REQUEST;
            } else {
                state = SDO_FAIL;
            }
            break;
        }

        // ----------------------------------------------------------------
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

        // Cooperative yield — keeps watchdog happy
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ============================================================================
// handleFrame — called from taskLoop when an SDO frame arrives
// ============================================================================

void SDOManager::handleFrame(const twai_message_t& msg) {
    if (msg.data_length_code < 4) return;

    uint8_t cmd     = msg.data[0];
    uint8_t paramId = msg.data[3];   // Subindex = param ID in ZombieVerter format

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
            // Abort is final — no retry
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
// ============================================================================

bool SDOManager::sendFrame(uint8_t cmd, uint8_t paramId, int32_t value) {
    twai_message_t tx = {};
    tx.identifier       = SDO_TX_ID;
    tx.data_length_code = 8;
    tx.extd             = 0;
    tx.rtr              = 0;

    tx.data[0] = cmd;
    tx.data[1] = SDO_INDEX_LO;
    tx.data[2] = SDO_INDEX_HI;
    tx.data[3] = paramId;
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
// deliverResult — invoke callback (if set) with transaction outcome
// ============================================================================

void SDOManager::deliverResult(bool success, uint8_t paramId,
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