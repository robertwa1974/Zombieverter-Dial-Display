#pragma once
// ============================================================================
// EfficiencyTracker.h
// Calculates real-time Wh/km efficiency from IVT-S power and ZombieVerter
// speed. Uses a rolling 30-second energy/distance window for stability.
// Also tracks session totals for the charging and driving screens.
// ============================================================================

#include <Arduino.h>

#define EFFICIENCY_WINDOW_S   30     // rolling average window in seconds
#define EFFICIENCY_SAMPLES    30     // one sample per second

class EfficiencyTracker {
public:
    static EfficiencyTracker& getInstance() {
        static EfficiencyTracker inst;
        return inst;
    }

    void begin();

    // Call every second with current power (W) and speed (RPM)
    // wheelCircumference_m: wheel circumference in metres (set once at init)
    void update(float powerW, int speedRPM);

    // Set motor-to-wheel ratio and wheel circumference for speed->distance conversion
    // finalDriveRatio: e.g. 4.1 for 4.1:1 final drive
    // wheelCircumference_m: e.g. 1.88 for 205/55R16
    void setDriveParams(float finalDriveRatio, float wheelCircumference_m);

    // Returns instantaneous Wh/km from rolling window (NAN if insufficient data)
    float getWhPerKm() const;

    // Returns km/kWh (NAN if insufficient data or zero)
    float getKmPerKWh() const;

    // Session totals since last reset
    float getSessionEnergyWh() const { return _sessionEnergyWh; }
    float getSessionDistanceKm() const { return _sessionDistanceKm; }

    // Charging session: energy added since charge mode started (Wh)
    float getChargeEnergyWh() const { return _chargeEnergyWh; }
    void startChargeSession();
    void endChargeSession();

    void resetSession();

private:
    EfficiencyTracker() :
        _finalDrive(4.1f), _wheelCirc(1.88f),
        _head(0), _sampleCount(0),
        _sessionEnergyWh(0), _sessionDistanceKm(0),
        _chargeEnergyWh(0), _charging(false),
        _lastUpdateMs(0) {}

    EfficiencyTracker(const EfficiencyTracker&) = delete;
    EfficiencyTracker& operator=(const EfficiencyTracker&) = delete;

    float    _finalDrive;
    float    _wheelCirc;

    // Rolling window — one entry per second
    float    _powerW[EFFICIENCY_SAMPLES];    // W
    float    _distM[EFFICIENCY_SAMPLES];     // metres
    int      _head;
    int      _sampleCount;

    float    _sessionEnergyWh;
    float    _sessionDistanceKm;
    float    _chargeEnergyWh;
    bool     _charging;
    uint32_t _lastUpdateMs;
};
