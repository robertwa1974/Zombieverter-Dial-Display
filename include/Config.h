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
#define MAX_JSON_SIZE       16384  // 16KB - enough for large param files

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
#define MAX_PARAMETERS      64
#define TX_QUEUE_SIZE       16
#define RX_QUEUE_SIZE       32
#define PARAM_UPDATE_INTERVAL_MS  100

// Debug
#define DEBUG_SERIAL        true
#define DEBUG_CAN           true   // Enable to see CAN messages
#define DEBUG_TOUCH         false

#endif // CONFIG_H
