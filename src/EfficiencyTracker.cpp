// ============================================================================
// EfficiencyTracker.cpp
// ============================================================================

#include "EfficiencyTracker.h"
#include <math.h>

void EfficiencyTracker::begin() {
    memset(_powerW, 0, sizeof(_powerW));
    memset(_distM,  0, sizeof(_distM));
    _head = 0; _sampleCount = 0;
    _sessionEnergyWh = 0; _sessionDistanceKm = 0;
    _chargeEnergyWh  = 0; _charging = false;
    _lastUpdateMs = millis();
}

void EfficiencyTracker::setDriveParams(float finalDriveRatio, float wheelCircumference_m) {
    _finalDrive = finalDriveRatio;
    _wheelCirc  = wheelCircumference_m;
}

void EfficiencyTracker::update(float powerW, int speedRPM) {
    uint32_t now = millis();
    float dt_s = (now - _lastUpdateMs) / 1000.0f;
    if (dt_s < 0.5f) return;  // called too fast — wait
    _lastUpdateMs = now;

    // Convert motor RPM to wheel speed (m/s)
    // wheel_rps = motor_rps / finalDrive
    // speed_ms  = wheel_rps * circumference
    float motorRPS  = speedRPM / 60.0f;
    float speedMS   = (motorRPS / _finalDrive) * _wheelCirc;
    if (speedMS < 0) speedMS = 0;

    float distM   = speedMS * dt_s;
    float energyJ = powerW * dt_s;

    // Update rolling window
    _powerW[_head] = powerW;
    _distM[_head]  = distM;
    _head = (_head + 1) % EFFICIENCY_SAMPLES;
    if (_sampleCount < EFFICIENCY_SAMPLES) _sampleCount++;

    // Session totals
    _sessionEnergyWh   += energyJ / 3600.0f;
    _sessionDistanceKm += distM / 1000.0f;

    // Charging session energy
    if (_charging && powerW < 0) {  // negative power = charging
        _chargeEnergyWh += (-energyJ) / 3600.0f;
    }
}

float EfficiencyTracker::getWhPerKm() const {
    if (_sampleCount < 3) return NAN;

    float totalEnergyWh = 0;
    float totalDistKm   = 0;

    for (int i = 0; i < _sampleCount; i++) {
        totalEnergyWh += _powerW[i] / 3600.0f;  // W * 1s / 3600 = Wh
        totalDistKm   += _distM[i] / 1000.0f;
    }

    if (totalDistKm < 0.001f) return NAN;
    return totalEnergyWh / totalDistKm;
}

float EfficiencyTracker::getKmPerKWh() const {
    float wh = getWhPerKm();
    if (isnan(wh) || wh <= 0) return NAN;
    return 1000.0f / wh;
}

void EfficiencyTracker::startChargeSession() {
    _charging = true;
    _chargeEnergyWh = 0;
}

void EfficiencyTracker::endChargeSession() {
    _charging = false;
}

void EfficiencyTracker::resetSession() {
    _sessionEnergyWh   = 0;
    _sessionDistanceKm = 0;
    _chargeEnergyWh    = 0;
}
