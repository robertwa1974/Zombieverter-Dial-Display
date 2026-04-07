#include "CANMonitor.h"
#include "CANData.h"
#include <ArduinoJson.h>

// =============================================================================
// Known CAN ID names
// =============================================================================

const char* CANMonitor::knownIDName(uint32_t id) {
    switch (id) {
        case 0x603: return "SDO Request";
        case 0x583: return "SDO Response";
        case 0x355: return "SOC Broadcast";
        case 0x356: return "Motor Temp";
        case 0x126: return "Inverter Temp";
        case 0x257: return "Speed";
        case 0x521: return "IVT-S U1";
        case 0x522: return "IVT-S U2 (Battery V)";
        case 0x523: return "IVT-S U3";
        case 0x524: return "IVT-S U4";
        case 0x525: return "IVT-S I2";
        case 0x526: return "IVT-S Temp";
        case 0x527: return "IVT-S Power";
        case 0x528: return "IVT-S Charge";
        case 0x411: return "Current (IVT-S)";
        case 0x373: return "BMS Cell Voltages";
        case 0x351: return "BMS Limits";
        case 0x35A: return "BMS Status";
        case 0x35E: return "BMS Name";
        case 0x500: return "VCU Heartbeat";
        case 0x300: return "Gear Command";
        case 0x301: return "Motor Command";
        case 0x302: return "Regen Command";
        default:    return nullptr;
    }
}

// =============================================================================
// init
// =============================================================================

void CANMonitor::init(CANDataManager* mgr) {
    canMgr = mgr;
    bufMutex  = xSemaphoreCreateMutex();
    wsQMutex  = xSemaphoreCreateMutex();
    memset(idStats, 0, sizeof(idStats));
    memset(throttle, 0, sizeof(throttle));
    sessionStartMs = millis();
    Serial.println("[CANMonitor] Initialized");
}

// =============================================================================
// registerEndpoints
// =============================================================================

void CANMonitor::registerEndpoints(AsyncWebServer* server) {
    // WebSocket — allow up to 2 clients, ping every 10s to detect dead connections
    ws = new AsyncWebSocket("/ws/can");
    ws->onEvent(onWsEvent);
    ws->enable(true);
    server->addHandler(ws);

    // HTTP endpoints
    server->on("/can/transmit", HTTP_POST,
        [](AsyncWebServerRequest* r){ CANMonitor::instance().handleTransmit(r); });

    server->on("/can/log/start", HTTP_POST,
        [](AsyncWebServerRequest* r){ CANMonitor::instance().handleLogStart(r); });

    server->on("/can/log/stop", HTTP_POST,
        [](AsyncWebServerRequest* r){ CANMonitor::instance().handleLogStop(r); });

    server->on("/can/log/download", HTTP_GET,
        [](AsyncWebServerRequest* r){ CANMonitor::instance().handleLogDownload(r); });

    server->on("/can/stats", HTTP_GET,
        [](AsyncWebServerRequest* r){ CANMonitor::instance().handleStats(r); });

    Serial.println("[CANMonitor] Endpoints registered");
}

// =============================================================================
// pushFrame — called from CANData::update() for every received frame
// =============================================================================

