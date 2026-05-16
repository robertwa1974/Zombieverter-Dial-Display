// =============================================================================
// WiFiManager.cpp — ZombieVerter Dial Display
//
// Implements the openinverter/jamiejones85 esp32-web-interface API surface so
// the existing HTML/JS/CSS web assets work without modification.
//
// Uses ESPAsyncWebServer for concurrent file serving.
// Uses synchronous SDO polling (like jamiejones CAN backend) during WiFi mode
// so /cmd?cmd=json returns live values and /cmd?cmd=get works correctly.
// =============================================================================

#include "WiFiManager.h"
#include "CANData.h"
#include "CANMonitor.h"
#include "Config.h"
#include "TripLogger.h"
#include "GVRETServer.h"
#include "FaultLogger.h"
#include "EfficiencyTracker.h"
#include "HealthChecker.h"
#include "UIManager.h"
#include "Immobilizer.h"
#include "pngle.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "driver/twai.h"

// ---------------------------------------------------------------------------
// SDO constants (mirroring jamiejones oi_can.cpp)
// ---------------------------------------------------------------------------
#define SDO_READ              (2 << 5)
#define SDO_WRITE             ((1 << 5) | (1 << 1) | 1)
#define SDO_WRITE_REPLY       (3 << 5)
#define SDO_READ_REPLY        ((2 << 5) | (1 << 1) | 1)
#define SDO_ABORT             0x80
#define SDO_INDEX_PARAM_UID   0x2100
#define SDO_INDEX_COMMANDS    0x5002
#define SDO_CMD_SAVE          0

// ---------------------------------------------------------------------------
// Synchronous SDO helpers — used during WiFi mode when LVGL is suspended
// Sends a raw SDO request and waits up to timeoutMs for a response.
// Returns the raw int32 value (caller applies /32 scaling if needed).
// ---------------------------------------------------------------------------
static bool sdoRequestSync(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                           twai_message_t* response, uint32_t timeoutMs = 50) {
    twai_message_t tx;
    tx.extd = false;
    tx.identifier = 0x600 | nodeId;
    tx.data_length_code = 8;
    tx.data[0] = SDO_READ;
    tx.data[1] = index & 0xFF;
    tx.data[2] = index >> 8;
    tx.data[3] = subIndex;
    tx.data[4] = tx.data[5] = tx.data[6] = tx.data[7] = 0;

    if (twai_transmit(&tx, pdMS_TO_TICKS(10)) != ESP_OK) return false;

    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (twai_receive(response, pdMS_TO_TICKS(5)) == ESP_OK) {
            if (response->identifier == (uint32_t)(0x580 | nodeId)) return true;
        }
    }
    return false;
}

