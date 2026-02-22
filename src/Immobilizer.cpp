#include "Immobilizer.h"
#include <M5Unified.h>  // M5Unified provides M5.Rfid for M5Dial
#include <driver/twai.h>  // For CAN SDO commands

// M5Dial has built-in RFID accessible via M5.Rfid API

Immobilizer::Immobilizer() 
    : unlocked(false), pinEntryMode(false), pinPosition(0), currentDigit(0),
      lastHeartbeat(0), lastVCUHeartbeat(0) {
    // Initialize entered PIN to zeros
    for (int i = 0; i < SECRET_PIN_LENGTH; i++) {
        enteredPIN[i] = 0;
    }
}

void Immobilizer::init() {
    Serial.println("=== Immobilizer Init Start ===");
    
    #if RFID_ENABLED
    Serial.println("Initializing M5Dial WS1850S RFID...");
    Serial.println("Using M5.Rfid API from M5Unified");
    
    // M5.Rfid should be initialized by M5.begin()
    // Just verify it's available
    
    delay(100);
    
    Serial.println("===========================================");
    Serial.println("✓ RFID Ready (WS1850S @ 0x28)");
    Serial.println(">>> Place RFID card on BACK of M5Dial <<<");
    Serial.println();
    Serial.println("Authorized UIDs:");
    for (int i = 0; i < sizeof(AUTHORIZED_UIDS) / sizeof(AUTHORIZED_UIDS[0]); i++) {
        Serial.print("  Card ");
        Serial.print(i + 1);
        Serial.print(": ");
        for (int j = 0; j < 4; j++) {
            if (AUTHORIZED_UIDS[i][j] < 0x10) Serial.print("0");
            Serial.print(AUTHORIZED_UIDS[i][j], HEX);
            if (j < 3) Serial.print(" ");
        }
        Serial.println();
    }
    Serial.println("\nNote: Ghost reads (20 20 20 20) are filtered");
    Serial.println("===========================================");
    #else
    Serial.println("RFID disabled in config");
    #endif
    
    unlocked = false;
    lastVCUHeartbeat = millis();
    
    Serial.println("Immobilizer: LOCKED (default)");
    Serial.println("=== Immobilizer Init Complete ===");
    Serial.println();
}

void Immobilizer::update() {
    // Send idcmax continuously via CAN mapping
    static uint32_t lastSend = 0;
    if (millis() - lastSend > IDCMAX_SEND_INTERVAL) {
        lastSend = millis();
        sendCurrentLimit();  // Send idcmax via CAN
    }
    
    // Check for VCU timeout (auto-lock if ignition off)
    // TODO: Monitor specific 0x500 ID when implemented
    // TEMPORARILY DISABLED - auto-locking too aggressive without real VCU
    /*
    if (unlocked && !isVCUActive()) {
        Serial.println("VCU heartbeat timeout - AUTO-LOCKING");
        lock();
    }
    */
    
    // Check RFID if enabled
    #if RFID_ENABLED
    static uint32_t lastRfidCheck = 0;
    static uint32_t rfidCheckCount = 0;
    static uint32_t lastCardTime = 0;
    
    // Check every 500ms and print status every 10 seconds
    if (millis() - lastRfidCheck > 500) {
        lastRfidCheck = millis();
        rfidCheckCount++;
        
        // Print RFID status every 20 checks (10 seconds)
        if (rfidCheckCount % 20 == 0) {
            Serial.println("[RFID] Scanning for cards...");
        }
        
        if (checkRFID()) {
            // Debounce - only act if at least 2 seconds since last card
            if (millis() - lastCardTime > 2000) {
                lastCardTime = millis();
                
                if (unlocked) {
                    lock();
                    Serial.println("RFID toggle - LOCKED");
                } else {
                    unlock();
                    Serial.println("RFID toggle - UNLOCKED");
                }
            }
        }
    }
    #endif
}

void Immobilizer::lock() {
    unlocked = false;
    pinEntryMode = false;
    clearPIN();
    Serial.println(">>> IMMOBILIZER LOCKED <<<");
    Serial.println("[IMMOBILIZER] Will send idcmax=0 via CAN mapping");
}