void CANMonitor::pushFrame(const twai_message_t& msg) {
    CANFrame f;
    f.timestamp = millis();
    f.id        = msg.identifier;
    f.len       = msg.data_length_code;
    memcpy(f.data, msg.data, 8);

    // Periodic WebSocket cleanup — prevents stale connections piling up
    static uint32_t lastCleanup = 0;
    if (ws && millis() - lastCleanup > 2000) {
        ws->cleanupClients();
        lastCleanup = millis();
    }

    // Update per-ID statistics (always, regardless of active state)
    updateIDStat(f.id);
    sessionFrameCount++;

    // Decode once — used by both logging and WebSocket
    String decoded = (logState == LogState::LOGGING || clientCount > 0)
                     ? decodeFrame(f) : String("");

    // Write to CSV log if logging
    if (logState == LogState::LOGGING) {
        writeCSVRow(f, decoded);
        logFrameCount++;
    }

    // Push to WebSocket clients if any connected
    if (clientCount > 0 && ws) {
        if (!shouldThrottle(f.id)) {
            markSent(f.id);
            uint32_t cnt = 0;
            for (uint8_t i = 0; i < idStatCount; i++) {
                if (idStats[i].id == f.id) { cnt = idStats[i].count; break; }
            }
            enqueueWsMsg(frameToJson(f, decoded, cnt));
        }
    }
    flushWsQueue();

    // Store in circular buffer
    if (xSemaphoreTake(bufMutex, 0) == pdTRUE) {
        frameBuf[bufHead] = f;
        bufHead = (bufHead + 1) % BUF_SIZE;
        if (bufCount < BUF_SIZE) bufCount++;
        xSemaphoreGive(bufMutex);
    }
}

// =============================================================================
// transmitFrame
// =============================================================================

bool CANMonitor::transmitFrame(uint32_t id, uint8_t* data, uint8_t len) {
    twai_message_t msg = {};
    msg.identifier       = id;
    msg.data_length_code = len;
    msg.extd             = 0;
    memcpy(msg.data, data, len);
    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(20));
    if (err != ESP_OK) {
        Serial.printf("[CANMonitor] Transmit failed: 0x%X\n", err);
        return false;
    }
    Serial.printf("[CANMonitor] TX: ID=0x%03X len=%d\n", id, len);
    return true;
}

// =============================================================================
// Logging
// =============================================================================

void CANMonitor::startLogging() {
    if (logState == LogState::LOGGING) return;

    // Remove old log
    if (SPIFFS.exists("/canlog.csv")) SPIFFS.remove("/canlog.csv");

    logFile = SPIFFS.open("/canlog.csv", "w");
    if (!logFile) {
        Serial.println("[CANMonitor] Failed to open /canlog.csv for writing");
        return;
    }

    // CSV header
    logFile.println("timestamp_ms,id_hex,id_name,len,b0,b1,b2,b3,b4,b5,b6,b7,decoded");
    logFrameCount = 0;
    logState = LogState::LOGGING;
    Serial.println("[CANMonitor] Logging started");
}

void CANMonitor::stopLogging() {
    if (logState != LogState::LOGGING) return;
    logFile.flush();
    logFile.close();
    logState = LogState::STOPPED;
    Serial.printf("[CANMonitor] Logging stopped — %u frames\n", logFrameCount);
}

void CANMonitor::writeCSVRow(const CANFrame& f, const String& decoded) {
    if (!logFile) return;
    const char* name = knownIDName(f.id);

    char row[200];
    snprintf(row, sizeof(row),
        "%u,0x%03X,%s,%d,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%s",
        f.timestamp,
        f.id,
        name ? name : "",
        f.len,
        f.data[0], f.data[1], f.data[2], f.data[3],
        f.data[4], f.data[5], f.data[6], f.data[7],
        decoded.c_str()
    );
    logFile.println(row);
}

// =============================================================================
// Decoder
// =============================================================================