static bool sdoWriteSync(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                         uint32_t value, uint32_t timeoutMs = 50) {
    twai_message_t tx, rx;
    tx.extd = false;
    tx.identifier = 0x600 | nodeId;
    tx.data_length_code = 8;
    tx.data[0] = SDO_WRITE;
    tx.data[1] = index & 0xFF;
    tx.data[2] = index >> 8;
    tx.data[3] = subIndex;
    *(uint32_t*)&tx.data[4] = value;

    if (twai_transmit(&tx, pdMS_TO_TICKS(10)) != ESP_OK) return false;

    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (twai_receive(&rx, pdMS_TO_TICKS(5)) == ESP_OK) {
            if (rx.identifier == (uint32_t)(0x580 | nodeId)) {
                return rx.data[0] != SDO_ABORT;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Module-level server instance
// ---------------------------------------------------------------------------
static AsyncWebServer* server = nullptr;
static WiFiManager* instance = nullptr;

// ---------------------------------------------------------------------------
// Logo upload — struct, statics and pngle callbacks defined here so they are
// visible to the inline lambda inside startServer()
// ---------------------------------------------------------------------------
struct LogoUploadCtx {
    pngle_t*  pngle      = nullptr;
    File      file;
    uint16_t  srcW       = 0;
    uint16_t  srcH       = 0;
    // Destination rectangle within 240x240 (letterboxed)
    uint16_t  dstX0      = 0;   // left edge of image in output
    uint16_t  dstY0      = 0;   // top edge of image in output
    uint16_t  dstW       = 0;   // scaled width
    uint16_t  dstH       = 0;   // scaled height
    uint16_t  rowBuf[240];
    uint32_t  lastY      = 0xFFFF;
    bool      error      = false;
    uint32_t  pixCount   = 0;
};

static LogoUploadCtx* s_logoCtx    = nullptr;
static bool           s_logoUploadOk = false;

static void logo_on_init(pngle_t* pngle, uint32_t w, uint32_t h) {
    if (!s_logoCtx) return;
    s_logoCtx->srcW = (uint16_t)w;
    s_logoCtx->srcH = (uint16_t)h;

    // Letterbox: scale to fit within 240x240 maintaining aspect ratio
    float scaleW = 240.0f / w;
    float scaleH = 240.0f / h;
    float scale  = (scaleW < scaleH) ? scaleW : scaleH;  // fit, don't crop
    s_logoCtx->dstW  = (uint16_t)(w * scale);
    s_logoCtx->dstH  = (uint16_t)(h * scale);
    s_logoCtx->dstX0 = (240 - s_logoCtx->dstW) / 2;
    s_logoCtx->dstY0 = (240 - s_logoCtx->dstH) / 2;

    Serial.printf("[LOGO] %dx%d -> %dx%d at (%d,%d) letterboxed\n",
        w, h, s_logoCtx->dstW, s_logoCtx->dstH,
        s_logoCtx->dstX0, s_logoCtx->dstY0);

    s_logoCtx->file = SPIFFS.open("/logo.tmp", "w");
    if (!s_logoCtx->file) {
        Serial.println("[LOGO] SPIFFS open failed");
        s_logoCtx->error = true;
        return;
    }
    uint16_t fw = 240, fh = 240;
    s_logoCtx->file.write((uint8_t*)&fw, 2);
    s_logoCtx->file.write((uint8_t*)&fh, 2);

    // Pre-fill with black
    memset(s_logoCtx->rowBuf, 0, sizeof(s_logoCtx->rowBuf));
    for (int row = 0; row < 240; row++) {
        s_logoCtx->file.write((uint8_t*)s_logoCtx->rowBuf, 480);
        if (row % 30 == 0) yield();
    }
    Serial.println("[LOGO] Pre-fill done, starting decode");
}

static void logo_flush_row(uint32_t y) {
    // Seek to this row's position and write it
    if (!s_logoCtx || !s_logoCtx->file) return;
    uint32_t offset = 4 + (uint32_t)y * 240 * 2;
    s_logoCtx->file.seek(offset);
    s_logoCtx->file.write((uint8_t*)s_logoCtx->rowBuf, 480);
    memset(s_logoCtx->rowBuf, 0, 480);  // clear for next row
}

static void logo_on_draw(pngle_t* pngle, uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h,
                          const uint8_t rgba[4]) {
    if (!s_logoCtx || s_logoCtx->error || !s_logoCtx->file) return;

    // Map source pixel to destination using letterbox rect
    uint32_t dstX = s_logoCtx->dstX0 + (uint32_t)x * s_logoCtx->dstW / s_logoCtx->srcW;
    uint32_t dstY = s_logoCtx->dstY0 + (uint32_t)y * s_logoCtx->dstH / s_logoCtx->srcH;
    if (dstX >= 240 || dstY >= 240) return;

    // Flush previous row when we move to a new destination row
    if (dstY != s_logoCtx->lastY && s_logoCtx->lastY != 0xFFFF) {
        logo_flush_row(s_logoCtx->lastY);
    }
    s_logoCtx->lastY = dstY;

    // Convert RGBA8888 → RGB565 byte-swapped for LVGL
    uint8_t r = rgba[0], g = rgba[1], b = rgba[2];
    uint16_t rgb565  = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    uint16_t swapped = (rgb565 >> 8) | (rgb565 << 8);
    s_logoCtx->rowBuf[dstX] = swapped;
    s_logoCtx->pixCount++;
    if (s_logoCtx->pixCount % 10000 == 0) yield();
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
WiFiManager::WiFiManager()
    : can(nullptr), active(false), serverStarted(false)
{
    instance = this;
}

// ---------------------------------------------------------------------------
// init — mounts SPIFFS, configures AP, registers routes
// ---------------------------------------------------------------------------
bool WiFiManager::init(CANDataManager* canManager) {
    can = canManager;

    if (!SPIFFS.begin(true)) {
        Serial.println("[WiFi] SPIFFS mount failed");
    }

    WiFi.mode(WIFI_OFF);
    active = false;

    Serial.println("[WiFi] Manager initialized (AP off at boot)");
    return true;
}

// ---------------------------------------------------------------------------
// startAP — bring up the access point and web server
// ---------------------------------------------------------------------------
void WiFiManager::startAP() {
    if (active) return;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    delay(100); // let AP stabilize before starting server

    Serial.printf("[WiFi] AP started: %s / %s\n", WIFI_AP_SSID, WIFI_AP_PASSWORD);
    Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());

    if (MDNS.begin("inverter")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("[WiFi] mDNS: http://inverter.local");
    }

    startServer();
    GVRETServer::getInstance().begin();
    active = true;
}

// ---------------------------------------------------------------------------
// stopAP
// ---------------------------------------------------------------------------
void WiFiManager::stopAP() {
    if (!active) return;
    stopServer();
    GVRETServer::getInstance().stop();
    MDNS.end();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    active = false;
    Serial.println("[WiFi] AP stopped");
}

// ---------------------------------------------------------------------------
// update — no-op: ESPAsyncWebServer handles everything internally
// ---------------------------------------------------------------------------
void WiFiManager::update() {
    if (active) GVRETServer::getInstance().update();

    // Deferred PNG decode — runs on loopTask so async_tcp watchdog is never starved
    if (pngPending && pngBuffer && pngBufLen > 0) {
        pngPending = false;
        Serial.printf("[LOGO] Decoding %u bytes on loopTask...\n", (unsigned)pngBufLen);

        s_logoCtx = new LogoUploadCtx();
        s_logoCtx->pngle = pngle_new();
        bool ok = false;

        if (s_logoCtx->pngle) {
            pngle_set_init_callback(s_logoCtx->pngle, logo_on_init);
            pngle_set_draw_callback(s_logoCtx->pngle, logo_on_draw);

            const size_t CHUNK = 2048;
            bool error = false;
            for (size_t off = 0; off < pngBufLen && !error; off += CHUNK) {
                size_t n = min(CHUNK, pngBufLen - off);
                if (pngle_feed(s_logoCtx->pngle, pngBuffer + off, n) < 0) {
                    Serial.printf("[LOGO] pngle error: %s\n", pngle_error(s_logoCtx->pngle));
                    s_logoCtx->error = true;
                    error = true;
                }
                yield();
            }

            if (!error && s_logoCtx->file && s_logoCtx->srcW > 0) {
                if (s_logoCtx->lastY != 0xFFFF) logo_flush_row(s_logoCtx->lastY);
                s_logoCtx->file.close();
                SPIFFS.remove("/logo.bin");
                SPIFFS.rename("/logo.tmp", "/logo.bin");
                ok = true;
                s_logoUploadOk = true;
                logoReloadRequested = true;
                Serial.printf("[LOGO] /logo.bin written OK (%u pixels)\n", s_logoCtx->pixCount);
            } else {
                if (s_logoCtx->file) s_logoCtx->file.close();
                SPIFFS.remove("/logo.tmp");
                Serial.println("[LOGO] Decode failed — logo.bin preserved");
            }
        }

        pngle_destroy(s_logoCtx->pngle);
        delete s_logoCtx;
        s_logoCtx = nullptr;

        free(pngBuffer);
        pngBuffer = nullptr;
        pngBufLen = pngBufCap = 0;
        logoUploadInProgress = false;
        Serial.printf("[LOGO] Decode %s\n", ok ? "OK" : "FAILED");
    }
}

// ---------------------------------------------------------------------------
// getIPAddress
// ---------------------------------------------------------------------------
String WiFiManager::getIPAddress() {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
        return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

// ---------------------------------------------------------------------------
// startServer — register all routes
// ---------------------------------------------------------------------------
void WiFiManager::startServer() {
    if (serverStarted) return;

    server = new AsyncWebServer(80);

    // -----------------------------------------------------------------------
    // /cmd — the core API used by all web pages
    // -----------------------------------------------------------------------
    server->on("/cmd", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleCmd(request);
    });

    // -----------------------------------------------------------------------
    // /wifi — GET returns settings page, POST updates credentials
    // -----------------------------------------------------------------------
    server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleWifiGet(request);
    });

    server->on("/wifi", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleWifiPost(request);
    });

    // -----------------------------------------------------------------------
    // /list — SPIFFS file listing
    // -----------------------------------------------------------------------
    server->on("/list", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleFileList(request);
    });

    // -----------------------------------------------------------------------
    // /edit DELETE — delete file from SPIFFS
    // -----------------------------------------------------------------------
    server->on("/edit", HTTP_DELETE, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleFileDelete(request);
    });

    // -----------------------------------------------------------------------
    // /edit POST — upload file to SPIFFS
    // -----------------------------------------------------------------------
    server->on("/edit", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "text/plain", "");
        },
        [](AsyncWebServerRequest* request, const String& filename,
           size_t index, uint8_t* data, size_t len, bool final) {
            if (!instance) return;
            instance->handleFileUpload(request, filename, index, data, len, final);
        }
    );

    // -----------------------------------------------------------------------
    // /spot — live parameter values from RAM, no SPIFFS
    // Returns {"name":value,...} for all parameters with live CAN data
    // -----------------------------------------------------------------------
    server->on("/spot", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance || !instance->can) { request->send(200, "application/json", "{}"); return; }
        instance->handleSpot(request);
    });

    // -----------------------------------------------------------------------
    // /value?id=N — single live value poll used by gauges.html
    // -----------------------------------------------------------------------
    server->on("/value", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(200, "text/plain", "0.00"); return; }
        instance->handleValue(request);
    });

    // -----------------------------------------------------------------------
    // /log — GET: CSV trip log download   DELETE: clear log
    // -----------------------------------------------------------------------
    server->on("/log", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleTripLog(request);
    });

    server->on("/log", HTTP_DELETE, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleTripLogDelete(request);
    });

    // -----------------------------------------------------------------------
    // /faults — GET: fault log JSON   DELETE: clear fault log
    // -----------------------------------------------------------------------
    server->on("/faults", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleFaultLog(request);
    });

    server->on("/faults", HTTP_DELETE, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleFaultLogDelete(request);
    });

    // -----------------------------------------------------------------------
    // /dial-settings — GET: read local M5Dial config (NVS)
    //                  POST: update local M5Dial config
    // Stores finalDrive and wheelCirc for EfficiencyTracker — not sent to VCU
    // -----------------------------------------------------------------------
    server->on("/dial-settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleDialSettingsGet(request);
    });

    server->on("/dial-settings", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "text/plain", "ok");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len,
           size_t index, size_t total) {
            if (!instance) return;
            instance->handleDialSettingsPost(request, data, len, index, total);
        }
    );

    // -----------------------------------------------------------------------
    // /version
    // -----------------------------------------------------------------------
    server->on("/version", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "ZVDisplay-1.0");
    });

    // -----------------------------------------------------------------------
    // /update — OTA firmware update
    // -----------------------------------------------------------------------
    server->on("/update", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(Update.hasError() ? 500 : 200, "text/plain",
                          Update.hasError() ? "Update FAILED" : "Update OK — rebooting");
            delay(500);
            ESP.restart();
        },
        [](AsyncWebServerRequest* request, const String& filename,
           size_t index, uint8_t* data, size_t len, bool final) {
            if (!instance) return;
            instance->handleOTA(request, filename, index, data, len, final);
        }
    );

    // -----------------------------------------------------------------------
    // /upload-logo  POST — accepts PNG, decodes, saves /logo.bin to SPIFFS
    // /delete-logo  DELETE — removes /logo.bin
    // -----------------------------------------------------------------------
    server->on("/upload-logo", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            // Immediate response — browser polls /logo-status for decode result
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json",
                "{\"ok\":true,\"message\":\"Buffered — decoding in background\"}");
            resp->addHeader("Access-Control-Allow-Origin", "*");
            request->send(resp);
        },
        [](AsyncWebServerRequest* request, const String& filename,
           size_t index, uint8_t* data, size_t len, bool final) {
            // Buffer raw PNG bytes only — all pngle/SPIFFS work happens in update()
            // on the loopTask so the async_tcp watchdog is never starved.
            if (index == 0) {
                s_logoUploadOk = false;
                Serial.printf("[LOGO] Upload start, filename=%s\n", filename.c_str());
                if (instance) {
                    if (instance->pngBuffer) { free(instance->pngBuffer); instance->pngBuffer = nullptr; }
                    instance->pngBufLen  = 0;
                    instance->pngBufCap  = 0;
                    instance->pngPending = false;
                    instance->logoUploadInProgress = true;
                }
            }
            if (instance && len > 0) {
                size_t needed = instance->pngBufLen + len;
                if (needed > instance->pngBufCap) {
                    size_t newCap = needed + 4096;
                    uint8_t* nb = (uint8_t*)realloc(instance->pngBuffer, newCap);
                    if (nb) { instance->pngBuffer = nb; instance->pngBufCap = newCap; }
                    else {
                        Serial.println("[LOGO] realloc failed");
                        free(instance->pngBuffer);
                        instance->pngBuffer = nullptr;
                        instance->pngBufLen = instance->pngBufCap = 0;
                        instance->logoUploadInProgress = false;
                        return;
                    }
                }
                memcpy(instance->pngBuffer + instance->pngBufLen, data, len);
                instance->pngBufLen += len;
            }
            if (final) {
                Serial.printf("[LOGO] Upload buffered %u bytes — decode deferred\n",
                    instance ? (unsigned)instance->pngBufLen : 0);
                if (instance && instance->pngBuffer && instance->pngBufLen > 0) {
                    instance->pngPending = true;
                } else {
                    if (instance) instance->logoUploadInProgress = false;
                    Serial.println("[LOGO] Nothing buffered");
                }
            }
        },
        nullptr
    );

    server->on("/delete-logo", HTTP_DELETE, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleLogoDelete(request);
    });

    // /logo-status — polled by browser after upload to know when deferred decode finishes
    server->on("/logo-status", HTTP_GET, [](AsyncWebServerRequest* request) {
        String state;
        if (instance && instance->pngPending) {
            state = "pending";
        } else if (instance && instance->logoUploadInProgress) {
            state = "decoding";
        } else if (s_logoUploadOk) {
            state = "ok";
        } else {
            state = "failed";
        }
        AsyncWebServerResponse* resp = request->beginResponse(200, "application/json",
            "{\"state\":\"" + state + "\"}");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        request->send(resp);
    });

    // -----------------------------------------------------------------------
    // Catch-all — serve static files from SPIFFS (with .gz support)
    // -----------------------------------------------------------------------
    server->onNotFound([](AsyncWebServerRequest* request) {
        if (!instance) { request->send(404); return; }
        if (!instance->serveFile(request)) {
            request->send(404, "text/plain", "Not Found: " + request->url());
        }
    });

    // -----------------------------------------------------------------------
    // /health-settings — GET: read health thresholds + fail behaviour (NVS)
    //                    POST: update and persist
    // -----------------------------------------------------------------------
    server->on("/health-settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!instance) { request->send(500); return; }
        instance->handleHealthSettingsGet(request);
    });

    server->on("/health-settings", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "text/plain", "ok");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len,
           size_t index, size_t total) {
            if (!instance) return;
            instance->handleHealthSettingsPost(request, data, len, index, total);
        }
    );

    // -----------------------------------------------------------------------
    // /refetch — trigger fresh VCU parameter download via SDO
    // -----------------------------------------------------------------------
    server->on("/refetch", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            if (!instance) { request->send(500); return; }
            instance->handleRefetch(request);
        }
    );

    // -----------------------------------------------------------------------
    // CAN Monitor — WebSocket + REST endpoints
    // -----------------------------------------------------------------------
    CANMonitor::instance().registerEndpoints(server);

    server->begin();
    serverStarted = true;
    Serial.println("[WiFi] Async web server started on port 80");
}

