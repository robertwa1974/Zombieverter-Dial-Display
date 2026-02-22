# CRITICAL FIX - CAN 0x351 Heartbeat Must Always Send

## The Problem:

The immobilizer CAN heartbeat (0x351) must be sent **every 100-200ms** or ZombieVerter will fault with "BMS Timeout".

**Current implementation is WRONG:**
- Heartbeat only sends when `loop()` runs
- If UI is blocking (menu scrolling, button handling), heartbeat stops
- ZombieVerter trips BMS timeout fault
- Vehicle shuts down while driver is just looking at menus!

## The Solution:

Use a **hardware timer interrupt** that sends the CAN message **independent of loop()**.

---

## Implementation:

### Step 1: Add Hardware Timer (in main.cpp)

**At top of file, add:**

```cpp
#include <esp_timer.h>

// Hardware timer for CAN heartbeat
esp_timer_handle_t canHeartbeatTimer;

// CAN heartbeat callback (runs in interrupt context)
void IRAM_ATTR sendCanHeartbeat(void* arg) {
    // Prepare CAN message 0x351
    twai_message_t msg;
    msg.identifier = 0x351;
    msg.extd = 0;
    msg.rtr = 0;
    msg.data_length_code = 8;
    
    // Clear all bytes
    for (int i = 0; i < 8; i++) {
        msg.data[i] = 0x00;
    }
    
    // Set current limit based on immobilizer state
    if (immobilizer.isUnlocked()) {
        // Unlocked: 200A (2000 in 0.1A units = 0x07D0)
        msg.data[4] = 0xD0;  // Low byte
        msg.data[5] = 0x07;  // High byte
    } else {
        // Locked: 0A
        msg.data[4] = 0x00;
        msg.data[5] = 0x00;
    }
    
    // Send CAN message (don't wait - we're in interrupt!)
    twai_transmit(&msg, 0);  // 0 = don't block
}
```

### Step 2: Start Timer in setup()

**In setup(), after CAN initialization:**

```cpp
void setup() {
    // ... existing setup code ...
    
    // Initialize CAN
    if (!canManager.init()) {
        Serial.println("CAN init failed!");
    }
    
    // *** ADD THIS: Start CAN heartbeat timer ***
    // Create timer that fires every 100ms (10 Hz)
    const esp_timer_create_args_t timerArgs = {
        .callback = &sendCanHeartbeat,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "can_heartbeat",
        .skip_unhandled_events = false
    };
    
    esp_timer_create(&timerArgs, &canHeartbeatTimer);
    esp_timer_start_periodic(canHeartbeatTimer, 100000);  // 100ms = 100,000 microseconds
    
    Serial.println("CAN heartbeat timer started (100ms)");
    
    // ... rest of setup ...
}
```

### Step 3: Remove Heartbeat from loop()

**In loop(), REMOVE any manual heartbeat sending code:**

```cpp
void loop() {
    Hardware::update();
    inputManager.update();
    immobilizer.update();
    uiManager.update();
    
    // *** REMOVE THIS ENTIRE SECTION: ***
    // static uint32_t lastHeartbeat = 0;
    // if (millis() - lastHeartbeat >= 100) {
    //     lastHeartbeat = millis();
    //     ... manual heartbeat code ...
    // }
    
    // Handle CAN messages
    twai_message_t rx_msg;
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
        canManager.processMessage(rx_msg.identifier, rx_msg.data, rx_msg.data_length_code);
        immobilizer.processCANMessage(rx_msg.identifier, rx_msg.data, rx_msg.data_length_code);
    }
    
    delay(10);
}
```

---

## Why This Works:

**Hardware Timer Interrupt:**
- Runs **independently** of main loop
- **Not blocked** by UI, menus, or any other code
- **Guaranteed timing** - fires every 100ms precisely
- **Even if loop() hangs**, timer still fires!

**Result:**
- ZombieVerter **always receives 0x351** every 100ms
- No BMS timeout faults
- Can scroll menus, enter PIN, navigate screens without issues
- **Vehicle stays running** regardless of what M5Dial is doing

---

## Alternative: FreeRTOS Task (More Complex)

If you want even more isolation, use a dedicated RTOS task:

```cpp
// CAN heartbeat task (runs on separate CPU core)
void canHeartbeatTask(void* parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(100);  // 100ms
    
    twai_message_t msg;
    msg.identifier = 0x351;
    msg.extd = 0;
    msg.rtr = 0;
    msg.data_length_code = 8;
    
    while (true) {
        // Clear data
        for (int i = 0; i < 8; i++) {
            msg.data[i] = 0x00;
        }
        
        // Set current limit
        if (immobilizer.isUnlocked()) {
            msg.data[4] = 0xD0;
            msg.data[5] = 0x07;
        } else {
            msg.data[4] = 0x00;
            msg.data[5] = 0x00;
        }
        
        // Send
        twai_transmit(&msg, pdMS_TO_TICKS(10));
        
        // Wait for next period
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

// In setup():
xTaskCreatePinnedToCore(
    canHeartbeatTask,     // Task function
    "CAN_Heartbeat",      // Name
    4096,                 // Stack size
    NULL,                 // Parameters
    1,                    // Priority
    NULL,                 // Task handle
    0                     // Core 0 (Core 1 runs Arduino loop)
);
```

This runs on **Core 0** while main loop runs on **Core 1**, providing complete isolation.

---

## Testing:

**Verify heartbeat is working:**

1. Upload code with hardware timer
2. Open Serial Monitor
3. Navigate menus, scroll through screens
4. **Monitor ZombieVerter for BMS timeout**
5. Should **NOT fault** even when:
   - Scrolling screens
   - Entering PIN
   - Editing parameters
   - System is "busy"

**CAN bus monitoring:**
```
Use CAN analyzer to verify:
- Message 0x351 appears every 100ms ± 5ms
- Never stops or delays
- Continues even during menu navigation
```

---

## Summary:

**Problem:** CAN heartbeat was tied to `loop()`, could be blocked by UI
**Solution:** Use hardware timer interrupt or FreeRTOS task
**Result:** 0x351 always sends every 100ms, no BMS timeout faults

**Critical for safety:**
- Vehicle won't shut down unexpectedly
- Driver can navigate menus safely
- Immobilizer works reliably

---

## Recommended Implementation:

**Use hardware timer (esp_timer):**
- Simpler than RTOS task
- Guaranteed timing
- Low overhead
- Perfect for this use case

**Code to add:**
1. Include `<esp_timer.h>`
2. Add `sendCanHeartbeat()` callback
3. Create timer in `setup()`
4. Remove manual heartbeat from `loop()`

**This is a CRITICAL FIX for production use!**
