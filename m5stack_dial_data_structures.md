# ZombieVerter Display - M5Stack Dial Port
## Data Structure Design Document

---

## Hardware Differences Summary

| Feature | Lilygo T-Embed | M5Stack Dial |
|---------|---------------|--------------|
| Display | 170x320 TFT | 240x240 Round TFT (GC9A01) |
| Encoder | Rotary encoder | Rotary encoder + touch support |
| Button | Physical button | Touch screen + encoder button |
| CAN Bus | External (requires backpack) | External (requires CANBUS Unit) |
| MCU | ESP32-S3 | ESP32-S3 |
| LVGL Support | Yes | Yes |

---

## Core Data Structures

### 1. Hardware Abstraction Layer (HAL)

```cpp
// Hardware configuration structure
struct HardwareConfig {
    // Display settings
    struct Display {
        uint16_t width;
        uint16_t height;
        bool isRound;           // New: True for M5Stack Dial
        uint8_t rotation;
        uint16_t centerX;       // For round display calculations
        uint16_t centerY;
    } display;
    
    // Input device settings
    struct Input {
        // Encoder
        uint8_t encoderPinA;
        uint8_t encoderPinB;
        uint8_t encoderButton;
        bool hasTouch;          // New: M5Stack Dial has touch
        
        // Touch (M5Stack Dial specific)
        uint8_t touchSDA;
        uint8_t touchSCL;
        uint8_t touchInt;
        uint8_t touchRst;
    } input;
    
    // CAN Bus
    struct CANBus {
        uint8_t txPin;
        uint8_t rxPin;
        uint32_t baudRate;
    } canBus;
    
    // Power
    struct Power {
        uint8_t powerPin;
        bool hasBacklight;
        uint8_t backlightPin;
    } power;
};
```

### 2. UI Layout Manager (Circular Display Adaptation)

```cpp
// Position calculation for round displays
struct CircularLayout {
    // Zone definitions for round display
    enum Zone {
        CENTER,           // Main data display
        TOP_ARC,         // Header/status
        BOTTOM_ARC,      // Footer/controls
        LEFT_ARC,        // Additional info
        RIGHT_ARC        // Additional info
    };
    
    // Coordinate mapping
    struct Position {
        uint16_t x;
        uint16_t y;
        uint16_t radius;    // Distance from center
        float angle;        // Angle in radians
    };
    
    // Convert rectangular coords to circular
    Position rectangularToCircular(uint16_t rectX, uint16_t rectY) {
        Position pos;
        pos.x = rectX;
        pos.y = rectY;
        
        uint16_t dx = rectX - centerX;
        uint16_t dy = rectY - centerY;
        
        pos.radius = sqrt(dx*dx + dy*dy);
        pos.angle = atan2(dy, dx);
        
        return pos;
    }
    
    // Place widget in zone
    Position getZonePosition(Zone zone, uint8_t itemIndex, uint8_t totalItems) {
        Position pos;
        // Calculate position based on zone and item arrangement
        // ... implementation details
        return pos;
    }
    
private:
    uint16_t centerX;
    uint16_t centerY;
    uint16_t displayRadius;
};
```

### 3. Screen Manager (Adapted for Circular Display)

```cpp
// Screen data structure
struct Screen {
    uint8_t id;
    const char* name;
    
    // Layout type
    enum LayoutType {
        RADIAL,          // Circular arrangement
        GAUGE,           // Single large gauge
        LIST,            // Scrollable list (uses center area)
        GRID_2x2,        // 4 quadrant layout
        CUSTOM
    } layout;
    
    // Widget collection
    struct Widget {
        uint8_t id;
        enum Type {
            LABEL,
            VALUE,
            GAUGE,
            ARC_INDICATOR,   // New: Circular progress
            RING_GAUGE,      // New: Ring around edge
            BUTTON,
            SLIDER
        } type;
        
        // Position (can be zone-based or absolute)
        CircularLayout::Zone zone;
        CircularLayout::Position position;
        
        // Data binding
        uint16_t paramId;           // ZombieVerter parameter ID
        const char* label;
        char value[32];
        
        // Styling
        lv_color_t color;
        uint16_t fontSize;
        bool visible;
        
        // For gauges/arcs
        int32_t min;
        int32_t max;
        int32_t current;
        float startAngle;           // For arc placement
        float endAngle;
        
        // Interactivity
        bool editable;
        bool focused;
    } widgets[MAX_WIDGETS_PER_SCREEN];
    
    uint8_t widgetCount;
    uint8_t focusedWidget;
};

// Screen collection manager
struct ScreenManager {
    Screen screens[MAX_SCREENS];
    uint8_t screenCount;
    uint8_t currentScreen;
    
    // Screen transition
    void switchScreen(uint8_t newScreenId);
    void nextScreen();
    void previousScreen();
    
    // Widget management
    void updateWidget(uint8_t screenId, uint8_t widgetId, const char* value);
    void focusNextWidget();
    void focusPreviousWidget();
};
```

