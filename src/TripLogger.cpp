// ============================================================================
// TripLogger.cpp — RAM-BUFFERED RING BUFFER VERSION
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

    // Load existing logged entries from flash into RAM
    memset(_entries, 0, sizeof(_entries));
    for (int i = 0; i < _count; i++) {
        char key[8];
        snprintf(key, sizeof(key), "e%d", i);
        _prefs.getBytes(key, &_entries[i], sizeof(TripEntry));
    }
    _prefs.end(); // Keep NVS closed to prevent continuous open/write states

    _dirty = false;
    _lastSyncTime = millis();

    Serial.printf("[TRIPLOG] Loaded %d entries from NVS to RAM, start=%d, %s\n",
                  _count, _startIdx, isFull() ? "FULL" : "not full");
}

// ---------------------------------------------------------------------------
// update() — call every loop(); rate-limits to 5s and buffers in RAM
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
        // Buffer not yet full — write to next sequential slot in RAM
        _entries[_count] = e;
        _count++;
        _dirty = true;
        Serial.printf("[TRIPLOG] Buffered entry %d/%d (RAM): %drpm %.1fV %.1fA %d%%\n",
                      _count, TRIPLOG_MAX_ENTRIES,
                      speed_rpm, udc_dv / 10.0f, idc_da / 10.0f, soc_pct);
    } else {
        // Buffer full — overwrite oldest slot in RAM, advance start index
        _entries[_startIdx] = e;
        _startIdx = (_startIdx + 1) % TRIPLOG_MAX_ENTRIES;
        _dirty = true;
        Serial.printf("[TRIPLOG] RAM buffer overwrite at %d: %drpm %.1fV %.1fA %d%%\n",
                      _startIdx, speed_rpm, udc_dv / 10.0f, idc_da / 10.0f, soc_pct);
    }

    // Safely auto-sync to flash every 10 minutes to protect against accidental power cut
    if (now - _lastSyncTime >= 600000) {
        sync();
    }
}

// ---------------------------------------------------------------------------
// sync() — flushes the in-memory RAM buffer to NVS flash if dirty
// ---------------------------------------------------------------------------
void TripLogger::sync() {
    if (!_dirty) return;

    Serial.println("[TRIPLOG] Syncing RAM cache to NVS...");
    _prefs.begin("triplog", false);
    _prefs.putInt("count", _count);
    _prefs.putInt("start", _startIdx);

    int countToSync = (_count < TRIPLOG_MAX_ENTRIES) ? _count : TRIPLOG_MAX_ENTRIES;
    for (int i = 0; i < countToSync; i++) {
        char key[8];
        snprintf(key, sizeof(key), "e%d", i);
        _prefs.putBytes(key, &_entries[i], sizeof(TripEntry));
    }
    _prefs.end();

    _dirty = false;
    _lastSyncTime = millis();
    Serial.println("[TRIPLOG] Sync complete");
}

// ---------------------------------------------------------------------------
// getEntry() — retrieves a single entry from RAM in chronological order
// ---------------------------------------------------------------------------
bool TripLogger::getEntry(int index, TripEntry& outEntry) const {
    if (index < 0 || index >= _count) return false;
    int slot = index;
    if (_count >= TRIPLOG_MAX_ENTRIES) {
        slot = (_startIdx + index) % TRIPLOG_MAX_ENTRIES;
    }
    outEntry = _entries[slot];
    return true;
}

// ---------------------------------------------------------------------------
// clear() — wipe all entries from RAM and NVS
// ---------------------------------------------------------------------------
void TripLogger::clear() {
    _prefs.begin("triplog", false);
    int countToClear = (_count < TRIPLOG_MAX_ENTRIES) ? _count : TRIPLOG_MAX_ENTRIES;
    for (int i = 0; i < countToClear; i++) {
        char key[8];
        snprintf(key, sizeof(key), "e%d", i);
        _prefs.remove(key);
    }
    _prefs.putInt("count", 0);
    _prefs.putInt("start", 0);
    _prefs.end();

    memset(_entries, 0, sizeof(_entries));
    _count    = 0;
    _startIdx = 0;
    _dirty    = false;
    _lastSyncTime = millis();

    Serial.println("[TRIPLOG] RAM and NVS Log cleared");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void TripLogger::writeSlot(int slot, const TripEntry& e) {
    // Legacy helper - no-op since we write directly to _entries array
    if (slot >= 0 && slot < TRIPLOG_MAX_ENTRIES) {
        _entries[slot] = e;
        _dirty = true;
    }
}

bool TripLogger::readSlot(int slot, TripEntry& e) const {
    // Legacy helper - reads from RAM cache
    if (slot >= 0 && slot < TRIPLOG_MAX_ENTRIES) {
        e = _entries[slot];
        return true;
    }
    return false;
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
