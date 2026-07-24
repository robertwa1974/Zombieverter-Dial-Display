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

bool FaultLogger::getEntry(int index, FaultEntry& outEntry) const {
    int total = min(_count, (int)FAULTLOG_MAX_ENTRIES);
    if (index < 0 || index >= total) return false;

    int i = total - 1 - index;
    int slot = i;
    if (_count >= FAULTLOG_MAX_ENTRIES) {
        slot = (_startIdx + i) % FAULTLOG_MAX_ENTRIES;
    }
    return readSlot(slot, outEntry);
}

void FaultLogger::entryToJSON(const FaultEntry& e, int index, char* outBuf, size_t outLen) const {
    if (!outBuf || outLen == 0) return;
    float t_s = e.timestamp_ms / 1000.0f;
    if (e.isFault) {
        snprintf(outBuf, outLen,
                 "{\"t\":%.1f,\"type\":\"fault\",\"code\":\"0x%08X\",\"desc\":\"%s\",\"param\":%u,\"opmode\":\"%s\"}",
                 t_s, e.abortCode, decodeAbortCode(e.abortCode), e.paramId, decodeOpmode(e.opmode));
    } else {
        snprintf(outBuf, outLen,
                 "{\"t\":%.1f,\"type\":\"opmode\",\"opmode\":\"%s\"}",
                 t_s, decodeOpmode(e.opmode));
    }
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
