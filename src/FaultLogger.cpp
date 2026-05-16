// ============================================================================
// FaultLogger.cpp
// ============================================================================

#include "FaultLogger.h"

void FaultLogger::begin() {
    _prefs.begin("faultlog", false);
    _count    = _prefs.getInt("count", 0);
    _startIdx = _prefs.getInt("start", 0);
    if (_count < 0 || _count > FAULTLOG_MAX_ENTRIES) _count = 0;
    if (_startIdx < 0 || _startIdx >= FAULTLOG_MAX_ENTRIES) _startIdx = 0;
    Serial.printf("[FAULT] Loaded %d fault entries\n", _count);
}

void FaultLogger::logFault(uint32_t abortCode, uint16_t paramId, uint8_t currentOpmode) {
    FaultEntry e;
    e.timestamp_ms = millis();
    e.abortCode    = abortCode;
    e.paramId      = paramId;
    e.opmode       = currentOpmode;
    e.isFault      = 1;
    push(e);
    Serial.printf("[FAULT] Logged abort 0x%08X param=%d opmode=%s\n",
                  abortCode, paramId, decodeOpmode(currentOpmode));
}

void FaultLogger::logOpmodeChange(uint8_t newOpmode) {
    FaultEntry e;
    e.timestamp_ms = millis();
    e.abortCode    = 0;
    e.paramId      = 0;
    e.opmode       = newOpmode;
    e.isFault      = 0;
    push(e);
    Serial.printf("[FAULT] Logged opmode -> %s\n", decodeOpmode(newOpmode));
}

void FaultLogger::push(const FaultEntry& e) {
    if (_count < FAULTLOG_MAX_ENTRIES) {
        writeSlot(_count, e);
        _count++;
        _prefs.putInt("count", _count);
    } else {
        writeSlot(_startIdx, e);
        _startIdx = (_startIdx + 1) % FAULTLOG_MAX_ENTRIES;
        _prefs.putInt("start", _startIdx);
    }
}

String FaultLogger::getJSON() {
    String json = "[";
    if (_count == 0) { json += "]"; return json; }

    int total = min(_count, (int)FAULTLOG_MAX_ENTRIES);
    bool first = true;

    // Newest first — iterate backwards through ring buffer
    for (int i = total - 1; i >= 0; i--) {
        int slot = (_startIdx + i) % FAULTLOG_MAX_ENTRIES;
        if (_count < FAULTLOG_MAX_ENTRIES) slot = i;

        FaultEntry e;
        if (!readSlot(slot, e)) continue;

        if (!first) json += ",";
        first = false;

        float t_s = e.timestamp_ms / 1000.0f;
        json += "{";
        json += "\"t\":" + String(t_s, 1) + ",";
        json += "\"type\":\"" + String(e.isFault ? "fault" : "opmode") + "\",";
        if (e.isFault) {
            char codeBuf[12];
            snprintf(codeBuf, sizeof(codeBuf), "0x%08X", e.abortCode);
            json += "\"code\":\"" + String(codeBuf) + "\",";
            json += "\"desc\":\"" + String(decodeAbortCode(e.abortCode)) + "\",";
            json += "\"param\":" + String(e.paramId) + ",";
        }
        json += "\"opmode\":\"" + String(decodeOpmode(e.opmode)) + "\"";
        json += "}";
    }
    json += "]";
    return json;
}

void FaultLogger::clear() {
    int n = min(_count, (int)FAULTLOG_MAX_ENTRIES);
    for (int i = 0; i < n; i++) {
        char key[8];
        snprintf(key, sizeof(key), "f%d", i);
        _prefs.remove(key);
    }
    _count = 0; _startIdx = 0;
    _prefs.putInt("count", 0);
    _prefs.putInt("start", 0);
    Serial.println("[FAULT] Log cleared");
}

void FaultLogger::writeSlot(int slot, const FaultEntry& e) {
    char key[8];
    snprintf(key, sizeof(key), "f%d", slot);
    _prefs.putBytes(key, &e, sizeof(FaultEntry));
}

bool FaultLogger::readSlot(int slot, FaultEntry& e) const {
    Preferences& p = const_cast<Preferences&>(_prefs);
    char key[8];
    snprintf(key, sizeof(key), "f%d", slot);
    return p.getBytes(key, &e, sizeof(FaultEntry)) == sizeof(FaultEntry);
}

const char* FaultLogger::decodeAbortCode(uint32_t code) {
    switch (code) {
        case 0x06090030: return "Value range exceeded";
        case 0x06090011: return "Subindex does not exist";
        case 0x06010000: return "Unsupported access (read-only)";
        case 0x06010001: return "Write to read-only object";
        case 0x06010002: return "Read from write-only object";
        case 0x08000020: return "Data cannot be transferred";
        case 0x08000021: return "Local control active";
        case 0x08000022: return "Device state prevents transfer";
        case 0x05040000: return "SDO protocol timed out";
        case 0x05030000: return "Toggle bit not alternated";
        case 0x06020000: return "Object does not exist";
        case 0x06040041: return "Object cannot be mapped to PDO";
        case 0x06040042: return "PDO length exceeded";
        case 0x06040043: return "Parameter incompatibility";
        case 0x06040047: return "Internal incompatibility";
        case 0x06060000: return "Hardware error";
        case 0x06070010: return "Data type/length mismatch";
        default:         return "Unknown abort code";
    }
}

const char* FaultLogger::decodeOpmode(uint8_t opmode) {
    switch (opmode) {
        case 0: return "Off";
        case 1: return "Precharge";
        case 2: return "Run";
        case 3: return "Charge";
        case 4: return "HV_On";
        case 5: return "Prepare";
        default: return "Unknown";
    }
}
