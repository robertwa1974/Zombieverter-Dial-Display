// =============================================================================
// WiFiManager.cpp — ZombieVerter Dial Display
//
// Implements the openinverter/jamiejones85 esp32-web-interface API surface so
// the existing HTML/JS/CSS web assets work without modification.
//
// The original firmware routes /cmd over UART to a separate MCU.
// This implementation routes the same /cmd calls to CANDataManager,
// which reads/writes parameters via SDO over CAN.
//
// Uses ESPAsyncWebServer for reliable concurrent file serving from SPIFFS.
// =============================================================================

#include "WiFiManager.h"
#include "CANData.h"
#include "Config.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <SPIFFS.h>
#include <FS.h>

// ---------------------------------------------------------------------------
// Module-level server instance
// ---------------------------------------------------------------------------
static AsyncWebServer* server = nullptr;
static WiFiManager* instance = nullptr;

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

    Serial.printf("[WiFi] AP started: %s / %s\n", WIFI_AP_SSID, WIFI_AP_PASSWORD);
    Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());

    if (MDNS.begin("inverter")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("[WiFi] mDNS: http://inverter.local");
    }

    startServer();
    active = true;
}

// ---------------------------------------------------------------------------
// stopAP
// ---------------------------------------------------------------------------
void WiFiManager::stopAP() {
    if (!active) return;
    stopServer();
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
    // Intentionally empty — ESPAsyncWebServer is fully non-blocking
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
    // Catch-all — serve static files from SPIFFS (with .gz support)
    // -----------------------------------------------------------------------
    server->onNotFound([](AsyncWebServerRequest* request) {
        if (!instance) { request->send(404); return; }
        if (!instance->serveFile(request)) {
            request->send(404, "text/plain", "Not Found: " + request->url());
        }
    });

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
        // Serve params.json directly from SPIFFS — fast, no heap allocation.
        // The JS polls /cmd?cmd=get for live values after initial load.
        if (!serveFile(request, "/params.json")) {
            request->send(200, "text/json", "{}");
        }
        return;

    } else if (cmd.startsWith("get ")) {
        String names = cmd.substring(4);
        names.trim();
        response = cmdGet(names);
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
// cmdGet
// ---------------------------------------------------------------------------
String WiFiManager::cmdGet(const String& names) {
    if (!can) return "";

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

        CANParameter* param = nullptr;
        for (uint16_t i = 0; i < can->getParameterCount(); i++) {
            CANParameter* p = can->getParameterByIndex(i);
            if (p && name.equals(p->name)) {
                param = p;
                break;
            }
        }

        char valBuf[32];
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
// cmdSet
// ---------------------------------------------------------------------------
String WiFiManager::cmdSet(const String& name, const String& value) {
    if (!can) return "0\n";

    for (uint16_t i = 0; i < can->getParameterCount(); i++) {
        CANParameter* p = can->getParameterByIndex(i);
        if (p && name.equals(p->name)) {
            int32_t intVal = (int32_t)value.toFloat();
            can->setParameter(p->id, intVal);
            Serial.printf("[WiFi] Set %s = %d via web\n", p->name, intVal);
            return "1\n";
        }
    }

    Serial.printf("[WiFi] Set: param '%s' not found\n", name.c_str());
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