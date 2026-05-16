#pragma once
// ============================================================================
// HealthChecker.h
// Pre-drive health check — SDO polls 6 critical parameters on unlock/boot,
// compares against safe thresholds, returns a pass/fail result with detail.
// Runs asynchronously; caller checks isComplete() before reading result.
//
// Thresholds and fail behaviour are configurable via web UI and stored in NVS.
// ============================================================================

#include <Arduino.h>
#include <Preferences.h>
#include "CANData.h"

#define HEALTH_CHECK_TIMEOUT_MS  8000   // give up after 8s if SDO slow
#define HEALTH_DISPLAY_MS        5000   // show result screen for 5s

enum class HealthState {
    IDLE,
    RUNNING,
    PASS,
    FAIL
};

// What happens when a health check item fails
enum class HealthFailBehaviour : uint8_t {
    ADVISORY    = 0,  // A: always auto-advances regardless of result
    SEVERITY    = 1,  // B: warnings auto-advance, failures require button press
    ACKNOWLEDGE = 2,  // C: any warn or fail requires button press
    BLOCK       = 3,  // D: warnings require button press, failures re-lock vehicle
};

struct HealthItem {
    const char* name;
    const char* paramName;
    float       warnMin;
    float       warnMax;
    float       failMin;
    float       failMax;
    float       value;
    bool        checked;
    bool        passed;
    char        detail[32];
};

// Default threshold values — used when NVS has no saved config
struct HealthDefaults {
    static constexpr float cellDeltaWarn  =  80.0f;   // mV
    static constexpr float cellDeltaFail  = 150.0f;   // mV
    static constexpr float minCellWarn    =   3.0f;   // V
    static constexpr float minCellFail    =   2.8f;   // V
    static constexpr float motorTempWarn  =  70.0f;   // °C
    static constexpr float motorTempFail  =  85.0f;   // °C
    static constexpr float invTempWarn    =  65.0f;   // °C
    static constexpr float invTempFail    =  80.0f;   // °C
    static constexpr float packVoltWarn   = 200.0f;   // V
    static constexpr float packVoltFail   = 100.0f;   // V
};

class HealthChecker {
public:
    static HealthChecker& getInstance() {
        static HealthChecker inst;
        return inst;
    }

    void begin(CANDataManager* can);

    void startCheck();
    void update();

    HealthState getState() const { return _state; }
    bool isComplete()  const { return _state == HealthState::PASS || _state == HealthState::FAIL; }
    bool isPassed()    const { return _state == HealthState::PASS; }
    bool needsAck()      const { return _needsAck; }
    bool relockNeeded()  const { return _relockNeeded; }  // true after Block-mode hard fail ack
    void acknowledge();

    int getItemCount() const { return _itemCount; }
    const HealthItem& getItem(int i) const { return _items[i]; }

    uint32_t displayTimeRemaining() const;

    void reset();

    // NVS persistence
    void loadFromNVS();
    void saveToNVS();

    // Threshold accessors (for web UI)
    HealthFailBehaviour getFailBehaviour() const { return _failBehaviour; }
    void setFailBehaviour(HealthFailBehaviour b) { _failBehaviour = b; }

    float getCellDeltaWarn()  const { return _cellDeltaWarn; }
    float getCellDeltaFail()  const { return _cellDeltaFail; }
    float getMinCellWarn()    const { return _minCellWarn; }
    float getMinCellFail()    const { return _minCellFail; }
    float getMotorTempWarn()  const { return _motorTempWarn; }
    float getMotorTempFail()  const { return _motorTempFail; }
    float getInvTempWarn()    const { return _invTempWarn; }
    float getInvTempFail()    const { return _invTempFail; }
    float getPackVoltWarn()   const { return _packVoltWarn; }
    float getPackVoltFail()   const { return _packVoltFail; }

    void setCellDeltaWarn(float v)  { _cellDeltaWarn = v; }
    void setCellDeltaFail(float v)  { _cellDeltaFail = v; }
    void setMinCellWarn(float v)    { _minCellWarn = v; }
    void setMinCellFail(float v)    { _minCellFail = v; }
    void setMotorTempWarn(float v)  { _motorTempWarn = v; }
    void setMotorTempFail(float v)  { _motorTempFail = v; }
    void setInvTempWarn(float v)    { _invTempWarn = v; }
    void setInvTempFail(float v)    { _invTempFail = v; }
    void setPackVoltWarn(float v)   { _packVoltWarn = v; }
    void setPackVoltFail(float v)   { _packVoltFail = v; }

private:
    HealthChecker() : _can(nullptr), _state(HealthState::IDLE),
                      _currentItem(0), _startTime(0), _completeTime(0),
                      _itemCount(0), _needsAck(false), _relockNeeded(false),
                      _failBehaviour(HealthFailBehaviour::SEVERITY),
                      _cellDeltaWarn(HealthDefaults::cellDeltaWarn),
                      _cellDeltaFail(HealthDefaults::cellDeltaFail),
                      _minCellWarn(HealthDefaults::minCellWarn),
                      _minCellFail(HealthDefaults::minCellFail),
                      _motorTempWarn(HealthDefaults::motorTempWarn),
                      _motorTempFail(HealthDefaults::motorTempFail),
                      _invTempWarn(HealthDefaults::invTempWarn),
                      _invTempFail(HealthDefaults::invTempFail),
                      _packVoltWarn(HealthDefaults::packVoltWarn),
                      _packVoltFail(HealthDefaults::packVoltFail) {}
    HealthChecker(const HealthChecker&) = delete;
    HealthChecker& operator=(const HealthChecker&) = delete;

    CANDataManager*     _can;
    HealthState         _state;
    int                 _currentItem;
    uint32_t            _startTime;
    uint32_t            _completeTime;
    bool                _needsAck;
    bool                _relockNeeded;
    HealthFailBehaviour _failBehaviour;

    // Configurable thresholds
    float _cellDeltaWarn, _cellDeltaFail;
    float _minCellWarn,   _minCellFail;
    float _motorTempWarn, _motorTempFail;
    float _invTempWarn,   _invTempFail;
    float _packVoltWarn,  _packVoltFail;

    static const int MAX_HEALTH_ITEMS = 8;
    HealthItem _items[MAX_HEALTH_ITEMS];
    int        _itemCount;

    void _buildItems();
    void _evaluateItem(HealthItem& item);
    bool _allPassed();
    bool _anyFailed();
    bool _anyWarned();
    void _applyFailBehaviour();
};