// ---------------------------------------------------------------------------
// stopServer
// ---------------------------------------------------------------------------
void WiFiManager::stopServer() {
    if (!serverStarted || !server) return;
    server->end();
    delete server;
    server = nullptr;
    serverStarted = false;
}

// ===========================================================================
// Route handlers
// ===========================================================================

// ---------------------------------------------------------------------------
// /cmd
// ---------------------------------------------------------------------------
void WiFiManager::handleCmd(AsyncWebServerRequest* request) {
    if (!request->hasParam("cmd")) {
        request->send(400, "text/plain", "BAD ARGS");
        return;
    }

    String cmd = request->getParam("cmd")->value();
    cmd.trim();
    String response;

    AsyncWebServerResponse* resp = nullptr;

    if (cmd == "json") {
        serveFile(request, "/params.json");
        return;

    } else if (cmd.startsWith("stream ")) {
        // plot.js uses this path when inverter.firmwareVersion >= 3.53
        // format: "stream <repeat> <name1,name2,...>"
        int firstSpace = cmd.indexOf(' ', 7);
        if (firstSpace < 0) { request->send(200, "text/plain", ""); return; }
        int repeat = cmd.substring(7, firstSpace).toInt();
        if (repeat < 1) repeat = 1;
        if (repeat > 500) repeat = 500;
        String names = cmd.substring(firstSpace + 1);
        names.trim();
        response = cmdGetRepeated(names, repeat);
        resp = request->beginResponse(200, "text/plain", response);

    } else if (cmd.startsWith("get ")) {
        // plot.js uses: /cmd?cmd=get name1,name2&repeat=N
        String names = cmd.substring(4);
        names.trim();
        int repeat = 1;
        if (request->hasParam("repeat")) {
            repeat = request->getParam("repeat")->value().toInt();
            if (repeat < 1) repeat = 1;
            if (repeat > 500) repeat = 500;
        }
        response = cmdGetRepeated(names, repeat);
        resp = request->beginResponse(200, "text/plain", response);

    } else if (cmd.startsWith("set ")) {
        String rest = cmd.substring(4);
        rest.trim();
        int spaceIdx = rest.indexOf(' ');
        if (spaceIdx < 0) {
            request->send(200, "text/plain", "0\n");
            return;
        }
        String name  = rest.substring(0, spaceIdx);
        String value = rest.substring(spaceIdx + 1);
        name.trim();
        value.trim();
        response = cmdSet(name, value);
        resp = request->beginResponse(200, "text/plain", response);

    } else if (cmd == "save" || cmd == "load") {
        resp = request->beginResponse(200, "text/plain", "1\n");

    } else if (cmd == "errors") {
        resp = request->beginResponse(200, "text/plain", "0\n");

    } else {
        resp = request->beginResponse(200, "text/plain", "1\n");
    }

    if (resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        request->send(resp);
    }
}

