# Credits and Acknowledgments

## Original Author

**Jamie Jones (jamiejones85)** - Creator of the original LilyGO T-Display CAN bus interface for ZombieVerter

Repository: https://github.com/jamiejones85/ZombieVerterDisplay

Jamie Jones developed the initial implementation of a CAN bus display for the ZombieVerter motor controller using the LilyGO T-Display ESP32 board. Their work provided the foundation for this project, including:

- CAN message parsing and parameter management system
- ZombieVerter CAN protocol implementation
- Display rendering concepts and UI design
- Parameter configuration via JSON
- Real-time telemetry processing
- SDO (Service Data Object) communication handling

Without Jamie Jones' excellent groundwork, this M5Stack Dial adaptation would not have been possible. The original project can be found at:
https://github.com/jamiejones85/ZombieVerterDisplay

## M5Stack Dial Adaptation

**Robert Wahler (robertwa1974)** - Adaptation for M5Stack Dial hardware

This project ports Jamiejones' code to the M5Stack Dial platform and adds:

- M5Stack Dial hardware support (ESP32-S3, round display)
- Rotary encoder input handling for parameter control
- Direct CAN control for Gear/Motor/Regen parameters
- Optimistic UI updates for instant feedback
- Encoder sensitivity tuning (4x quadrature handling)
- WiFi configuration interface
- Additional screens and UI improvements

## Hardware Platforms

### Original Platform
- **LilyGO T-Display** - ESP32-based development board with built-in TFT display
- Designed by LilyGO Technology

### Current Platform  
- **M5Stack Dial** - ESP32-S3 smart rotary knob with 1.28" round touch screen
- Designed by M5Stack Technology Co., Ltd.

## Software Components

### Core Libraries
- **ESP32-TWAI-CAN** / **ESP32CAN** - CAN bus communication
- **M5Dial** - M5Stack hardware abstraction layer
- **ESP32Encoder** - Rotary encoder handling
- **OneButton** - Button debouncing and event detection
- **ArduinoJson** - JSON parameter parsing

### Motor Controller
- **ZombieVerter** - Open-source AC induction motor controller
- Developed by the OpenInverter community
- Based on Johannes Huebner's original inverter design

### OpenInverter Project
- **OpenInverter Community** - Open-source EV motor controller development
- Forum, documentation, and support at [openinverter.org](https://openinverter.org)
- Community members who developed and documented the CAN protocol

## Special Thanks

- **OpenInverter Forum Members** - For CAN protocol documentation and support
- **M5Stack Community** - For hardware support and examples
- **ESP32 Arduino Core Team** - For the excellent ESP32 Arduino framework
- **PlatformIO Team** - For the development platform

## Contributing

This project welcomes contributions from the community. If you improve the code, add features, or fix bugs, please consider submitting a pull request.

## Open Source

Both Jamie Jones' original LilyGO implementation and this M5Stack Dial adaptation are open source projects, freely available for the electric vehicle community to use, modify, and improve.

## Links

- **Original Project:** https://github.com/jamiejones85/ZombieVerterDisplay
- **This Adaptation:** https://github.com/robertwa1974/Zombieverter-Dial-Display
- **ZombieVerter:** https://openinverter.org/wiki/Zombieverter
- **OpenInverter Forum:** https://openinverter.org/forum/
