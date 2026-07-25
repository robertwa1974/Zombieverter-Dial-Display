#pragma once
// ============================================================================
// TripLogger.h
// Logs key telemetry at 5-second intervals while the vehicle is moving
// (speed > 0). Entries live in a RAM ring buffer and are periodically
// flushed to NVS (ESP32 Non-Volatile Storage) so a power cycle doesn't lose
// more than the last sync interval's worth of data.
// Accessible via GET /log (CSV download) and DELETE /log (clear).
//
// Why RAM-buffered: writing every 5s directly to NVS flash wears out the
// ESP32's internal flash sectors (rated ~100,000 write/erase cycles) in a
// matter of months under regular driving. Buffering in RAM and syncing only
// every 10 minutes (plus on VCU disconnect) cuts flash writes by ~99%.
//
// Storage: NVS namespace "triplog"
//   Keys:  "count"  — number of valid entries (0..MAX)
//          "start"  — ring buffer start index (oldest entry slot)
//          "e0".."e199" — TripEntry structs as raw bytes
//
// Ring buffer behaviour:
//   - First 200 entries fill slots 0..199, count goes 0..200
//   - Once full, oldest slot is overwritten, start index advances
//   - CSV always returned in chronological order (oldest → newest)
//   - Capacity: 200 entries x 5s = ~16 min of continuous driving
//     After that oldest data is silently replaced as new data arrives
// ============================================================================

#include <Arduino.h>
#include <Preferences.h>

#define TRIPLOG_MAX_ENTRIES     200
#define TRIPLOG_LOG_INTERVAL_MS 5000   // log every 5 seconds
#define TRIPLOG_MIN_SPEED       10     // only log when speed > 10 RPM
#define TRIPLOG_SYNC_INTERVAL_MS 600000  // auto-sync RAM cache to NVS every 10 min

struct TripEntry {
    uint32_t timestamp_ms;   // millis() since boot
    int16_t  speed;          // RPM
    int16_t  udc;            // pack voltage x 10  (e.g. 3115 = 311.5 V)
    int16_t  idc;            // pack current x 10  (e.g. -132 = -13.2 A)
    int16_t  pwr;            // power x 100        (e.g. 180 = 1.80 kW)
    uint8_t  SOC;            // 0-100 %
    int8_t   tmphs;          // heatsink temp degC
    int8_t   tmpm;           // motor temp degC
    int16_t  potnorm;        // throttle position 0-1000 (0.0-100.0%)
};

class TripLogger {
public:
    static TripLogger& getInstance() {
        static TripLogger instance;
        return instance;
    }

    void begin();

    // Call every loop() — internally rate-limits to TRIPLOG_LOG_INTERVAL_MS
    void update(int speed_rpm,
                int udc_dv,    // volts x 10
                int idc_da,    // amps x 10
                int pwr_dkw,   // kW x 100
                int soc_pct,
                int tmphs_c,
                int tmpm_c,
                int potnorm);  // throttle 0-1000

    // Erases all entries from RAM and NVS
    void clear();

    // Force-flush the in-memory RAM cache to NVS. Called automatically every
    // TRIPLOG_SYNC_INTERVAL_MS from update(), and explicitly from main.cpp
    // when the VCU CAN link drops (ignition off) so a drive isn't lost.
    void sync();

    int  getEntryCount() const { return _count; }
    int  getCount()      const { return _count; }
    bool isFull()        const { return _count >= TRIPLOG_MAX_ENTRIES; }

    bool getEntry(int index, TripEntry& outEntry) const;
    void getCSVHeader(char* outBuf, size_t outLen) const {
        snprintf(outBuf, outLen, "row,time_s,speed_rpm,voltage_V,current_A,power_kW,SOC_pct,heatsink_C,motor_C,throttle_pct\n");
    }
    void entryToCSVRow(const TripEntry& e, int rowNum, char* outBuf, size_t outLen) const;

private:
    TripLogger() : _count(0), _startIdx(0), _lastLogTime(0), _dirty(false), _lastSyncTime(0) {
        memset(_entries, 0, sizeof(_entries));
    }
    TripLogger(const TripLogger&) = delete;
    TripLogger& operator=(const TripLogger&) = delete;

    Preferences _prefs;
    int      _count;       // number of valid entries, capped at MAX
    int      _startIdx;    // slot index of the oldest entry (ring head)
    uint32_t _lastLogTime;

    // RAM cache — all reads/writes during normal operation happen here;
    // NVS is only opened for the initial load and periodic sync() flushes.
    TripEntry _entries[TRIPLOG_MAX_ENTRIES];
    bool      _dirty;
    uint32_t  _lastSyncTime;

    void   writeSlot(int slot, const TripEntry& e);
    bool   readSlot (int slot, TripEntry& e) const;
};
