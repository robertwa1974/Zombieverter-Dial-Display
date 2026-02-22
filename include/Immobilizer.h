#ifndef IMMOBILIZER_H
#define IMMOBILIZER_H

#include <Arduino.h>
#include <M5Unified.h>
#include "Config.h"

// RFID Module - WS1850S I2C RFID (13.56MHz)
// M5Dial V1.1 has built-in WS1850S at I2C address 0x28
// ISSUE: M5Unified 0.1.17 does not include M5.Rfid API
// Need to either:
//   1. Update to newer M5Unified (may have M5.Rfid)
//   2. Find WS1850S I2C library
//   3. Implement I2C protocol manually
// TEMPORARILY DISABLED until proper library is found
#define RFID_ENABLED        false  // M5.Rfid not available in M5Unified 0.1.17

// I2C configuration for future implementation
#define RFID_I2C_SDA        11     // I2C Data
#define RFID_I2C_SCL        12     // I2C Clock
#define RFID_I2C_ADDR       0x28   // WS1850S I2C address

// Ghost read: 0x20 0x20 0x20 0x20 must be filtered

// Old SPI pins - NOT USED (kept for reference)
// The previous attempts failed because we were using SPI
// when the M5Dial RFID is actually I2C-based!
#define RFID_SDA_PIN        15     // WRONG - this was for SPI
#define RFID_SCK_PIN        5      // WRONG
#define RFID_MOSI_PIN       6      // WRONG
#define RFID_MISO_PIN       7      // WRONG
#define RFID_RST_PIN        8      // WRONG
#define RFID_SS_PIN         RFID_SDA_PIN

// Note: Place RFID cards on the BACK of M5Dial to read

// Immobilizer Security Settings
#define SECRET_PIN_LENGTH   4
#define VCU_TIMEOUT_MS      5000   // 5 seconds without VCU heartbeat = auto-lock
#define IMMOBILIZER_CAN_ID  0x351  // Immobilizer heartbeat CAN ID
#define VCU_HEARTBEAT_ID    0x500  // VCU heartbeat to monitor
#define HEARTBEAT_INTERVAL  100    // Send immobilizer status every 100ms

// ZombieVerter Current Limit Control via CAN Mapping
// idcmax must be mapped in ZombieVerter canmap to CAN ID 0x300
// When locked: Send idcmax=0 on CAN 0x300
// When unlocked: Send idcmax=500 on CAN 0x300
#define IDCMAX_CAN_ID       0x300  // CAN ID where idcmax is mapped (adjust if different)
#define CURRENT_LOCKED      0      // 0A when locked
#define CURRENT_UNLOCKED    500    // 500A when unlocked (matches BMS limit)
#define IDCMAX_SEND_INTERVAL 100   // Send every 100ms for continuous override

// Authorized RFID UIDs
// Add your actual card UIDs here after scanning them
const uint8_t AUTHORIZED_UIDS[][4] = {
    {0xDE, 0xAD, 0xBE, 0xEF},  // Example UID 1 - replace with real card
    {0xCA, 0xFE, 0xBA, 0xBE},  // Example UID 2 - replace with real card
    // Add more authorized cards here
    // Format: {0xAA, 0xBB, 0xCC, 0xDD}
    // DO NOT add {0x20, 0x20, 0x20, 0x20} - this is a ghost read!
};
const uint8_t NUM_AUTHORIZED_UIDS = sizeof(AUTHORIZED_UIDS) / sizeof(AUTHORIZED_UIDS[0]);

// Secret PIN (change this!)
const uint8_t SECRET_PIN[SECRET_PIN_LENGTH] = {1, 2, 3, 4};  // Change to your PIN

class Immobilizer {
public:
    Immobilizer();
    
    // Main control
    void init();
    void update();  // Call in main loop
    
    // Lock state
    bool isUnlocked() { return unlocked; }
    void lock();
    void unlock();
    void toggleLock();
    
    // CAN control for immobilizer
    void sendCurrentLimit();  // Send idcmax via CAN mapping
    
    // PIN entry
    void enterDigit(uint8_t digit);
    void clearPIN();
    uint8_t getCurrentDigit() { return currentDigit; }
    uint8_t getPINPosition() { return pinPosition; }
    bool isPINEntry() { return pinEntryMode; }
    void setCurrentDigit(uint8_t digit) { if (digit <= 9) currentDigit = digit; }
    void incrementDigit() { currentDigit = (currentDigit + 1) % 10; }
    void decrementDigit() { currentDigit = (currentDigit == 0) ? 9 : currentDigit - 1; }
    
    // RFID (if enabled)
    bool checkRFID();
    
    // CAN monitoring
    void processCANMessage(uint32_t id, uint8_t* data, uint8_t len);
    void sendHeartbeat();  // Called by timer
    
    // VCU monitoring
    uint32_t getTimeSinceVCU() { return millis() - lastVCUHeartbeat; }
    bool isVCUActive() { return (millis() - lastVCUHeartbeat) < VCU_TIMEOUT_MS; }
    
private:
    // State
    bool unlocked;
    bool pinEntryMode;
    
    // PIN entry
    uint8_t enteredPIN[SECRET_PIN_LENGTH];
    uint8_t pinPosition;
    uint8_t currentDigit;  // 0-9 for rotary selection
    
    // Timing
    uint32_t lastHeartbeat;
    uint32_t lastVCUHeartbeat;
    
    // RFID
    bool checkAuthorizedUID(uint8_t* uid);
    
    // PIN validation
    bool validatePIN();
};

#endif // IMMOBILIZER_H
