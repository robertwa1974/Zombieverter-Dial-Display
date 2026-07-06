# ZombieVerter BLE Parameter Bus — Protocol Specification v1.0

## Purpose

A small, generic BLE GATT protocol for reading and writing a bounded subset
of EV conversion parameters (SOC, pack voltage, motor temp, regen level,
etc.) from a phone or other BLE central, **without** requiring one GATT
characteristic per parameter. A client app built against this spec should
work against any peripheral that implements it — not just the specific
ZombieVerter Dial Display firmware it was first built for.

This is deliberately *not* a full replacement for a web UI or CAN
diagnostic tool. It exposes a small, curated, safe subset of values —
similar in spirit to what you'd see on a physical dashboard, not raw access
to every internal parameter.

## Design goals

- **No MTU negotiation required.** All payloads fit inside the default
  ~20-byte usable ATT payload, so minimal/constrained implementations don't
  need to support larger MTUs.
- **Self-describing.** A client discovers what parameters exist, their
  names, units, and safe ranges from the peripheral itself — nothing about
  the specific parameter set needs to be hardcoded in the client.
- **Hardware-agnostic.** Any peripheral (ESP32, Pi, etc.) implementing this
  same characteristic layout and byte format is a valid target for a
  compliant client, regardless of what's actually running underneath.

## GATT Service

Reuses whichever service UUID the peripheral advertises (in the reference
implementation: `b25e0000-0001-4a5e-8f1a-000000000001`). The six
characteristics below can coexist in the same service as other
peripheral-specific characteristics (e.g. dedicated telemetry chars for a
watch companion).

## Characteristics

| Name | Properties | Payload | UUID (reference impl) |
|---|---|---|---|
| Directory Count | Read | 1 byte: `count` | `...0009` |
| Directory Index | Write | 1 byte: `index` | `...000a` |
| Directory Entry | Read, Notify | 19 bytes (see below) | `...000b` |
| Param Read Request | Write | 2 bytes: `id` (uint16 LE) | `...000c` |
| Param Value | Read, Notify | 4 bytes: `id` (uint16 LE) + `value` (int16 LE) | `...000d` |
| Param Write Request | Write | 4 bytes: `id` (uint16 LE) + `value` (int16 LE) | `...000e` |

### Directory Entry payload (19 bytes)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | `id` | uint16 LE — the parameter's virtual ID, used in all other characteristics |
| 2 | 1 | `flags` | bit0 = readable, bit1 = writable |
| 3 | 1 | `scale` | int8 — `real_value = raw_value * 10^scale`. e.g. scale=-1 means divide by 10 |
| 4 | 2 | `min` | int16 LE, in raw units |
| 6 | 2 | `max` | int16 LE, in raw units |
| 8 | 8 | `name` | ASCII, NOT null-terminated if exactly 8 chars — trim trailing `\0` |
| 16 | 3 | `unit` | ASCII, same trimming rule, e.g. `"%"`, `"V"`, `"C"`, `"A"`, `"-"` |

## Discovery flow (client)

1. Read **Directory Count** → `N`
2. For `i` in `0..N-1`:
   a. Write `i` to **Directory Index**
   b. Read (or wait for notify on) **Directory Entry** → decode the 19-byte record
3. Build UI from the decoded entries — one row per param, showing `name (unit)`, a value display if readable, and an editable control (respecting `min`/`max`) if writable.

## Reading a value

1. Write the param's `id` (2 bytes LE) to **Param Read Request**
2. The peripheral responds via a notify on **Param Value** containing `{id, value}` — match on `id` since multiple reads/writes may be in flight
3. Apply `scale` to convert `value` (raw) to a real-world number for display

## Writing a value

1. Write `{id, value}` (4 bytes: id LE, value LE) to **Param Write Request**, where `value` is in *raw* units per that param's `scale`
2. The peripheral validates: does the ID exist, is it writable, is the value within `[min, max]`? If any check fails, the write is rejected.
3. Result is signalled via whatever ACK mechanism the peripheral uses (in the reference implementation, a shared ACK characteristic: `0x00` = accepted, `0x01` = rejected). This spec doesn't mandate a specific ACK channel — a compliant peripheral MUST reject invalid writes rather than silently clamping or ignoring them, and SHOULD provide some signal of success/failure, but the exact mechanism is implementation-defined in v1.0.

## Reference parameter set (ZombieVerter Dial Display v1)

| ID | Name | Unit | R/W | Scale | Range (real) |
|---|---|---|---|---|---|
| 0 | SOC | % | R | 0 | 0 – 100 |
| 1 | PackV | V | R | -1 | 0.0 – 600.0 |
| 2 | MotorTmp | C | R | -1 | -40.0 – 200.0 |
| 3 | PackA | A | R | -1 | -3000.0 – 3000.0 |
| 4 | Regen | % | R/W | 0 | 0 – 35 |
| 5 | Throttle | % | R/W | 0 | 0 – 100 |
| 6 | Gear | - | R/W | 0 | 0 – 3 (P/R/N/D) |

This set is **not** part of the protocol itself — it's just what one
particular firmware happens to expose. A different implementation could
expose a different subset with different IDs; a compliant client discovers
this from the directory at runtime rather than assuming this table.

## Versioning

This is v1.0. Future versions should remain backward compatible where
possible (e.g. adding new directory flag bits rather than repurposing
existing ones) so v1.0 clients continue working against newer peripherals
exposing a superset of functionality.
