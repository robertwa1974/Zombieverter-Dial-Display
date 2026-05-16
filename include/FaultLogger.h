#pragma once
// ============================================================================
// FaultLogger.h
// Stores ZombieVerter SDO fault codes to NVS with timestamps.
// ZombieVerter broadcasts fault codes via SDO abort frames (0x583, cmd=0x80).
// Abort codes: 0x06090030=value range, 0x06010000=read-only, 0x08000022=device state etc.
// Also stores opmode transitions so you can see "went to charge at T+14:32".
//
// NVS namespace: "faultlog"
// Keys: "count", "f0".."f{N-1}"
// Capacity: FAULTLOG_MAX_ENTRIES ring buffer
// ============================================================================

#include <Arduino.h>
#include <Preferences.h>

#define FAULTLOG_MAX_ENTRIES  50

struct FaultEntry {
    uint32_t timestamp_ms;   // millis() since boot
    uint32_t abortCode;      // SDO abort code (0x08xxxxxx etc.) or 0 for opmode change
    uint16_t paramId;        // parameter subindex that triggered the fault (0 for opmode)
    uint8_t  opmode;         // opmode value at time of fault (or new opmode if opmode change)
    uint8_t  isFault;        // 1 = SDO abort, 0 = opmode transition
};

class FaultLogger {
public:
    static FaultLogger& getInstance() {
        static FaultLogger inst;
        return inst;
    }

    void begin();

    // Log an SDO abort (call when 0x583 cmd=0x80 received)
    void logFault(uint32_t abortCode, uint16_t paramId, uint8_t currentOpmode);

    // Log an opmode transition
    void logOpmodeChange(uint8_t newOpmode);

    // Returns JSON array string of all entries, newest first
    String getJSON();

    void clear();
    int getCount() const { return _count; }

    // Decode known abort codes to human-readable string
    static const char* decodeAbortCode(uint32_t code);
    static const char* decodeOpmode(uint8_t opmode);

private:
    FaultLogger() : _count(0), _startIdx(0) {}
    FaultLogger(const FaultLogger&) = delete;
    FaultLogger& operator=(const FaultLogger&) = delete;

    Preferences _prefs;
    int _count;
    int _startIdx;

    void writeSlot(int slot, const FaultEntry& e);
    bool readSlot(int slot, FaultEntry& e) const;
    void push(const FaultEntry& e);
};
