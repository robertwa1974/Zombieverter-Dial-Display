# Build Instructions

## Option 1 — Web Installer (No Tools Required)

Flash a pre-built binary directly from your browser:

**https://robertwa1974.github.io/Zombieverter-Dial-Display**

Requires Chrome or Edge. Flashes firmware and web interface files in one step.

---

## Option 2 — PlatformIO (Recommended for Developers)

### Requirements

- [VS Code](https://code.visualstudio.com/)
- PlatformIO IDE extension (install from VS Code Extensions)

### Steps

```bash
# Clone the repo
git clone https://github.com/robertwa1974/Zombieverter-Dial-Display.git
cd Zombieverter-Dial-Display

# Build firmware
pio run -e m5stack-dial

# Upload firmware
pio run -e m5stack-dial --target upload

# Upload web interface (SPIFFS filesystem)
pio run -e m5stack-dial --target uploadfs

# Monitor serial output
pio device monitor --baud 115200
```

Or use the VS Code toolbar icons: ✓ Build → → Upload → Upload Filesystem Image.

---

## Option 3 — Espressif Flash Download Tool (Windows)

Use this to flash a pre-built `factory.bin` without any development tools.

1. Download the [Flash Download Tool](https://www.espressif.com/en/support/download/other-tools) from Espressif
2. Run `flash_download_tool.exe`
3. Select: **ChipType = ESP32-S3**, **WorkMode = Develop**, **LoadMode = USB**
4. Add the following files at these addresses:

| File | Address |
|------|---------|
| `bootloader.bin` | `0x0000` |
| `partitions.bin` | `0x8000` |
| `firmware.bin` | `0x10000` |
| `spiffs.bin` | `0x3F0000` |

5. Set **BAUD = 921600**, **FLASH SIZE = 64Mbit**
6. Click **ERASE**, then **START**

---

## Partition Layout

```
nvs       0x9000    0x5000
otadata   0xe000    0x2000
app0      0x10000   0x1F0000
app1      0x200000  0x1F0000
spiffs    0x3F0000  0x400000
```

Total flash: 8MB (ESP32-S3).

---

## Troubleshooting

**"Library not found"** — PlatformIO downloads dependencies automatically on first build. Wait for it to complete.

**"Upload failed"** — Try a different USB-C cable (many are charge-only). The M5Stack Dial does not need any buttons held to enter flash mode.

**"Permission denied" on Linux:**
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

**Port not detected on Windows** — Install the [CH341 USB driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html).

---

## After Flashing

Power on the M5Stack Dial with the ZombieVerter VCU connected and CAN active. The Dial will automatically fetch the parameter list from the VCU on first boot. No manual file upload needed.
