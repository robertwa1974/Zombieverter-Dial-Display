# Credits

## Original Project

**Jamie Jones (jamiejones85)**
[ZombieVerterDisplay](https://github.com/jamiejones85/ZombieVerterDisplay)

Jamie created the original CAN bus display for the ZombieVerter using the LilyGO T-Display ESP32. His work established the CAN/SDO protocol implementation, parameter management system, and the OpenInverter web interface integration that this project builds on.

## M5Stack Dial Adaptation

**robertwa1974**
[Zombieverter-Dial-Display](https://github.com/robertwa1974/Zombieverter-Dial-Display)

Ported to M5Stack Dial and extended:

- Full LVGL UI with 10 screens optimised for the 240×240 round display
- Rotary encoder navigation with edit mode for Gear, Motor, Regen
- Live values on both dial and web UI via CAN broadcast + SDO polling
- Automatic parameter fetch from VCU at boot via SDO segmented transfer
- `/spot` endpoint serving live values to the web interface
- All parameter writes via SDO — no VCU CAN map required
- GitHub Actions CI/CD with automated factory binary build and web flasher deployment

## Libraries

| Library | Purpose |
|---------|---------|
| LVGL 8.4 | UI framework |
| M5Unified | M5Stack hardware abstraction |
| M5GFX | Display driver |
| ESP32-TWAI | CAN bus driver (built into ESP-IDF) |
| ArduinoJson | JSON parsing |
| ESPAsyncWebServer | Async web server |
| AsyncTCP | Async TCP layer |

## Community

- **OpenInverter community** — CAN protocol documentation and forum support
- **Nishanth Samala (outlandnish)** — SDO protocol reference implementation
- **M5Stack community** — Hardware support and examples

## Links

- Original project: https://github.com/jamiejones85/ZombieVerterDisplay
- This project: https://github.com/robertwa1974/Zombieverter-Dial-Display
- ZombieVerter VCU: https://openinverter.org/wiki/Zombieverter
- OpenInverter forum: https://openinverter.org/forum/
