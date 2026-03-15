# ZombieVerter Dial Display — Live Values Implementation

## Overview

This document describes the work done to achieve live parameter values on both the M5Stack Dial display and the web UI served over WiFi.

---

## Problem Statement

The v2.0 baseline had:
- Static values on the dial display (read from `params.json` at boot)
- Static values in the web UI (`/cmd?cmd=json` served `params.json` unchanged)
- No live CAN data visible in either interface

---

## Architecture

### Parameter Storage

All live values are stored in a single in-memory array:

```cpp
CANParameter parameters[MAX_PARAMETERS];  // in CANDataManager
```

Each entry holds: `id`, `name`, `valueInt`, `lastUpdateTime`, `editable`, `fromBroadcast`.

Both the dial display and web UI read from this same array — there is no separate data path.

### Value Sources

Parameters are updated from two sources:

**1. CAN Broadcasts** (`handleGenericMessage` in `CANData.cpp`)

These frames arrive continuously and update values directly:

| CAN ID | Parameters |
|--------|-----------|
| 0x355  | SOC |
| 0x356  | tmpm |
| 0x522  | udc (IVT-S shunt) |
| 0x411  | idc (IVT-S shunt) |
| 0x126  | tmphs |
| 0x257  | speed |
| 0x373  | BMS_Vmin, BMS_Vmax |

**2. SDO Polling** (round-robin via `SDOManager` FreeRTOS task)

All other parameters are polled via CANopen SDO protocol. The `SDOManager` runs on core 0 as a FreeRTOS task, queuing requests and processing responses asynchronously without blocking the main loop.

---

## Key Changes

### SDOManager — 16-bit Param ID Support

The original `SDOManager` only supported `uint8_t` param IDs (1–255). ZombieVerter VCU spot values use IDs 2000+ (e.g. `opmode` = 2002, `udc` = 2006).

**Fix:** Changed `SDORequest`, `SDOResult`, and all related functions to use `uint16_t`. The SDO index/subindex calculation now correctly handles the full range:

```cpp
uint16_t index    = 0x2100 | (paramId >> 8);
uint8_t  subindex = paramId & 0xFF;
```

For backward compatibility:
- Param ID 27 (Gear): `index=0x2100`, `subindex=0x1B` — unchanged
- Param ID 2006 (udc): `index=0x2107`, `subindex=0xD6` — new

### SDO Scaling

All SDO responses use ZombieVerter fixed-point ×32 encoding. The callback divides by 32:

```cpp
instance->updateParameterBySDOId(result.paramId, result.value / 32);
```

### Broadcast Protection

CAN broadcast params must NOT be overwritten by SDO polling — broadcast values are raw integers, SDO values are ×32. A hardcoded skip list in `requestParameter()` prevents SDO polling for known broadcast param IDs:

```cpp
static const uint16_t broadcastIds[] = {
    2006,  // udc
    2012,  // idc
    2015,  // SOC
    2016,  // speed
    2028,  // tmphs
    2029,  // tmpm
    2084,  // BMS_Vmin
    2085,  // BMS_Vmax
    2070,  // U12V
    0
};
```

### Web UI — `/spot` Endpoint

A new `/spot` HTTP endpoint was added to `WiFiManager`. It builds a small JSON object (~2KB) from the in-memory `parameters[]` array — no SPIFFS, no JSON library, no allocation:

```cpp
// Example response:
{"speed":0,"udc":309,"SOC":99,"tmphs":18,"tmpm":20,"opmode":0,...}
```

### Web UI — `inverter.js` Patch

`getParamList()` in `inverter.js` was modified to fetch `/spot` after `cmd=json` and merge live values into the params before the UI renders:

```javascript
inverter.fetchSpotValues(params, replyFunc);
```

If `/spot` fails, it gracefully falls back to the static values from `params.json`.

### UIManager — Name-Based Lookup

The dial display previously used `getParameter(N)` with old short IDs (2, 3, 7, 8 etc.). After loading `params.json`, these IDs no longer exist — all spot values use IDs 2000+.

**Fix:** All `getParameter(N)` calls replaced with `getParameterByName("name")`:

```cpp
// Before
CANParameter* rpm = canManager->getParameter(2);

// After
CANParameter* rpm = canManager->getParameterByName("speed");
```

---

## SDO Polling in WiFi Mode

The main loop SDO polling was originally disabled in WiFi mode. This meant spot values stopped updating when the web UI was open.

**Fix:** SDO round-robin polling now runs in both normal mode and WiFi mode:

```cpp
if (wifiMode) {
    canManager.update();
    // SDO polling continues in WiFi mode
    if (millis() - lastParamRequestTime > PARAM_UPDATE_INTERVAL_MS) {
        ...
        canManager.requestParameter(param->id);
    }
}
```

---

## Files Changed

| File | Change |
|------|--------|
| `include/SDOManager.h` | `uint8_t` → `uint16_t` for paramId throughout |
| `src/SDOManager.cpp` | 16-bit index/subindex calculation in `sendFrame()`, `handleFrame()` |
| `src/CANData.cpp` | Remove `>= 1000` guard; broadcast skip list; always divide SDO by 32 |
| `src/CANData.h` | Add `fromBroadcast` flag to `CANParameter` |
| `src/WiFiManager.cpp` | Add `/spot` endpoint and `handleSpot()` |
| `src/WiFiManager.h` | Declare `handleSpot()` |
| `src/main.cpp` | SDO polling in WiFi mode; WiFi screen sequence fix |
| `src/UIManager.cpp` | All parameter lookups by name; `updateSettings()`; WiFi screen layout |
| `data/inverter.js` | `fetchSpotValues()` added to `getParamList()` |
