# ZombieVerter Dial Display — v2.5.1

## Summary

Stability release fixing a severe WiFi performance regression introduced in
v2.5.0, plus a latent bug in the GVRET dashboard control. The web UI is now
fast and reliable, and the SavvyCAN bridge can be started from the browser as
intended. Logo upload remains a known limitation on this hardware (see below).

## Fixed

### WiFi / web UI performance (critical)
The web UI in v2.5.0 was slow to load, frequently timed out, and threw
`ERR_CONTENT_LENGTH_MISMATCH` / `ERR_CONNECTION_TIMED_OUT` errors in the
browser. Ping times to the dial's access point were 300–1500 ms with heavy
jitter.

Root cause: verbose serial debug logging (`DEBUG_CAN` and unconditional SDO
TX/RX prints) was saturating the 115200-baud serial link. At normal CAN bus
rates this produced thousands of bytes/sec of debug output, and each blocking
`Serial.printf` call on the main loop starved the WiFi/TCP stack of CPU time.
The effect was invisible to per-iteration timing checks because each individual
print returned quickly — it was the cumulative volume that caused the stall.

Fix:
- `DEBUG_CAN` now defaults to `false`.
- SDO per-transaction TX/RX logging is now gated behind a new `DEBUG_SDO` flag,
  defaulting to `false`. SDO error/timeout/abort logging remains always-on.

Result: AP ping times dropped from ~320 ms average to ~4 ms, and the web UI
loads reliably.

### GVRET start/stop button (latent bug)
The GVRET control on the dashboard never worked since the feature was added.
An unterminated HTML comment (`<!-- END SUPPORT` with no closing `-->`) in
`index.html` swallowed the `<script>` block that defined `toggleGVRET()` and
`updateGVRETStatus()`, so clicks produced a silent `ReferenceError` in the
console and never reached the dial.

Fix: closed the comment. The button now correctly starts and stops the GVRET
TCP server; SavvyCAN connections to `192.168.4.1` port `23` are verified
working.

## Changed

- **PSRAM configuration removed.** This M5Stack Dial hardware variant ships
  without PSRAM (confirmed on two units — PSRAM ID read returns `0x00ffffff`).
  The `-DBOARD_HAS_PSRAM` flag and `board_build.arduino.memory_type` setting
  have been removed from `platformio.ini` to eliminate boot-time PSRAM
  initialisation errors. The firmware runs entirely from internal SRAM.
- A lightweight `[LOOP] new max gap` diagnostic remains in the main loop. It
  is self-silencing (prints only on a new worst-case iteration time) and
  exists to catch any future regression that reintroduces loop blocking.

## Known issues

- **Logo upload is not functional on this hardware.** Uploading a custom
  splash logo can exhaust internal heap (no PSRAM available), causing a
  "heap low" message and preventing parameter fetch. Do not use the logo
  upload feature in this release. A redesign that fits the splash buffer
  within internal RAM is planned for a future release.
- **Web UI may fail to reload after closing the browser window.** First load
  works reliably; subsequent loads after closing and reopening the browser
  may hang or fail. Workaround: reboot the dial. Root cause under
  investigation — suspected AsyncWebServer client slot leak. Targeted for
  v2.5.2.
- **Recurring `[IMMOBILIZER] DriveInhibited read failed (0x06020000)` on
  serial.** The immobilizer polls VCU parameter 2124 every cycle, and the
  current VCU branch responds with SDO abort code `0x06020000` (object does
  not exist). Cosmetic noise only — does not affect operation. Will be
  addressed by either correcting the param ID or stopping the poll after
  first failure.

## Not verified in this release

- GVRET / SavvyCAN bridge basic start/stop and connection verified. Extended
  load testing (high-rate frame streaming over WiFi) not performed.

## Upgrade notes

A clean rebuild is recommended due to the `platformio.ini` changes:

    pio run -e m5stack-dial --target clean
    pio run -e m5stack-dial --target upload
    pio run -e m5stack-dial --target uploadfs

The `uploadfs` step is required to deploy the `index.html` GVRET fix.

To re-enable verbose logging for debugging, set `DEBUG_CAN` and/or
`DEBUG_SDO` to `true` in `Config.h`.
