#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================
// Board Hardware Configuration
// ============================================
// Pins, resolution and feature flags (HAS_RFID etc.) live in BoardConfig.h,
// selected by the BOARD_* build flag (default: BOARD_M5DIAL).
#include "BoardConfig.h"

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
#define DEBUG_SERIAL        false
#define DEBUG_CAN           false  // Enable to see CAN messages
#define DEBUG_SDO           false  // Enable to see SDO TX/RX traffic (very chatty)
#define DEBUG_TOUCH         false

#endif // CONFIG_H