String CANMonitor::decodeFrame(const CANFrame& f) {
    char buf[80];

    switch (f.id) {
        case 0x583: {
            // SDO Response: [cmd, idxLo, idxHi, subIdx, v0, v1, v2, v3]
            uint8_t cmd = f.data[0];
            uint16_t idx = f.data[1] | (f.data[2] << 8);
            uint8_t sub  = f.data[3];
            int32_t val  = (int32_t)(f.data[4] | (f.data[5]<<8) |
                                     (f.data[6]<<16) | (f.data[7]<<24));
            if (cmd == 0x80) {
                snprintf(buf, sizeof(buf), "ABORT idx=0x%04X sub=%d err=0x%08X",
                    idx, sub, (unsigned)val);
            } else if (cmd == 0x60) {
                snprintf(buf, sizeof(buf), "WRITE ACK idx=0x%04X sub=%d", idx, sub);
            } else {
                // Read response — look up param name
                float fval = val / 32.0f;
                const char* paramName = nullptr;
                if (canMgr) {
                    // Reconstruct paramId from index/subindex
                    uint16_t paramId = ((idx & 0xFF) << 8) | sub;
                    CANParameter* p = canMgr->getParameter(paramId);
                    if (p) paramName = p->name;
                }
                if (paramName) {
                    snprintf(buf, sizeof(buf), "READ %s = %.2f", paramName, fval);
                } else {
                    snprintf(buf, sizeof(buf), "READ idx=0x%04X sub=%d val=%.2f",
                        idx, sub, fval);
                }
            }
            return String(buf);
        }

        case 0x603: {
            // SDO Request
            uint8_t cmd = f.data[0];
            uint16_t idx = f.data[1] | (f.data[2] << 8);
            uint8_t sub  = f.data[3];
            if (cmd == 0x40) {
                snprintf(buf, sizeof(buf), "READ REQ idx=0x%04X sub=%d", idx, sub);
            } else if (cmd == 0x23) {
                int32_t val = (int32_t)(f.data[4] | (f.data[5]<<8) |
                                        (f.data[6]<<16) | (f.data[7]<<24));
                snprintf(buf, sizeof(buf), "WRITE REQ idx=0x%04X sub=%d val=%d (%.2f)",
                    idx, sub, val, val / 32.0f);
            } else if (cmd == 0x60) {
                snprintf(buf, sizeof(buf), "SEGMENT REQ toggle=%d",
                    (f.data[0] >> 4) & 1);
            } else {
                snprintf(buf, sizeof(buf), "SDO cmd=0x%02X", cmd);
            }
            return String(buf);
        }

        case 0x355: {
            int16_t soc = f.data[0] | (f.data[1] << 8);
            snprintf(buf, sizeof(buf), "SOC=%d%%", soc);
            return String(buf);
        }

        case 0x356: {
            int16_t tmpm = (int16_t)(f.data[4] | (f.data[5] << 8));
            snprintf(buf, sizeof(buf), "Motor temp=%d°C", tmpm);
            return String(buf);
        }

        case 0x126: {
            int16_t tmphs = (int16_t)(f.data[4] | (f.data[5] << 8));
            snprintf(buf, sizeof(buf), "Inverter temp=%d°C", tmphs);
            return String(buf);
        }

        case 0x257: {
            int16_t spd = (int16_t)(f.data[0] | (f.data[1] << 8));
            snprintf(buf, sizeof(buf), "Speed=%d rpm", spd);
            return String(buf);
        }

        case 0x521: {
            int32_t mv = (int32_t)(f.data[2] | (f.data[3]<<8) | (f.data[4]<<16));
            if (mv & 0x800000) mv |= 0xFF000000;
            snprintf(buf, sizeof(buf), "U1=%.1fV", mv / 1000.0f);
            return String(buf);
        }
        case 0x522: {
            // IVT-S: bytes 2-4 = 24-bit little-endian value in mV
            int32_t mv = (int32_t)(f.data[2] | (f.data[3] << 8) | (f.data[4] << 16));
            if (mv & 0x800000) mv |= 0xFF000000;  // sign extend
            snprintf(buf, sizeof(buf), "Voltage=%.1fV", mv / 1000.0f);
            return String(buf);
        }
        case 0x523: {
            int32_t mv = (int32_t)(f.data[2] | (f.data[3]<<8) | (f.data[4]<<16));
            if (mv & 0x800000) mv |= 0xFF000000;
            snprintf(buf, sizeof(buf), "U3=%.1fV", mv / 1000.0f);
            return String(buf);
        }
        case 0x524: {
            int32_t mv = (int32_t)(f.data[2] | (f.data[3]<<8) | (f.data[4]<<16));
            if (mv & 0x800000) mv |= 0xFF000000;
            snprintf(buf, sizeof(buf), "U4=%.1fV", mv / 1000.0f);
            return String(buf);
        }
        case 0x525: {
            int32_t ma = (int32_t)(f.data[2] | (f.data[3]<<8) | (f.data[4]<<16));
            if (ma & 0x800000) ma |= 0xFF000000;
            snprintf(buf, sizeof(buf), "I2=%.2fA", ma / 1000.0f);
            return String(buf);
        }
        case 0x526: {
            int32_t dt = (int32_t)(f.data[2] | (f.data[3]<<8) | (f.data[4]<<16));
            if (dt & 0x800000) dt |= 0xFF000000;
            snprintf(buf, sizeof(buf), "Temp=%.1f°C", dt / 10.0f);
            return String(buf);
        }
        case 0x527: {
            int32_t pw = (int32_t)(f.data[2] | (f.data[3]<<8) | (f.data[4]<<16));
            if (pw & 0x800000) pw |= 0xFF000000;
            snprintf(buf, sizeof(buf), "Power=%.1fkW", pw / 1000.0f);
            return String(buf);
        }
        case 0x528: {
            int32_t as = (int32_t)(f.data[2] | (f.data[3]<<8) | (f.data[4]<<16));
            if (as & 0x800000) as |= 0xFF000000;
            snprintf(buf, sizeof(buf), "Charge=%.1fAs", as / 10.0f);
            return String(buf);
        }

        case 0x411: {
            // IVT-S: bytes 2-4 = 24-bit little-endian value in mA
            int32_t ma = (int32_t)(f.data[2] | (f.data[3] << 8) | (f.data[4] << 16));
            if (ma & 0x800000) ma |= 0xFF000000;  // sign extend
            snprintf(buf, sizeof(buf), "Current=%.2fA", ma / 1000.0f);
            return String(buf);
        }

        case 0x373: {
            uint16_t vmin = f.data[0] | (f.data[1] << 8);
            uint16_t vmax = f.data[2] | (f.data[3] << 8);
            snprintf(buf, sizeof(buf), "Vmin=%umV Vmax=%umV delta=%umV",
                vmin, vmax, vmax - vmin);
            return String(buf);
        }

        case 0x35E: {
            // BMS name — ASCII string
            char name[9] = {0};
            for (int i = 0; i < 8 && i < (int)f.len; i++) name[i] = f.data[i];
            snprintf(buf, sizeof(buf), "BMS=%s", name);
            return String(buf);
        }

        default:
            return String("");
    }
}

