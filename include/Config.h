#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================
// M5Stack Dial Hardware Configuration
// ============================================

// Display Configuration
#define SCREEN_WIDTH        240
#define SCREEN_HEIGHT       240
#define SCREEN_IS_ROUND     true
#define SCREEN_CENTER_X     120
#define SCREEN_CENTER_Y     120
#define SCREEN_RADIUS       120

// Encoder Pins (Built-in on M5Stack Dial)
#define ENCODER_PIN_A       41  // Swapped with PIN_B
#define ENCODER_PIN_B       40  // Swapped with PIN_A
#define ENCODER_BUTTON      42

// Touch Screen (FT6336U)
#define TOUCH_SDA           11
#define TOUCH_SCL           12
#define TOUCH_INT           14
#define TOUCH_RST           -1

// CAN Bus (External Unit - Grove Port)
// USE PORT B (GREY CONNECTOR) - This is the one that works!
// Port B uses GPIO 2 and GPIO 1 (confirmed by testing)
#define CAN_TX_PIN          2   // Port B (Grey) - Pin 1 (Yellow wire)
#define CAN_RX_PIN          1   // Port B (Grey) - Pin 2 (White wire)

// Power & LED
#define POWER_HOLD_PIN      46
#define LED_PIN             21

// ============================================
// WiFi Configuration
// ============================================

// WiFi AP Mode (for configuration)
#define WIFI_AP_SSID        "ZombieVerter-Display"
#define WIFI_AP_PASSWORD    "zombieverter"
#define WIFI_AP_CHANNEL     1

// Web Server
#define WEB_SERVER_PORT     80

// File Upload
#define MAX_JSON_SIZE       65536  // 64KB - enough for large param files

// ============================================
// Application Configuration
// ============================================

// CAN Bus Settings
#define CAN_BAUDRATE        500000  // 500kbps - standard for ZombieVerter
#define CAN_NODE_ID         3       // YOUR ZombieVerter is on Node 3!

// UI Settings
#define MAX_SCREENS         6
#define MAX_WIDGETS_PER_SCREEN  8
#define DEFAULT_BRIGHTNESS  128
#define SLEEP_TIMEOUT_MS    300000  // 5 minutes

// Data Settings
#define MAX_PARAMETERS      250
#define TX_QUEUE_SIZE       16
#define RX_QUEUE_SIZE       32
#define PARAM_UPDATE_INTERVAL_MS  100

// Debug
#define DEBUG_SERIAL        true
#define DEBUG_CAN           false  // Enable to see CAN messages
#define DEBUG_SDO           false  // Enable to see SDO TX/RX traffic (very chatty)
#define DEBUG_TOUCH         false

// ============================================
// BLE Parameter Bus — watch + phone companion apps
// ============================================
// GATT server (peripheral) exposing a small set of parameters via a
// generic directory-based bus (see PROTOCOL.md) — used by BOTH the Wear OS
// watch app and the Android phone app. Uses the same classic ESP32 Arduino
// BLE library as Immobilizer's BLE_ENABLED scanner — do NOT add NimBLE
// alongside this; two BLE stacks in one firmware conflict.
#define BLE_TELEMETRY_ENABLED     true
#define BLE_DEVICE_NAME           "ZombieVerter-Dial"
#define BLE_TELEMETRY_INTERVAL_MS 1000   // auto-push rate for readable params
#define BLE_GEAR_INTERLOCK_RPM    100    // reject gear-change writes above this speed

#define BLE_SERVICE_UUID          "b25e0000-0001-4a5e-8f1a-000000000001"
#define BLE_CHAR_ACK_UUID         "b25e0000-0001-4a5e-8f1a-000000000008"

// Generic parameter bus — see PROTOCOL.md
#define BLE_CHAR_DIR_COUNT_UUID       "b25e0000-0001-4a5e-8f1a-000000000009"
#define BLE_CHAR_DIR_INDEX_UUID       "b25e0000-0001-4a5e-8f1a-00000000000a"
#define BLE_CHAR_DIR_ENTRY_UUID       "b25e0000-0001-4a5e-8f1a-00000000000b"
#define BLE_CHAR_PARAM_READ_REQ_UUID  "b25e0000-0001-4a5e-8f1a-00000000000c"
#define BLE_CHAR_PARAM_VALUE_UUID     "b25e0000-0001-4a5e-8f1a-00000000000d"
#define BLE_CHAR_PARAM_WRITE_REQ_UUID "b25e0000-0001-4a5e-8f1a-00000000000e"

// BLE proximity unlock (connection+token based — NOT the old disabled
// BLE_ENABLED scanner in Immobilizer.h/.cpp, which remains untouched).
// A paired watch/phone writes a stored 16-byte token to this characteristic
// immediately after connecting; Immobilizer checks it against its own
// stored token list. See Immobilizer::onBleAuthReceived().
#define BLE_CHAR_AUTH_UUID        "b25e0000-0001-4a5e-8f1a-00000000000f"
#define BLE_AUTH_TOKEN_LEN        16
#define MAX_BLE_AUTH_TOKENS       4

#endif // CONFIG_H