### 4. CAN Data Handler (Compatible with existing ZombieVerter)

```cpp
// CAN parameter structure
struct CANParameter {
    uint16_t id;                    // SDO parameter ID
    const char* name;
    
    enum DataType {
        INT8,
        UINT8,
        INT16,
        UINT16,
        INT32,
        UINT32,
        FLOAT
    } dataType;
    
    union {
        int8_t i8;
        uint8_t u8;
        int16_t i16;
        uint16_t u16;
        int32_t i32;
        uint32_t u32;
        float f32;
    } value;
    
    // For editable parameters
    bool editable;
    union {
        int32_t i;
        float f;
    } min, max;
    
    // Display formatting
    const char* unit;
    uint8_t decimalPlaces;
    
    // Update tracking
    uint32_t lastUpdateTime;
    bool dirty;                     // Needs update
};

// CAN message queue
struct CANMessage {
    uint32_t id;
    uint8_t data[8];
    uint8_t length;
    uint32_t timestamp;
};

// CAN Data Manager
struct CANDataManager {
    CANParameter parameters[MAX_PARAMETERS];
    uint16_t parameterCount;
    
    // Message queues
    CANMessage txQueue[TX_QUEUE_SIZE];
    CANMessage rxQueue[RX_QUEUE_SIZE];
    uint8_t txHead, txTail;
    uint8_t rxHead, rxTail;
    
    // Methods
    void requestParameter(uint16_t paramId);
    void setParameter(uint16_t paramId, int32_t value);
    bool processIncomingMessage(CANMessage& msg);
    void updateFromJSON(const char* jsonString);  // From params.json
    
    // Get parameter by ID
    CANParameter* getParameter(uint16_t id);
};
```

### 5. Input Handler (Touch + Encoder)

```cpp
// Input event structure
struct InputEvent {
    enum Type {
        ENCODER_ROTATE_CW,
        ENCODER_ROTATE_CCW,
        ENCODER_BUTTON_CLICK,
        ENCODER_BUTTON_DOUBLE_CLICK,
        ENCODER_BUTTON_LONG_PRESS,
        TOUCH_PRESS,
        TOUCH_RELEASE,
        TOUCH_DRAG,
        TOUCH_TAP
    } type;
    
    // Touch coordinates (for M5Stack Dial)
    struct TouchData {
        uint16_t x;
        uint16_t y;
        bool isPressed;
        uint32_t timestamp;
    } touch;
    
    // Encoder data
    struct EncoderData {
        int32_t position;
        int32_t delta;
        bool buttonPressed;
    } encoder;
    
    uint32_t timestamp;
};

// Input manager
struct InputManager {
    // Hardware interfaces
    RotaryEncoder* encoder;
    FT6336U* touchScreen;      // M5Stack Dial touch controller
    
    // State tracking
    InputEvent lastEvent;
    int32_t encoderPosition;
    bool encoderButtonState;
    
    // Callbacks
    void (*onEncoderRotate)(int32_t delta);
    void (*onEncoderClick)();
    void (*onEncoderDoubleClick)();
    void (*onEncoderLongPress)();
    void (*onTouchTap)(uint16_t x, uint16_t y);
    void (*onTouchDrag)(uint16_t x, uint16_t y);
    
    // Methods
    void update();
    InputEvent getNextEvent();
    bool hasEvent();
};
```

### 6. Configuration Storage