// ---------------------------------------------------------------------------
// cmdJson — not used, params.json served directly from SPIFFS
// ---------------------------------------------------------------------------
String WiFiManager::cmdJson() {
    return "{}";
}

// ---------------------------------------------------------------------------
// cmdGet — returns live values from local CAN parameter cache
// The cache is updated continuously from CAN broadcasts and SDO polling
// ---------------------------------------------------------------------------
String WiFiManager::cmdGet(const String& names) {
    String response;
    String nameList = names;
    nameList.trim();

    int start = 0;
    while (start < (int)nameList.length()) {
        int comma = nameList.indexOf(',', start);
        String name;
        if (comma < 0) {
            name = nameList.substring(start);
            start = nameList.length();
        } else {
            name = nameList.substring(start, comma);
            start = comma + 1;
        }
        name.trim();
        if (name.length() == 0) continue;

        char valBuf[32];
        CANParameter* param = can ? can->getParameterByName(name.c_str()) : nullptr;
        if (param) {
            snprintf(valBuf, sizeof(valBuf), "%.2f", (float)param->getValueAsInt());
        } else {
            snprintf(valBuf, sizeof(valBuf), "0.00");
        }
        response += valBuf;
        response += "\n";
    }
    return response;
}

// ---------------------------------------------------------------------------
// cmdSet — writes a parameter value via synchronous SDO
// ---------------------------------------------------------------------------
String WiFiManager::cmdSet(const String& name, const String& value) {
    if (!can) return "0\n";

    // Look up param in local cache to get id
    CANParameter* p = can->getParameterByName(name.c_str());
    if (!p) {
        Serial.printf("[WiFi] Set: param '%s' not found\n", name.c_str());
        return "0\n";
    }

    double dval = value.toDouble();
    uint32_t raw = (uint32_t)(int32_t)(dval * 32.0);

    uint16_t index = SDO_INDEX_PARAM_UID | (p->id >> 8);
    uint8_t  sub   = p->id & 0xFF;

    if (sdoWriteSync(CAN_NODE_ID, index, sub, raw)) {
        Serial.printf("[WiFi] Set %s = %.2f\n", name.c_str(), dval);
        p->setValue((int32_t)dval);
        return "1\n";
    }

    Serial.printf("[WiFi] Set %s failed\n", name.c_str());
    return "0\n";
}