// =============================================================================
// Frame → JSON
// =============================================================================

String CANMonitor::frameToJson(const CANFrame& f, const String& decoded,
                                uint32_t countForID) {
    char json[300];
    const char* name = knownIDName(f.id);

    snprintf(json, sizeof(json),
        "{\"t\":%u,\"id\":\"0x%03X\",\"name\":\"%s\",\"len\":%d,"
        "\"data\":[%d,%d,%d,%d,%d,%d,%d,%d],"
        "\"decoded\":\"%s\",\"count\":%u}",
        f.timestamp,
        f.id,
        name ? name : "",
        f.len,
        f.data[0], f.data[1], f.data[2], f.data[3],
        f.data[4], f.data[5], f.data[6], f.data[7],
        decoded.c_str(),
        countForID
    );
    return String(json);
}

// =============================================================================
// WebSocket send queue — enqueue from anywhere, flush from main loop only
// =============================================================================

void CANMonitor::enqueueWsMsg(const String& json) {
    if (!wsQMutex) return;
    if (xSemaphoreTake(wsQMutex, 0) == pdTRUE) {
        uint8_t next = (wsQHead + 1) % WS_QUEUE_SIZE;
        if (next != wsQTail) {  // not full
            strncpy(wsQueue[wsQHead].json, json.c_str(), 299);
            wsQueue[wsQHead].json[299] = '\0';
            wsQHead = next;
        }
        xSemaphoreGive(wsQMutex);
    }
}

