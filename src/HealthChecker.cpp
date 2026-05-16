// ============================================================================
// HealthChecker.cpp
// ============================================================================

#include "HealthChecker.h"
#include <math.h>

static const char* NVS_NS = "healthcfg";

void HealthChecker::begin(CANDataManager* can) {
    _can = can;
    _state = HealthState::IDLE;
    loadFromNVS();
}

void HealthChecker::loadFromNVS() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    _failBehaviour  = (HealthFailBehaviour)prefs.getUChar("failBehav", (uint8_t)HealthFailBehaviour::SEVERITY);
    _cellDeltaWarn  = prefs.getFloat("cdWarn",  HealthDefaults::cellDeltaWarn);
    _cellDeltaFail  = prefs.getFloat("cdFail",  HealthDefaults::cellDeltaFail);
    _minCellWarn    = prefs.getFloat("mcWarn",  HealthDefaults::minCellWarn);
    _minCellFail    = prefs.getFloat("mcFail",  HealthDefaults::minCellFail);
    _motorTempWarn  = prefs.getFloat("mtWarn",  HealthDefaults::motorTempWarn);
    _motorTempFail  = prefs.getFloat("mtFail",  HealthDefaults::motorTempFail);
    _invTempWarn    = prefs.getFloat("itWarn",  HealthDefaults::invTempWarn);
    _invTempFail    = prefs.getFloat("itFail",  HealthDefaults::invTempFail);
    _packVoltWarn   = prefs.getFloat("pvWarn",  HealthDefaults::packVoltWarn);
    _packVoltFail   = prefs.getFloat("pvFail",  HealthDefaults::packVoltFail);
    prefs.end();
    Serial.println("[HEALTH] Thresholds loaded from NVS");
}

void HealthChecker::saveToNVS() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putUChar("failBehav", (uint8_t)_failBehaviour);
    prefs.putFloat("cdWarn",  _cellDeltaWarn);
    prefs.putFloat("cdFail",  _cellDeltaFail);
    prefs.putFloat("mcWarn",  _minCellWarn);
    prefs.putFloat("mcFail",  _minCellFail);
    prefs.putFloat("mtWarn",  _motorTempWarn);
    prefs.putFloat("mtFail",  _motorTempFail);
    prefs.putFloat("itWarn",  _invTempWarn);
    prefs.putFloat("itFail",  _invTempFail);
    prefs.putFloat("pvWarn",  _packVoltWarn);
    prefs.putFloat("pvFail",  _packVoltFail);
    prefs.end();
    Serial.println("[HEALTH] Thresholds saved to NVS");
}

void HealthChecker::startCheck() {
    if (!_can) return;
    _needsAck     = false;
    _relockNeeded = false;
    _buildItems();
    _currentItem = 0;
    _startTime   = millis();
    _state       = HealthState::RUNNING;
    Serial.println("[HEALTH] Pre-drive check started");
}

void HealthChecker::_buildItems() {
    _itemCount = 0;
    auto add = [&](const char* name, const char* param,
                   float wMin, float wMax, float fMin, float fMax) {
        if (_itemCount >= MAX_HEALTH_ITEMS) return;
        HealthItem& it = _items[_itemCount++];
        it.name = name; it.paramName = param;
        it.warnMin = wMin; it.warnMax = wMax;
        it.failMin = fMin; it.failMax = fMax;
        it.value = 0; it.checked = false; it.passed = true; it.detail[0] = '\0';
    };
    add("Cell Delta",    "BMS_Vmax",  NAN,           _cellDeltaWarn, NAN,           _cellDeltaFail);
    add("Min Cell V",    "BMS_Vmin",  _minCellWarn,  NAN,            _minCellFail,  NAN);
    add("Motor Temp",    "tmpm",      NAN,           _motorTempWarn, NAN,           _motorTempFail);
    add("Inverter Temp", "tmphs",     NAN,           _invTempWarn,   NAN,           _invTempFail);
    add("Pack Voltage",  "udc",       _packVoltWarn, NAN,            _packVoltFail, NAN);
}