// ---------------------------------------------------------------------------
// /wifi GET
// ---------------------------------------------------------------------------
void WiFiManager::handleWifiGet(AsyncWebServerRequest* request) {
    if (SPIFFS.exists("/wifi.html")) {
        File f = SPIFFS.open("/wifi.html", "r");
        String html = f.readString();
        f.close();
        html.replace("%apSSID%",  WiFi.softAPSSID());
        html.replace("%staSSID%", WiFi.SSID());
        html.replace("%staIP%",   WiFi.localIP().toString());
        request->send(200, "text/html", html);
    } else {
        request->send(404, "text/plain", "wifi.html not found");
    }
}

// ---------------------------------------------------------------------------
// /wifi POST
// ---------------------------------------------------------------------------
void WiFiManager::handleWifiPost(AsyncWebServerRequest* request) {
    if (request->hasParam("apSSID", true) && request->hasParam("apPW", true)) {
        String ssid = request->getParam("apSSID", true)->value();
        String pw   = request->getParam("apPW", true)->value();
        if (ssid.length() > 0 && pw.length() >= 8) {
            WiFi.softAP(ssid.c_str(), pw.c_str());
            Serial.printf("[WiFi] AP credentials updated: %s\n", ssid.c_str());
        }
    } else if (request->hasParam("staSSID", true) && request->hasParam("staPW", true)) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(request->getParam("staSSID", true)->value().c_str(),
                   request->getParam("staPW", true)->value().c_str());
        Serial.printf("[WiFi] STA connect attempt: %s\n",
                      request->getParam("staSSID", true)->value().c_str());
    }
    serveFile(request, "/wifi-updated.html");
}

// ---------------------------------------------------------------------------
// /list
// ---------------------------------------------------------------------------
void WiFiManager::handleFileList(AsyncWebServerRequest* request) {
    File root = SPIFFS.open("/");
    String output = "[";
    File file = root.openNextFile();
    while (file) {
        if (output != "[") output += ",";
        output += "{\"type\":\"file\",\"name\":\"";
        output += String(file.name());
        output += "\"}";
        file = root.openNextFile();
    }
    output += "]";
    request->send(200, "text/json", output);
}

// ---------------------------------------------------------------------------
// /edit POST — file upload
// ---------------------------------------------------------------------------
void WiFiManager::handleFileUpload(AsyncWebServerRequest* request,
                                    const String& filename,
                                    size_t index, uint8_t* data,
                                    size_t len, bool final) {
    static File fsUploadFile;
    String path = filename.startsWith("/") ? filename : "/" + filename;

    if (index == 0) {
        Serial.printf("[WiFi] Upload start: %s\n", path.c_str());
        fsUploadFile = SPIFFS.open(path, "w");
    }
    if (fsUploadFile) {
        fsUploadFile.write(data, len);
    }
    if (final) {
        if (fsUploadFile) {
            fsUploadFile.close();
            Serial.printf("[WiFi] Upload complete: %u bytes\n", index + len);
        }
    }
}

// ---------------------------------------------------------------------------
// /edit DELETE
// ---------------------------------------------------------------------------
void WiFiManager::handleFileDelete(AsyncWebServerRequest* request) {
    if (!request->hasParam("f")) {
        request->send(400, "text/plain", "BAD ARGS");
        return;
    }
    String path = request->getParam("f")->value();
    if (!SPIFFS.exists(path)) {
        request->send(404, "text/plain", "File not found");
        return;
    }
    SPIFFS.remove(path);
    Serial.printf("[WiFi] Deleted: %s\n", path.c_str());
    request->send(200, "text/plain", "");
}

// ---------------------------------------------------------------------------
// /refetch — trigger VCU parameter re-download
// Sets a flag; main loop picks it up and calls fetchParamsFromVCU()
// so we don't block the async TCP task.
// ---------------------------------------------------------------------------
void WiFiManager::handleRefetch(AsyncWebServerRequest* request) {
    refetchRequested = true;
    Serial.println("[WiFi] Refetch from VCU requested via web UI");
    request->send(200, "application/json",
        "{\"ok\":true,\"message\":\"Refetch started\"}");
}