void CANMonitor::flushWsQueue() {
    if (!ws || !wsQMutex) return;
    if (xSemaphoreTake(wsQMutex, 0) != pdTRUE) return;
    while (wsQTail != wsQHead) {
        ws->textAll(wsQueue[wsQTail].json);
        wsQTail = (wsQTail + 1) % WS_QUEUE_SIZE;
    }
    xSemaphoreGive(wsQMutex);
}

// =============================================================================
// Throttle — don't send same ID more than once per 50ms to WebSocket
// =============================================================================

bool CANMonitor::shouldThrottle(uint32_t id) {
    for (uint8_t i = 0; i < THROTTLE_SLOTS; i++) {
        if (throttle[i].id == id) {
            return (millis() - throttle[i].lastSentMs) < 50;
        }
    }
    return false;  // ID not seen before — don't throttle
}

void CANMonitor::markSent(uint32_t id) {
    // Find existing slot
    for (uint8_t i = 0; i < THROTTLE_SLOTS; i++) {
        if (throttle[i].id == id) {
            throttle[i].lastSentMs = millis();
            return;
        }
    }
    // Find empty slot
    for (uint8_t i = 0; i < THROTTLE_SLOTS; i++) {
        if (throttle[i].id == 0) {
            throttle[i].id = id;
            throttle[i].lastSentMs = millis();
            return;
        }
    }
    // All slots full — evict oldest
    uint8_t oldest = 0;
    for (uint8_t i = 1; i < THROTTLE_SLOTS; i++) {
        if (throttle[i].lastSentMs < throttle[oldest].lastSentMs) oldest = i;
    }
    throttle[oldest].id = id;
    throttle[oldest].lastSentMs = millis();
}

// =============================================================================
// Per-ID statistics
// =============================================================================

void CANMonitor::updateIDStat(uint32_t id) {
    for (uint8_t i = 0; i < idStatCount; i++) {
        if (idStats[i].id == id) {
            idStats[i].count++;
            idStats[i].lastSeen = millis();
            return;
        }
    }
    if (idStatCount < MAX_ID_STATS) {
        idStats[idStatCount].id       = id;
        idStats[idStatCount].count    = 1;
        idStats[idStatCount].lastSeen = millis();
        idStatCount++;
    }
}

// =============================================================================
// WebSocket event handler
// =============================================================================

void CANMonitor::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                            AwsEventType type, void* arg, uint8_t* data, size_t len) {
    CANMonitor& mon = CANMonitor::instance();

    switch (type) {
        case WS_EVT_CONNECT:
            mon.clientCount++;
            Serial.printf("[CANMonitor] WS client %u connected (total=%d)\n",
                client->id(), mon.clientCount);
            // Send a hello with current log state
            {
                char hello[80];
                snprintf(hello, sizeof(hello),
                    "{\"type\":\"hello\",\"logState\":%d,\"logFrames\":%u}",
                    (int)mon.logState, mon.logFrameCount);
                client->text(hello);
            }
            break;

        case WS_EVT_DISCONNECT:
            if (mon.clientCount > 0) mon.clientCount--;
            Serial.printf("[CANMonitor] WS client %u disconnected (total=%d)\n",
                client->id(), mon.clientCount);
            break;

        case WS_EVT_DATA:
            mon.handleWsMessage(client, data, len);
            break;

        default:
            break;
    }
}

// =============================================================================
// Handle messages from browser over WebSocket
// =============================================================================