```cpp
// Persistent configuration
struct Configuration {
    // Display preferences
    struct DisplayPrefs {
        uint8_t brightness;
        uint16_t sleepTimeout;      // Minutes
        bool autoRotate;
        uint8_t defaultScreen;
    } display;
    
    // CAN Bus settings
    struct CANSettings {
        uint32_t baudRate;
        uint8_t nodeId;
        bool autoConnect;
    } can;
    
    // WiFi (for params.json upload)
    struct WiFiSettings {
        char ssid[32];
        char password[64];
        bool apMode;
        char apSSID[32];
        char apPassword[64];
    } wifi;
    
    // Screen layout customization
    struct LayoutPrefs {
        uint8_t screenOrder[MAX_SCREENS];
        bool enabledScreens[MAX_SCREENS];
    } layout;
    
    // Checksum for validation
    uint32_t checksum;
};
```

### 7. State Machine

```cpp
// Application states
enum AppState {
    STATE_BOOT,
    STATE_SPLASH,
    STATE_CONNECTING_CAN,
    STATE_NORMAL_OPERATION,
    STATE_EDITING_PARAMETER,
    STATE_MENU,
    STATE_SETTINGS,
    STATE_ERROR,
    STATE_SLEEP
};

// State manager
struct StateManager {
    AppState currentState;
    AppState previousState;
    uint32_t stateEntryTime;
    
    // State transition
    void transitionTo(AppState newState);
    void handleState();
    
    // State-specific handlers
    void handleNormalOperation();
    void handleEditingParameter();
    void handleMenu();
    void handleSettings();
};
```

---

## Migration Strategy

### Phase 1: Hardware Abstraction
1. Create `HardwareConfig` for M5Stack Dial
2. Implement `CircularLayout` helper class
3. Port pin definitions to M5Stack Dial

### Phase 2: UI Redesign
1. Convert rectangular screens to circular layouts
2. Redesign gauges as arc indicators
3. Implement touch support alongside encoder
4. Update LVGL theme for round display

### Phase 3: Input System
1. Add touch screen driver (FT6336U)
2. Combine touch + encoder events
3. Implement gesture recognition (optional)

### Phase 4: Testing & Refinement
1. Test CAN communication
2. Verify parameter editing
3. Optimize for round display
4. Performance tuning

---

## Key Circular Display Patterns

### Pattern 1: Radial Menu
```
        [Item 1]
   [Item 4]  ⊙  [Item 2]
        [Item 3]
```

### Pattern 2: Ring Gauges
```
    ╭────────╮
   │  Speed  │
   │  ⊙ 45  │
   │  mph   │
    ╰────────╯
   [Arc around edge shows 0-100%]
```

### Pattern 3: Center Focus + Context Arcs
```
   [Battery SOC Arc]
        ╭───╮
        │ 80│ <- Main value
        │ kW│
        ╰───╯
   [Motor Temp Arc]
```

---

## File Structure Recommendation

```
M5StackDial_ZombieVerter/
├── src/
│   ├── main.cpp
│   ├── hardware/
│   │   ├── HardwareConfig.h
│   │   ├── M5DialHAL.cpp
│   │   └── M5DialHAL.h
│   ├── ui/
│   │   ├── CircularLayout.h
│   │   ├── CircularLayout.cpp
│   │   ├── ScreenManager.h
│   │   ├── ScreenManager.cpp
│   │   └── screens/
│   │       ├── DashboardScreen.cpp
│   │       ├── DetailScreen.cpp
│   │       └── SettingsScreen.cpp
│   ├── input/
│   │   ├── InputManager.h
│   │   └── InputManager.cpp
│   ├── can/
│   │   ├── CANDataManager.h
│   │   ├── CANDataManager.cpp
│   │   ├── CanSDO.h
│   │   └── CanSDO.cpp
│   └── utils/
│       ├── StateManager.h
│       └── Config.h
├── data/
│   └── params.json
└── platformio.ini
```

---

## Next Steps

1. **Do you want me to generate actual C++ header files** with these structures?
2. **Should I create a visual mockup** of the circular UI layouts?
3. **Do you need help with the LVGL configuration** for the round display?
4. **Would you like a pin mapping document** comparing T-Embed to M5Stack Dial?

Let me know which direction would be most helpful!