void Immobilizer::unlock() {
    unlocked = true;
    pinEntryMode = false;
    clearPIN();
    Serial.println(">>> IMMOBILIZER UNLOCKED <<<");
    Serial.println("[IMMOBILIZER] Will send idcmax=500 via CAN mapping");
}

void Immobilizer::sendCurrentLimit() {
    // Send idcmax via SDO Write to parameter 37
    // HYPOTHESIS: idcmax might NOT use fixed-point encoding!
    // Trying RAW value (500, not 16000)
    
    int16_t current = unlocked ? CURRENT_UNLOCKED : CURRENT_LOCKED;
    
    // TRY WITHOUT FIXED-POINT ENCODING
    int32_t encodedValue = current;  // Send 500 directly, not 500*32
    
    twai_message_t msg;
    msg.identifier = 0x603;  // SDO TX to node 3
    msg.extd = 0;
    msg.rtr = 0;
    msg.data_length_code = 8;
    
    // SDO Write format - BACK TO PARAMETER 37, NO ENCODING
    msg.data[0] = 0x23;  // SDO write 4 bytes
    msg.data[1] = 0x00;  // Index low byte (0x2100)
    msg.data[2] = 0x21;  // Index high byte (0x2100)
    msg.data[3] = 37;    // Subindex = parameter ID 37 (idcmax "i" field)
    msg.data[4] = encodedValue & 0xFF;
    msg.data[5] = (encodedValue >> 8) & 0xFF;
    msg.data[6] = (encodedValue >> 16) & 0xFF;
    msg.data[7] = (encodedValue >> 24) & 0xFF;
    
    if (twai_transmit(&msg, pdMS_TO_TICKS(10)) == ESP_OK) {
        // Wait briefly for response (to check for errors)
        twai_message_t rxMsg;
        uint32_t startTime = millis();
        while (millis() - startTime < 50) {  // 50ms timeout
            if (twai_receive(&rxMsg, pdMS_TO_TICKS(5)) == ESP_OK) {
                if (rxMsg.identifier == 0x583 &&  // SDO response from node 3
                    rxMsg.data[1] == 0x00 &&
                    rxMsg.data[2] == 0x21 &&
                    rxMsg.data[3] == 37) {  // Response to our idcmax write
                    
                    if (rxMsg.data[0] == 0x60) {
                        // Success response
                        Serial.println("[IMMOBILIZER] ✓ SDO Write ACK received!");
                    } else if (rxMsg.data[0] == 0x80) {
                        // Abort response - ERROR!
                        uint32_t abortCode = rxMsg.data[4] | 
                                           (rxMsg.data[5] << 8) | 
                                           (rxMsg.data[6] << 16) | 
                                           (rxMsg.data[7] << 24);
                        Serial.printf("[IMMOBILIZER] ✗ SDO ABORT! Code: 0x%08X\n", abortCode);
                        if (abortCode == 0x06090011) Serial.println("  -> Subindex does not exist");
                        if (abortCode == 0x06010000) Serial.println("  -> Unsupported access");
                        if (abortCode == 0x06090030) Serial.println("  -> Value range exceeded");
                        if (abortCode == 0x08000020) Serial.println("  -> Data cannot be transferred");
                        if (abortCode == 0x08000021) Serial.println("  -> Data cannot be transferred (local control)");
                        if (abortCode == 0x08000022) Serial.println("  -> Data cannot be transferred (device state)");
                    }
                    break;
                }
            }
        }
    }
    
    // Debug output
    Serial.println("========================================");
    Serial.printf("[IMMOBILIZER] Status: %s\n", unlocked ? "UNLOCKED" : "LOCKED");
    Serial.printf("[IMMOBILIZER] Target idcmax: %dA (RAW, no encoding)\n", current);
    Serial.printf("[IMMOBILIZER] SDO TX: ID=0x603 Param=37 Len=8\n");
    Serial.print("[IMMOBILIZER] Data: [");
    for (int i = 0; i < 8; i++) {
        Serial.printf("%02X", msg.data[i]);
        if (i < 7) Serial.print(" ");
    }
    Serial.println("]");
    Serial.println("========================================");
}

void Immobilizer::toggleLock() {
    if (unlocked) {
        lock();
    } else {
        unlock();
    }
}

