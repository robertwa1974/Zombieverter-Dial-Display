# MINIMAL TEST - Disable Immobilizer Temporarily

To test if the immobilizer is causing the hang, try this:

## In src/main.cpp, comment out immobilizer:

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ZombieVerter Display - M5Stack Dial");
    
    Hardware::init();
    Serial.println("Hardware OK");
    
    inputManager.init();
    Serial.println("Input OK");
    
    uiManager.init(&canManager, nullptr);  // Pass nullptr for immobilizer
    Serial.println("UI OK");
    
    // Skip immobilizer entirely
    // immobilizer.init();
    
    // Force unlock and go to dashboard
    uiManager.setScreen(SCREEN_DASHBOARD);
    Serial.println("Going to dashboard");
    
    Serial.println("Setup complete!");
}

void loop() {
    Hardware::update();
    inputManager.update();
    
    // Process input events
    while (inputManager.hasEvent()) {
        InputEvent event = inputManager.getNextEvent();
        Serial.printf("Event: %d\n", event.type);
        
        switch (event.type) {
            case INPUT_ENCODER_CW:
                Serial.println("CW!");
                uiManager.setScreen(uiManager.getNextScreen());
                break;
            case INPUT_ENCODER_CCW:
                Serial.println("CCW!");
                uiManager.setScreen(uiManager.getPreviousScreen());
                break;
            case INPUT_BUTTON_CLICK:
                Serial.println("Click!");
                break;
            default:
                break;
        }
    }
    
    uiManager.update();
    delay(10);
}
```

This will tell us if:
1. Encoder works (you'll see "CW!" / "CCW!" in Serial)
2. Navigation works (screens change)
3. Immobilizer is the problem (if this works but full version doesn't)
