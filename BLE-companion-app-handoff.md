# BLE Companion App — Auth Token Handoff

**Context:** Zombieverter-Dial-Display firmware (`ble-unlock-v2` branch, merged with v2.6.0
Phase 1 fixes, now pushed to `main`-adjacent branch). Firmware-side BLE proximity unlock
is **fully verified working**. The remaining bug lives in the Android/Wear OS companion
app(s) and needs to be fixed there.

## Bottom line

**The companion app does not write to the Auth characteristic on connect.**

Everything downstream of that write — pairing, token storage, NVS persistence, unlock
trigger — is proven correct on the firmware side. Telemetry (read/notify) works fine from
the app, which is why it looked like "the app works" — but telemetry and the Auth write are
two separate code paths, and only the write path is broken/missing.

## What was proven, and how

All tests below were done via manual BLE writes using **nRF Connect for Mobile**, bypassing
the companion app entirely, to isolate firmware behavior from app behavior.

1. **Pairing (learn new token):** dial in `PAIR_BLE` mode, manual 16-byte write
   (`000102030405060708090A0B0C0D0E0F`) to the Auth characteristic →
   `[BLE] Token paired, total=1` + `[UI] Success: BLE device paired!`. Token confirmed
   persisted across reboot (`1 token(s) loaded from NVS` on next boot).
2. **Unlock (match known token):** dial `LOCKED`, same manual write →
   `[BLE] Known token — unlocking` → triggers `DriveInhibit=0` write sequence correctly.
   (The actual CAN write failed with `0x05040000` timeout in this test only because the
   bench setup has no live VCU on the CAN bus — unrelated to BLE, confirmed by the same
   timeout pattern on *every* SDO transaction all session.)
3. **Connection lifecycle:** `[BLE] Client connected` / `[BLE] Client disconnected —
   restarting advertising` fire correctly and reliably; single-client-slot behavior
   confirmed (advertising stops while connected, resumes on disconnect).

Across **every test where the real companion app connected** (multiple sessions, both a
fresh boot connection and explicit reconnects after toggling Bluetooth), **zero** Auth
characteristic writes were ever observed — no `Token paired`, `Token already stored`,
`Token storage full`, or `Known token` log lines, despite `[BLE] Client connected` firing
correctly every time. The only way any of those log lines have ever appeared in this whole
investigation was via manual nRF Connect writes.

## Relevant firmware reference (do not need to change this)

- **Auth characteristic UUID:** `b25e0000-0001-4a5e-8f1a-00000000000f`
- **Service UUID:** `b25e0000-0001-4a5e-8f1a-000000000001`
- **Token length:** 16 bytes exactly (`BLE_AUTH_TOKEN_LEN` in `Config.h`) — firmware
  silently rejects (no log) any write that isn't exactly 16 bytes, so this is a good thing
  to double check in the app's write call.
- **Expected app behavior (per design intent, from code comments in `Immobilizer.cpp` and
  `BleTelemetry.cpp`):** the app should write its stored 16-byte token to the Auth
  characteristic **every time it connects**, unconditionally — not just once at first pairing,
  not gated behind any "first run" state. The firmware distinguishes pairing vs. unlock
  purely by its own current mode (`PAIR_BLE` vs `LOCKED`), so the app doesn't need to know
  or care which one is happening — it should just always send the token on connect.
- **Token generation/storage on the app side:** per prior work, handled by
  `AuthTokenStore.kt` (SecureRandom-based 16-byte token, stored locally on watch/phone).
  This part was previously confirmed working — the gap is specifically the "send on every
  connect" step.

## What to check in the Android/Wear OS project

1. Find the BLE connection manager / GATT client code — likely near wherever the telemetry
   characteristic subscriptions (notify) are set up, since that's the code known to work.
2. Confirm there's a call that writes to the Auth characteristic
   (`b25e0000-0001-4a5e-8f1a-00000000000f`) **unconditionally on every successful connect**,
   not gated behind a "first launch" / "not yet paired" flag.
3. If the call exists but isn't firing: check for a guard condition that might be
   incorrectly skipping it on reconnects (e.g. `if (!alreadyPaired)`).
4. If the call doesn't exist at all: it may have been dropped/never wired up when telemetry
   was implemented, or lost in a refactor. Add a call to `AuthTokenStore`'s stored token,
   written via a standard GATT characteristic write, immediately after connection
   established (same point where telemetry notification subscriptions are set up).
5. Confirm the write is exactly 16 bytes and targets the correct UUID above.
6. Test with the same nRF Connect approach used here if needed — connect the *real app*
   while running `platformio device monitor` on the dial, watch for `[BLE] Client connected`
   followed immediately by one of `Token paired` / `Token already stored` / `Known token —
   unlocking`. That pair of log lines appearing together is the signal the fix worked.

## Firmware repo state (for reference, no action needed here)

- `ble-unlock-v2` branch: merged with `main` (v2.6.0), pushed to origin.
- One cosmetic edit made during this investigation: removed a redundant
  `if (!bleEnabled) return;` guard in `Immobilizer::onBleAuthReceived()`'s unlock branch.
  This was **not** the actual bug (that member variable defaults `true` and was never the
  blocker) — removing it just brings the code in line with its own existing header comment,
  which says the new token system should work independent of that flag. Harmless, already
  committed.
