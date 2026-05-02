#pragma once
// ============================================================================
// TripLogger.h
// Logs key telemetry to NVS (ESP32 Non-Volatile Storage) at 5-second intervals
// while the vehicle is moving (speed > 0). Survives power cycles.
// Accessible via GET /log (CSV download) and DELETE /log (clear).
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

    // Returns CSV string (header + all entries, chronological order)
    String getCSV();

    // Erases all entries from NVS
    void clear();

    int  getEntryCount() const { return _count; }
    bool isFull()        const { return _count >= TRIPLOG_MAX_ENTRIES; }

private:
    TripLogger() : _count(0), _startIdx(0), _lastLogTime(0) {}
    TripLogger(const TripLogger&) = delete;
    TripLogger& operator=(const TripLogger&) = delete;

    Preferences _prefs;
    int      _count;       // number of valid entries, capped at MAX
    int      _startIdx;    // slot index of the oldest entry (ring head)
    uint32_t _lastLogTime;

    void   writeSlot(int slot, const TripEntry& e);
    bool   readSlot (int slot, TripEntry& e) const;
    String entryToCSVRow(const TripEntry& e, int rowNum) const;
};