void CANMonitor::handleWsMessage(AsyncWebSocketClient* client,
                                  uint8_t* data, size_t len) {
    // Parse JSON command from browser
    // {"cmd":"transmit","id":1539,"data":[64,0,33,37,0,0,0,0]}
    // {"cmd":"logStart"}
    // {"cmd":"logStop"}

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        client->text("{\"error\":\"bad json\"}");
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "transmit") == 0) {
        uint32_t id = doc["id"] | 0;
        JsonArray arr = doc["data"];
        uint8_t bytes[8] = {0};
        uint8_t byteLen = 0;
        for (JsonVariant v : arr) {
            if (byteLen < 8) bytes[byteLen++] = v.as<uint8_t>();
        }
        bool ok = transmitFrame(id, bytes, byteLen);
        char resp[60];
        snprintf(resp, sizeof(resp), "{\"type\":\"txResult\",\"ok\":%s,\"id\":\"0x%03X\"}",
            ok ? "true" : "false", id);
        client->text(resp);
    }
    else if (strcmp(cmd, "logStart") == 0) {
        startLogging();
        client->text("{\"type\":\"logState\",\"state\":\"logging\"}");
    }
    else if (strcmp(cmd, "logStop") == 0) {
        stopLogging();
        char resp[80];
        snprintf(resp, sizeof(resp),
            "{\"type\":\"logState\",\"state\":\"stopped\",\"frames\":%u}", logFrameCount);
        client->text(resp);
    }
    else if (strcmp(cmd, "ping") == 0) {
        client->text("{\"type\":\"pong\"}");
    }
}

// =============================================================================
// HTTP handlers
// =============================================================================

void CANMonitor::handleTransmit(AsyncWebServerRequest* request) {
    // POST /can/transmit with body: {"id":1539,"data":[64,0,33,37,0,0,0,0]}
    request->send(200, "application/json", "{\"error\":\"use WebSocket\"}");
}

void CANMonitor::handleLogStart(AsyncWebServerRequest* request) {
    startLogging();
    request->send(200, "application/json",
        "{\"ok\":true,\"state\":\"logging\"}");
}

void CANMonitor::handleLogStop(AsyncWebServerRequest* request) {
    stopLogging();
    char resp[100];
    snprintf(resp, sizeof(resp),
        "{\"ok\":true,\"state\":\"stopped\",\"frames\":%u,\"sizeBytes\":%u}",
        logFrameCount,
        SPIFFS.exists("/canlog.csv") ? (unsigned)SPIFFS.open("/canlog.csv").size() : 0);
    request->send(200, "application/json", resp);
}

void CANMonitor::handleLogDownload(AsyncWebServerRequest* request) {
    if (!SPIFFS.exists("/canlog.csv")) {
        request->send(404, "text/plain", "No log file");
        return;
    }
    request->send(SPIFFS, "/canlog.csv", "text/csv",
        true);  // true = download (Content-Disposition: attachment)
}

void CANMonitor::handleStats(AsyncWebServerRequest* request) {
    // Build stats JSON
    String json = "{\"sessionFrames\":";
    json += sessionFrameCount;
    json += ",\"uptime\":";
    json += (millis() - sessionStartMs);
    json += ",\"ids\":[";

    // Sort by count (simple selection sort for small array)
    uint8_t order[MAX_ID_STATS];
    for (uint8_t i = 0; i < idStatCount; i++) order[i] = i;
    for (uint8_t i = 0; i < idStatCount; i++) {
        for (uint8_t j = i+1; j < idStatCount; j++) {
            if (idStats[order[j]].count > idStats[order[i]].count) {
                uint8_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }
    }

    uint8_t shown = min((uint8_t)10, idStatCount);
    for (uint8_t i = 0; i < shown; i++) {
        if (i) json += ",";
        const CANIDStat& s = idStats[order[i]];
        const char* name = knownIDName(s.id);
        json += "{\"id\":\"0x";
        char hex[8]; snprintf(hex, sizeof(hex), "%03X", s.id);
        json += hex;
        json += "\",\"name\":\"";
        json += name ? name : "";
        json += "\",\"count\":";
        json += s.count;
        json += "}";
    }
    json += "]}";

    request->send(200, "application/json", json);
}
