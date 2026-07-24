// ============================================================================
// TripLogger.cpp — RING BUFFER VERSION
// ============================================================================

#include "TripLogger.h"

// ---------------------------------------------------------------------------
// begin() — call once in setup() after Serial is ready
// ---------------------------------------------------------------------------
void TripLogger::begin() {
    _prefs.begin("triplog", false);
    _count    = _prefs.getInt("count", 0);
    _startIdx = _prefs.getInt("start", 0);

    // Sanity-check stored values
    if (_count    < 0 || _count    > TRIPLOG_MAX_ENTRIES) _count    = 0;
    if (_startIdx < 0 || _startIdx >= TRIPLOG_MAX_ENTRIES) _startIdx = 0;

    Serial.printf("[TRIPLOG] Loaded %d entries, ring start=%d, buffer %s\n",
                  _count, _startIdx, isFull() ? "FULL (looping)" : "not full");
}

// ---------------------------------------------------------------------------
// update() — call every loop(); internally rate-limits to 5s while moving
// ---------------------------------------------------------------------------
void TripLogger::update(int speed_rpm, int udc_dv, int idc_da,
                        int pwr_dkw,  int soc_pct, int tmphs_c,
                        int tmpm_c,   int potnorm) {
    uint32_t now = millis();

    if (speed_rpm < TRIPLOG_MIN_SPEED) return;
    if ((now - _lastLogTime) < TRIPLOG_LOG_INTERVAL_MS) return;
    _lastLogTime = now;

    TripEntry e;
    e.timestamp_ms = now;
    e.speed   = (int16_t)constrain(speed_rpm, -32768, 32767);
    e.udc     = (int16_t)constrain(udc_dv,   -32768, 32767);
    e.idc     = (int16_t)constrain(idc_da,   -32768, 32767);
    e.pwr     = (int16_t)constrain(pwr_dkw,  -32768, 32767);
    e.SOC     = (uint8_t)constrain(soc_pct,       0,   100);
    e.tmphs   = (int8_t) constrain(tmphs_c,     -128,   127);
    e.tmpm    = (int8_t) constrain(tmpm_c,      -128,   127);
    e.potnorm = (int16_t)constrain(potnorm,        0,  1000);

    if (_count < TRIPLOG_MAX_ENTRIES) {
        // Buffer not yet full — write to next sequential slot
        writeSlot(_count, e);
        _count++;
        _prefs.putInt("count", _count);
        Serial.printf("[TRIPLOG] Entry %d/%d: %drpm %.1fV %.1fA %d%% thr=%.1f%%\n",
                      _count, TRIPLOG_MAX_ENTRIES,
                      speed_rpm, udc_dv / 10.0f, idc_da / 10.0f, soc_pct, potnorm / 10.0f);
    } else {
        // Buffer full — overwrite oldest slot (ring head), then advance head
        writeSlot(_startIdx, e);
        _startIdx = (_startIdx + 1) % TRIPLOG_MAX_ENTRIES;
        _prefs.putInt("start", _startIdx);
        Serial.printf("[TRIPLOG] Ring overwrite, new start=%d: %drpm %.1fV %.1fA %d%% thr=%.1f%%\n",
                      _startIdx, speed_rpm, udc_dv / 10.0f, idc_da / 10.0f, soc_pct, potnorm / 10.0f);
    }
}

// ---------------------------------------------------------------------------
// getEntry() — retrieves a single entry in chronological order
// ---------------------------------------------------------------------------
bool TripLogger::getEntry(int index, TripEntry& outEntry) const {
    if (index < 0 || index >= _count) return false;
    int slot = index;
    if (_count >= TRIPLOG_MAX_ENTRIES) {
        slot = (_startIdx + index) % TRIPLOG_MAX_ENTRIES;
    }
    return readSlot(slot, outEntry);
}

// ---------------------------------------------------------------------------
// clear() — wipe all entries from NVS
// ---------------------------------------------------------------------------
void TripLogger::clear() {
    _prefs.begin("triplog", false);
    int countToClear = (_count < TRIPLOG_MAX_ENTRIES) ? _count : TRIPLOG_MAX_ENTRIES;
    for (int i = 0; i < countToClear; i++) {
        char key[8];
        snprintf(key, sizeof(key), "e%d", i);
        _prefs.remove(key);
    }
    _count    = 0;
    _startIdx = 0;
    _prefs.putInt("count", 0);
    _prefs.putInt("start", 0);
    Serial.println("[TRIPLOG] Log cleared");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void TripLogger::writeSlot(int slot, const TripEntry& e) {
    char key[8];
    snprintf(key, sizeof(key), "e%d", slot);
    _prefs.putBytes(key, &e, sizeof(TripEntry));
}

bool TripLogger::readSlot(int slot, TripEntry& e) const {
    // Preferences::getBytes needs a non-const instance — cast away const
    // for the read (NVS read is logically const even if API isn't)
    Preferences& p = const_cast<Preferences&>(_prefs);
    char key[8];
    snprintf(key, sizeof(key), "e%d", slot);
    size_t len = p.getBytes(key, &e, sizeof(TripEntry));
    return len == sizeof(TripEntry);
}

void TripLogger::entryToCSVRow(const TripEntry& e, int rowNum, char* outBuf, size_t outLen) const {
    if (!outBuf || outLen == 0) return;
    snprintf(outBuf, outLen,
        "%d,%.1f,%d,%.1f,%.1f,%.2f,%d,%d,%d,%.1f\n",
        rowNum,
        e.timestamp_ms / 1000.0f,
        (int)e.speed,
        e.udc     / 10.0f,
        e.idc     / 10.0f,
        e.pwr     / 100.0f,
        (int)e.SOC,
        (int)e.tmphs,
        (int)e.tmpm,
        e.potnorm / 10.0f
    );
}