void Immobilizer::enterDigit(uint8_t digit) {
    if (!unlocked && pinPosition < SECRET_PIN_LENGTH) {
        enteredPIN[pinPosition] = digit;
        pinPosition++;
        
        Serial.printf("PIN entry: position %d = %d\n", pinPosition - 1, digit);
        
        // Check if PIN complete
        if (pinPosition >= SECRET_PIN_LENGTH) {
            if (validatePIN()) {
                unlock();
            } else {
                Serial.println("INCORRECT PIN!");
                clearPIN();
            }
        }
    }
}

void Immobilizer::clearPIN() {
    pinPosition = 0;
    currentDigit = 0;
    for (int i = 0; i < SECRET_PIN_LENGTH; i++) {
        enteredPIN[i] = 0;
    }
}

bool Immobilizer::validatePIN() {
    for (int i = 0; i < SECRET_PIN_LENGTH; i++) {
        if (enteredPIN[i] != SECRET_PIN[i]) {
            return false;
        }
    }
    return true;
}

void Immobilizer::processCANMessage(uint32_t id, uint8_t* data, uint8_t len) {
    // Monitor for VCU heartbeat (0x500)
    if (id == VCU_HEARTBEAT_ID) {
        lastVCUHeartbeat = millis();
        
        // Optional: Log VCU heartbeat
        // Serial.printf("VCU heartbeat received at %lu\n", lastVCUHeartbeat);
    }
}

void Immobilizer::sendHeartbeat() {
    uint32_t now = millis();
    
    // Send every HEARTBEAT_INTERVAL ms
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        lastHeartbeat = now;
        
        // Prepare CAN message for ID 0x351
        uint8_t data[8] = {0};
        
        // Bytes 4 and 5 contain the current limit
        if (unlocked) {
            data[4] = 0xD0;  // 200A low byte (0x07D0 = 2000 = 200.0A)
            data[5] = 0x07;  // 200A high byte
        } else {
            data[4] = 0x00;  // 0A (locked - no current allowed)
            data[5] = 0x00;
        }
        
        // TODO: Send via CAN bus
        // This will be called from main.cpp where CAN access is available
        // Example: twai_transmit(&message, pdMS_TO_TICKS(10));
    }
}

bool Immobilizer::checkRFID() {
    #if RFID_ENABLED
    // Check for new card using M5.Rfid API
    if (!M5.Rfid.PICC_IsNewCardPresent()) {
        return false;
    }
    
    if (!M5.Rfid.PICC_ReadCardSerial()) {
        return false;
    }
    
    // Read UID and check for ghost read (0x20 0x20 0x20 0x20)
    uint8_t uid[4];
    bool isValid = false;
    
    for (byte i = 0; i < M5.Rfid.uid.size && i < 4; i++) {
        uid[i] = M5.Rfid.uid.uidByte[i];
        
        // If any byte is NOT 0x20, it's likely a real card
        if (uid[i] != 0x20) {
            isValid = true;
        }
    }
    
    // Filter ghost reads
    if (!isValid) {
        Serial.println("[RFID] Ghost read detected (20 20 20 20), ignoring...");
        M5.Rfid.PICC_HaltA();
        M5.Rfid.PCD_StopCrypto1();
        return false;
    }
    
    // Valid card detected
    Serial.println(">>> RFID: Real card detected! <<<");
    Serial.print("Card UID:");
    for (int i = 0; i < 4; i++) {
        if (uid[i] < 0x10) Serial.print(" 0");
        else Serial.print(" ");
        Serial.print(uid[i], HEX);
    }
    Serial.println();
    
    // Check if authorized
    bool authorized = checkAuthorizedUID(uid);
    
    // Halt card
    M5.Rfid.PICC_HaltA();
    M5.Rfid.PCD_StopCrypto1();
    
    if (authorized) {
        Serial.println(">>> AUTHORIZED! <<<");
        return true;
    } else {
        Serial.println(">>> UNAUTHORIZED! <<<");
        return false;
    }
    #endif
    
    return false;
}

bool Immobilizer::checkAuthorizedUID(uint8_t* uid) {
    for (int i = 0; i < NUM_AUTHORIZED_UIDS; i++) {
        bool match = true;
        for (int j = 0; j < 4; j++) {
            if (uid[j] != AUTHORIZED_UIDS[i][j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}