// ---------------------------------------------------------------------------
// /update — OTA
// ---------------------------------------------------------------------------
void WiFiManager::handleOTA(AsyncWebServerRequest* request,
                              const String& filename,
                              size_t index, uint8_t* data,
                              size_t len, bool final) {
    if (index == 0) {
        Serial.printf("[OTA] Update start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (Update.end(true)) {
            Serial.printf("[OTA] Update complete: %u bytes\n", index + len);
        } else {
            Update.printError(Serial);
        }
    }
}

// ===========================================================================
// Static file serving
// ===========================================================================

String WiFiManager::getContentType(const String& filename) {
    if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
    if (filename.endsWith(".css"))  return "text/css";
    if (filename.endsWith(".js"))   return "application/javascript";
    if (filename.endsWith(".json")) return "application/json";
    if (filename.endsWith(".png"))  return "image/png";
    if (filename.endsWith(".gif"))  return "image/gif";
    if (filename.endsWith(".jpg"))  return "image/jpeg";
    if (filename.endsWith(".ico"))  return "image/x-icon";
    if (filename.endsWith(".gz"))   return "application/x-gzip";
    return "text/plain";
}

// Serve file from SPIFFS — tries .gz version first
// Called from onNotFound catch-all
bool WiFiManager::serveFile(AsyncWebServerRequest* request, const String& overridePath) {
    String path = overridePath.length() > 0 ? overridePath : request->url();
    if (path == "/") path = "/index.html";

    // Try .gz first
    String gzPath = path + ".gz";
    if (SPIFFS.exists(gzPath)) {
        AsyncWebServerResponse* response =
            request->beginResponse(SPIFFS, gzPath, getContentType(path));
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=86400");
        request->send(response);
        return true;
    }

    if (SPIFFS.exists(path)) {
        AsyncWebServerResponse* response =
            request->beginResponse(SPIFFS, path, getContentType(path));
        response->addHeader("Cache-Control", "max-age=86400");
        request->send(response);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// cmdGetRepeated — returns <repeat> samples of named params, space-separated
// plot.js and log.js call /cmd?cmd=get names&repeat=N (or cmd=stream N names).
// Response is a flat stream of floats; the JS regex matches all of them and
// distributes them across datasets in signal order.
// ---------------------------------------------------------------------------
String WiFiManager::cmdGetRepeated(const String& names, int repeat) {
    // Parse comma-separated signal names
    std::vector<String> nameList;
    int start = 0;
    while (start < (int)names.length()) {
        int comma = names.indexOf(',', start);
        String name = (comma < 0) ? names.substring(start) : names.substring(start, comma);
        start = (comma < 0) ? names.length() : comma + 1;
        name.trim();
        if (name.length() > 0) nameList.push_back(name);
    }
    if (nameList.empty()) return "";

    String response;
    response.reserve(nameList.size() * repeat * 8);
    for (int r = 0; r < repeat; r++) {
        for (const String& name : nameList) {
            char valBuf[24];
            CANParameter* p = can ? can->getParameterByName(name.c_str()) : nullptr;
            snprintf(valBuf, sizeof(valBuf), "%.2f", p ? (float)p->getValueAsInt() : 0.0f);
            response += valBuf;
            response += " ";
        }
    }
    return response;
}

// ---------------------------------------------------------------------------
// handleValue — GET /value?id=N
// Returns a plain float for the parameter with that ID.
// gauges.html polls this once per gauge per update interval.
// ---------------------------------------------------------------------------
void WiFiManager::handleValue(AsyncWebServerRequest* request) {
    if (!request->hasParam("id")) {
        request->send(200, "text/plain", "0.00");
        return;
    }
    uint16_t paramId = (uint16_t)request->getParam("id")->value().toInt();
    char valBuf[24];
    CANParameter* p = can ? can->getParameter(paramId) : nullptr;
    snprintf(valBuf, sizeof(valBuf), "%.2f", p ? (float)p->getValueAsInt() : 0.0f);
    AsyncWebServerResponse* resp = request->beginResponse(200, "text/plain", String(valBuf));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Cache-Control", "no-cache");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleTripLog — GET /log
// Returns the full trip log as a CSV download (chronological, ring buffer order)
// ---------------------------------------------------------------------------
void WiFiManager::handleTripLog(AsyncWebServerRequest* request) {
    String csv = TripLogger::getInstance().getCSV();
    AsyncWebServerResponse* resp = request->beginResponse(200, "text/csv", csv);
    resp->addHeader("Content-Disposition", "attachment; filename=\"trip_log.csv\"");
    resp->addHeader("Access-Control-Allow-Origin", "*");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleTripLogDelete — DELETE /log
// Clears all trip log entries from NVS
// ---------------------------------------------------------------------------
void WiFiManager::handleTripLogDelete(AsyncWebServerRequest* request) {
    TripLogger::getInstance().clear();
    AsyncWebServerResponse* resp = request->beginResponse(200, "text/plain", "cleared");
    resp->addHeader("Access-Control-Allow-Origin", "*");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleFaultLog — GET /faults
// Returns NVS fault/opmode log as JSON array, newest first
// ---------------------------------------------------------------------------
void WiFiManager::handleFaultLog(AsyncWebServerRequest* request) {
    String json = FaultLogger::getInstance().getJSON();
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", json);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Cache-Control", "no-cache");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleFaultLogDelete — DELETE /faults
// Clears all fault log entries from NVS
// ---------------------------------------------------------------------------
void WiFiManager::handleFaultLogDelete(AsyncWebServerRequest* request) {
    FaultLogger::getInstance().clear();
    AsyncWebServerResponse* resp = request->beginResponse(200, "text/plain", "cleared");
    resp->addHeader("Access-Control-Allow-Origin", "*");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleDialSettingsGet — GET /dial-settings
// Returns M5Dial-local config (finalDrive, wheelCirc) as JSON
// ---------------------------------------------------------------------------
void WiFiManager::handleDialSettingsGet(AsyncWebServerRequest* request) {
    Preferences prefs;
    prefs.begin("dialsettings", true);
    float    finalDrive  = prefs.getFloat("finalDrive",  4.10f);
    float    wheelCirc   = prefs.getFloat("wheelCirc",   1.88f);
    uint16_t screenMask   = prefs.getUShort("screenMask",  0xFFFF);
    bool     immobEnabled = prefs.getBool("immobEnabled",  true);
    uint16_t immobWriteId = prefs.getUShort("immobWriteId", VCU_PARAM_DRIVE_INHIBIT);
    uint16_t immobReadId  = prefs.getUShort("immobReadId",  VCU_SPOT_DRIVE_INHIBITED);
    prefs.end();

    String json = "{\"finalDrive\":"   + String(finalDrive, 2) +
                  ",\"wheelCirc\":"    + String(wheelCirc,  2) +
                  ",\"screenMask\":"   + String(screenMask)    +
                  ",\"immobEnabled\":" + String(immobEnabled ? "true" : "false") +
                  ",\"immobWriteId\":" + String(immobWriteId) +
                  ",\"immobReadId\":"  + String(immobReadId)  + "}";
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", json);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleDialSettingsPost — POST /dial-settings
// Saves finalDrive and/or wheelCirc to NVS and updates EfficiencyTracker
// ---------------------------------------------------------------------------
void WiFiManager::handleDialSettingsPost(AsyncWebServerRequest* request,
                                          uint8_t* data, size_t len,
                                          size_t index, size_t total) {
    String body = "";
    for (size_t i = 0; i < len; i++) body += (char)data[i];

    Preferences prefs;
    prefs.begin("dialsettings", false);
    float    finalDrive   = prefs.getFloat("finalDrive",   4.10f);
    float    wheelCirc    = prefs.getFloat("wheelCirc",    1.88f);
    uint16_t screenMask   = prefs.getUShort("screenMask",  0xFFFF);
    bool     immobEnabled = prefs.getBool("immobEnabled",  true);

    int idx = body.indexOf("\"finalDrive\"");
    if (idx >= 0) {
        int colon = body.indexOf(':', idx);
        int comma = body.indexOf(',', colon);
        if (comma < 0) comma = body.indexOf('}', colon);
        float v = body.substring(colon + 1, comma).toFloat();
        if (v > 1.0f && v < 20.0f) { finalDrive = v; prefs.putFloat("finalDrive", v); }
    }

    idx = body.indexOf("\"wheelCirc\"");
    if (idx >= 0) {
        int colon = body.indexOf(':', idx);
        int comma = body.indexOf(',', colon);
        if (comma < 0) comma = body.indexOf('}', colon);
        float v = body.substring(colon + 1, comma).toFloat();
        if (v > 0.5f && v < 4.0f) { wheelCirc = v; prefs.putFloat("wheelCirc", v); }
    }

    idx = body.indexOf("\"screenMask\"");
    if (idx >= 0) {
        int colon = body.indexOf(':', idx);
        int comma = body.indexOf(',', colon);
        if (comma < 0) comma = body.indexOf('}', colon);
        int v = body.substring(colon + 1, comma).toInt();
        // Force Dashboard(2), WiFi(10), Settings(11) always on
        uint16_t forced = (1u << SCREEN_DASHBOARD) | (1u << SCREEN_WIFI) | (1u << SCREEN_SETTINGS);
        screenMask = (uint16_t)v | forced;
        prefs.putUShort("screenMask", screenMask);
        if (uiManager) uiManager->setScreenMask(screenMask);  // apply live
        Serial.printf("[WIFI] screenMask updated: 0x%04X\n", screenMask);
    }

    idx = body.indexOf("\"immobEnabled\"");
    if (idx >= 0) {
        int colon = body.indexOf(':', idx);
        int comma = body.indexOf(',', colon);
        if (comma < 0) comma = body.indexOf('}', colon);
        String val = body.substring(colon + 1, comma);
        val.trim();
        immobEnabled = (val == "true");
        prefs.putBool("immobEnabled", immobEnabled);
        if (immobilizer) immobilizer->setEnabled(immobEnabled);  // apply live
        Serial.printf("[WIFI] immobEnabled updated: %s\n", immobEnabled ? "true" : "false");
    }

    idx = body.indexOf("\"immobWriteId\"");
    if (idx >= 0) {
        int colon = body.indexOf(':', idx);
        int comma = body.indexOf(',', colon);
        if (comma < 0) comma = body.indexOf('}', colon);
        int v = body.substring(colon + 1, comma).toInt();
        if (v >= 0 && v <= 65535) {
            prefs.putUShort("immobWriteId", (uint16_t)v);
            uint16_t rid = prefs.getUShort("immobReadId", VCU_SPOT_DRIVE_INHIBITED);
            if (immobilizer) immobilizer->setInhibitParams((uint16_t)v, rid);
            Serial.printf("[WIFI] immobWriteId updated: %d\n", v);
        }
    }

    idx = body.indexOf("\"immobReadId\"");
    if (idx >= 0) {
        int colon = body.indexOf(':', idx);
        int comma = body.indexOf(',', colon);
        if (comma < 0) comma = body.indexOf('}', colon);
        int v = body.substring(colon + 1, comma).toInt();
        if (v >= 0 && v <= 65535) {
            prefs.putUShort("immobReadId", (uint16_t)v);
            uint16_t wid = prefs.getUShort("immobWriteId", VCU_PARAM_DRIVE_INHIBIT);
            if (immobilizer) immobilizer->setInhibitParams(wid, (uint16_t)v);
            Serial.printf("[WIFI] immobReadId updated: %d\n", v);
        }
    }

    prefs.end();
    EfficiencyTracker::getInstance().setDriveParams(finalDrive, wheelCirc);
    Serial.printf("[WIFI] dial-settings updated: finalDrive=%.2f wheelCirc=%.2f\n",
                  finalDrive, wheelCirc);
}

// ---------------------------------------------------------------------------
// handleSpot — build live values JSON purely from in-memory parameter cache
// No SPIFFS, no deserialization — just loop over parameters[] array
// ---------------------------------------------------------------------------
void WiFiManager::handleSpot(AsyncWebServerRequest* request) {
    String json = "{";
    bool first = true;
    uint16_t count = can->getParameterCount();

    for (uint16_t i = 0; i < count; i++) {
        CANParameter* p = can->getParameterByIndex(i);
        if (!p || p->lastUpdateTime == 0) continue;

        if (!first) json += ",";
        json += "\"";
        json += p->name;
        json += "\":";
        json += p->valueInt;
        first = false;
    }
    json += "}";

    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", json);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Cache-Control", "no-cache");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleHealthSettingsGet — GET /health-settings
// Returns current thresholds and fail behaviour as JSON
// ---------------------------------------------------------------------------
void WiFiManager::handleHealthSettingsGet(AsyncWebServerRequest* request) {
    HealthChecker& hc = HealthChecker::getInstance();
    String json = "{";
    json += "\"failBehav\":"   + String((int)hc.getFailBehaviour())  + ",";
    json += "\"cdWarn\":"      + String(hc.getCellDeltaWarn(),  1)   + ",";
    json += "\"cdFail\":"      + String(hc.getCellDeltaFail(),  1)   + ",";
    json += "\"mcWarn\":"      + String(hc.getMinCellWarn(),    3)   + ",";
    json += "\"mcFail\":"      + String(hc.getMinCellFail(),    3)   + ",";
    json += "\"mtWarn\":"      + String(hc.getMotorTempWarn(),  1)   + ",";
    json += "\"mtFail\":"      + String(hc.getMotorTempFail(),  1)   + ",";
    json += "\"itWarn\":"      + String(hc.getInvTempWarn(),    1)   + ",";
    json += "\"itFail\":"      + String(hc.getInvTempFail(),    1)   + ",";
    json += "\"pvWarn\":"      + String(hc.getPackVoltWarn(),   1)   + ",";
    json += "\"pvFail\":"      + String(hc.getPackVoltFail(),   1);
    json += "}";
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", json);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    request->send(resp);
}

// ---------------------------------------------------------------------------
// handleHealthSettingsPost — POST /health-settings
// Accepts JSON body, updates HealthChecker, saves to NVS
// ---------------------------------------------------------------------------
void WiFiManager::handleHealthSettingsPost(AsyncWebServerRequest* request,
                                            uint8_t* data, size_t len,
                                            size_t index, size_t total) {
    String body = "";
    for (size_t i = 0; i < len; i++) body += (char)data[i];

    HealthChecker& hc = HealthChecker::getInstance();

    // Helper: extract float value for a key from a simple JSON string
    auto getF = [&](const char* key, float def) -> float {
        String k = "\""; k += key; k += "\":";
        int idx = body.indexOf(k);
        if (idx < 0) return def;
        int start = idx + k.length();
        int end = body.indexOf(',', start);
        if (end < 0) end = body.indexOf('}', start);
        String s = body.substring(start, end);
        s.trim();
        return s.toFloat();
    };
    auto getI = [&](const char* key, int def) -> int {
        String k = "\""; k += key; k += "\":";
        int idx = body.indexOf(k);
        if (idx < 0) return def;
        int start = idx + k.length();
        int end = body.indexOf(',', start);
        if (end < 0) end = body.indexOf('}', start);
        String s = body.substring(start, end);
        s.trim();
        return s.toInt();
    };

    hc.setFailBehaviour((HealthFailBehaviour)getI("failBehav", (int)HealthFailBehaviour::SEVERITY));
    hc.setCellDeltaWarn(getF("cdWarn", hc.getCellDeltaWarn()));
    hc.setCellDeltaFail(getF("cdFail", hc.getCellDeltaFail()));
    hc.setMinCellWarn(  getF("mcWarn", hc.getMinCellWarn()));
    hc.setMinCellFail(  getF("mcFail", hc.getMinCellFail()));
    hc.setMotorTempWarn(getF("mtWarn", hc.getMotorTempWarn()));
    hc.setMotorTempFail(getF("mtFail", hc.getMotorTempFail()));
    hc.setInvTempWarn(  getF("itWarn", hc.getInvTempWarn()));
    hc.setInvTempFail(  getF("itFail", hc.getInvTempFail()));
    hc.setPackVoltWarn( getF("pvWarn", hc.getPackVoltWarn()));
    hc.setPackVoltFail( getF("pvFail", hc.getPackVoltFail()));
    hc.saveToNVS();

    Serial.println("[WiFi] Health settings updated and saved");
}

// ---------------------------------------------------------------------------
// Logo upload state — lives for the duration of one POST request
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// handleLogoUpload — legacy stub, actual upload handled inline in startServer()
void WiFiManager::handleLogoUpload(AsyncWebServerRequest* request,
                                    const String& filename,
                                    size_t index, uint8_t* data,
                                    size_t len, bool final) {
    // No-op — upload is handled by the inline body lambda in startServer()
}

// ---------------------------------------------------------------------------
// handleLogoDelete — DELETE /delete-logo
// ---------------------------------------------------------------------------
void WiFiManager::handleLogoDelete(AsyncWebServerRequest* request) {
    if (SPIFFS.exists("/logo.bin")) {
        SPIFFS.remove("/logo.bin");
        logoReloadRequested = true;   // clears live widget via main.cpp → reloadLogo()
        Serial.println("[LOGO] /logo.bin deleted");
        AsyncWebServerResponse* resp = request->beginResponse(200, "application/json",
            "{\"ok\":true,\"message\":\"Logo removed\"}");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        request->send(resp);
    } else {
        request->send(404, "application/json", "{\"ok\":false,\"message\":\"No logo\"}");
    }
}