void HealthChecker::update() {
    if (_state != HealthState::RUNNING) return;
    if (_needsAck) return;

    if (millis() - _startTime > HEALTH_CHECK_TIMEOUT_MS) {
        for (int i = 0; i < _itemCount; i++) {
            if (!_items[i].checked) {
                _items[i].checked = true;
                snprintf(_items[i].detail, sizeof(_items[i].detail), "No data");
            }
        }
        _applyFailBehaviour();
        return;
    }

    if (_currentItem >= _itemCount) {
        _applyFailBehaviour();
        return;
    }

    HealthItem& item = _items[_currentItem];
    CANParameter* p = _can->getParameterByName(item.paramName);
    if (p && (millis() - p->lastUpdateTime) < 5000) {
        item.value = (float)p->getValueAsInt();
        if (strcmp(item.paramName, "BMS_Vmax") == 0) {
            CANParameter* pMin = _can->getParameterByName("BMS_Vmin");
            if (pMin) {
                item.value   = item.value - (float)pMin->getValueAsInt();
                item.warnMax = _cellDeltaWarn;
                item.failMax = _cellDeltaFail;
                item.name    = "Cell Delta";
            }
        }
        _evaluateItem(item);
        item.checked = true;
        _currentItem++;
    }
}

void HealthChecker::_evaluateItem(HealthItem& item) {
    float v = item.value;
    item.passed = true;
    if      (!isnan(item.failMin) && v < item.failMin)
        { item.passed = false; snprintf(item.detail, sizeof(item.detail), "%.1f < %.1f FAIL", v, item.failMin); }
    else if (!isnan(item.failMax) && v > item.failMax)
        { item.passed = false; snprintf(item.detail, sizeof(item.detail), "%.1f > %.1f FAIL", v, item.failMax); }
    else if (!isnan(item.warnMin) && v < item.warnMin)
        { snprintf(item.detail, sizeof(item.detail), "%.1f < %.1f WARN", v, item.warnMin); }
    else if (!isnan(item.warnMax) && v > item.warnMax)
        { snprintf(item.detail, sizeof(item.detail), "%.1f > %.1f WARN", v, item.warnMax); }
    else
        { snprintf(item.detail, sizeof(item.detail), "%.1f OK", v); }
    Serial.printf("[HEALTH] %s: %s\n", item.name, item.detail);
}

bool HealthChecker::_allPassed() {
    for (int i = 0; i < _itemCount; i++) if (!_items[i].passed) return false;
    return true;
}
bool HealthChecker::_anyFailed() {
    for (int i = 0; i < _itemCount; i++) if (!_items[i].passed) return true;
    return false;
}
bool HealthChecker::_anyWarned() {
    for (int i = 0; i < _itemCount; i++)
        if (_items[i].passed && strstr(_items[i].detail, "WARN")) return true;
    return false;
}

void HealthChecker::_applyFailBehaviour() {
    bool failed = _anyFailed();
    bool warned = _anyWarned();
    _relockNeeded = false;

    switch (_failBehaviour) {
        case HealthFailBehaviour::ADVISORY:
            // A: always auto-advances, no interaction
            _state    = HealthState::PASS;
            _needsAck = false;
            break;

        case HealthFailBehaviour::SEVERITY:
            // B: warnings auto-advance, failures require button press
            _state    = failed ? HealthState::FAIL : HealthState::PASS;
            _needsAck = failed;
            break;

        case HealthFailBehaviour::ACKNOWLEDGE:
            // C: any warn or fail requires button press
            _state    = failed ? HealthState::FAIL : HealthState::PASS;
            _needsAck = (failed || warned);
            break;

        case HealthFailBehaviour::BLOCK:
            // D: warnings require button press; failures re-lock vehicle
            _state        = failed ? HealthState::FAIL : HealthState::PASS;
            _needsAck     = true;   // always wait for button
            _relockNeeded = failed; // main.cpp calls immobilizer.lock() after ack
            break;
    }

    if (!_needsAck) _completeTime = millis();
    Serial.printf("[HEALTH] Complete: %s  needsAck=%d  relock=%d  behaviour=%d\n",
        _state == HealthState::PASS ? "PASS" : "FAIL",
        (int)_needsAck, (int)_relockNeeded, (int)_failBehaviour);
}

void HealthChecker::acknowledge() {
    if (!_needsAck) return;
    _needsAck = false;
    _completeTime = millis();
    Serial.println("[HEALTH] Acknowledged by user");
}

uint32_t HealthChecker::displayTimeRemaining() const {
    if (_needsAck) return HEALTH_DISPLAY_MS;
    if (!isComplete()) return HEALTH_DISPLAY_MS;
    uint32_t elapsed = millis() - _completeTime;
    return elapsed >= HEALTH_DISPLAY_MS ? 0 : HEALTH_DISPLAY_MS - elapsed;
}

void HealthChecker::reset() {
    _state = HealthState::IDLE; _currentItem = 0; _itemCount = 0; _needsAck = false; _relockNeeded = false;
}
