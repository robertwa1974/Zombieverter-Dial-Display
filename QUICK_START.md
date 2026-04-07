# Quick Start Guide

## The Easiest Way — Web Installer

Flash your M5Stack Dial directly from Chrome or Edge, no tools required:

**https://robertwa1974.github.io/Zombieverter-Dial-Display**

Connect your M5Stack Dial via USB-C, click Install, wait ~60 seconds. Done.

After flashing, power on with your VCU connected — the Dial will automatically download the parameter list from the VCU over CAN and show live data.

---

## Building from Source

If you want to modify the code or build yourself, use PlatformIO.

### Step 1 — Install VS Code and PlatformIO

1. Download and install [VS Code](https://code.visualstudio.com/)
2. Open VS Code, click the Extensions icon (Ctrl+Shift+X)
3. Search for **PlatformIO IDE** and install it
4. Restart VS Code

### Step 2 — Open the Project

1. File → Open Folder
2. Select the `Zombieverter-Dial-Display` folder
3. PlatformIO will initialise automatically

### Step 3 — Build and Upload

1. Click the **✓** (Build) icon in the bottom toolbar
2. Connect M5Stack Dial via USB-C
3. Click the **→** (Upload) icon
4. Wait for "SUCCESS"
5. Upload the web interface files: click the PlatformIO menu → **Upload Filesystem Image**

---

## First Boot

With the VCU powered on and CAN connected:

1. Splash screen appears — **"Fetching params from VCU..."**
2. Parameter list downloads from VCU (~5 seconds)
3. **"VCU params loaded!"** confirms success
4. Dashboard shows live data

If the VCU is not connected at boot, the Dial uses a cached parameter file from the previous successful fetch.

---

## Controls

| Action | Result |
|--------|--------|
| Rotate encoder | Scroll through screens |
| Button click | Enable WiFi / enter edit mode on editable screens |
| Rotate in edit mode | Change value |
| Button click in edit mode | Save and exit edit mode |
| Rotate in WiFi mode | Exit WiFi mode |
| Long press | Return to Dashboard |

---

## WiFi Interface

Press the button on any screen to enter WiFi mode.

- **SSID:** `ZombieVerter-Display`
- **Password:** `zombieverter`
- **URL:** `http://192.168.4.1`

---

## Troubleshooting

**Display shows dashes everywhere**
Check CAN wiring (GPIO 2 = TX yellow, GPIO 1 = RX white). Navigate to the Settings screen — it shows CAN connection status.

**"VCU unavailable" on boot**
VCU was not connected at boot. Cached parameters will be used. Reboot with VCU connected to fetch fresh parameters.

**Upload fails**
Try a different USB cable — many are charge-only. The M5Stack Dial does not require holding any buttons to enter flash mode.

**Web interface won't load**
Make sure you connected to the `ZombieVerter-Display` WiFi network first, then open `http://192.168.4.1` in a browser.
