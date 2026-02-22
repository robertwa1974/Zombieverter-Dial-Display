#include "InputManager.h"
#include "Config.h"
#include <M5Unified.h>

InputManager* InputManager::instance = nullptr;

InputManager::InputManager() 
    : lastEncoderPosition(0), touchPressed(false), 
      lastTouchX(0), lastTouchY(0), touchPressTime(0),
      eventHead(0), eventTail(0),
      onEncoderRotate(nullptr), onButtonClick(nullptr),
      onButtonDoubleClick(nullptr), onButtonLongPress(nullptr),
      onTouchTap(nullptr),
      button(ENCODER_BUTTON, true, true) {
    instance = this;
}

bool InputManager::init() {
    // Initialize encoder
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachFullQuad(ENCODER_PIN_A, ENCODER_PIN_B);
    encoder.clearCount();
    encoder.setCount(0);
    lastEncoderPosition = 0;
    
    // Initialize button
    button.attachClick(buttonClickCallback);
    button.attachDoubleClick(buttonDoubleClickCallback);
    button.attachLongPressStart(buttonLongPressCallback);
    button.setClickMs(250);
    button.setPressMs(800);
    
    #if DEBUG_SERIAL
    Serial.println("Input Manager initialized");
    #endif
    
    return true;
}

void InputManager::update() {
    // Update button
    button.tick();
    
    // Check encoder with sensitivity adjustment
    // The encoder is in 4x quadrature mode, so divide by 4 for proper sensitivity
    int32_t rawPosition = encoder.getCount();
    int32_t currentPosition = rawPosition / 4;  // Divide by 4 to get actual detent clicks
    
    // Debug every 2 seconds (not 500ms)
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 2000) {
        Serial.printf("[ENC] raw=%d divided=%d last=%d\n", rawPosition, currentPosition, lastEncoderPosition);
        lastDebugTime = millis();
    }
    
    if (currentPosition != lastEncoderPosition) {
        int32_t delta = currentPosition - lastEncoderPosition;
        lastEncoderPosition = currentPosition;
        
        Serial.println("========================================");
        Serial.printf("ENCODER ROTATED: raw_delta=%d, actual_delta=%d\n", rawPosition, delta);
        Serial.println("========================================");
        
        InputEvent event;
        event.type = (delta > 0) ? INPUT_ENCODER_CW : INPUT_ENCODER_CCW;
        event.encoderDelta = delta;
        event.timestamp = millis();
        enqueueEvent(event);
        
        if (onEncoderRotate) {
            onEncoderRotate(delta);
        }
    }
    
    // Check touch
    checkTouch();
}

void InputManager::checkTouch() {
    M5.update(); // Update touch state
    
    auto touch = M5.Touch.getDetail();
    
    if (touch.wasPressed()) {
        touchPressed = true;
        lastTouchX = touch.x;
        lastTouchY = touch.y;
        touchPressTime = millis();
        
        InputEvent event;
        event.type = INPUT_TOUCH_PRESS;
        event.touchX = touch.x;
        event.touchY = touch.y;
        event.timestamp = millis();
        enqueueEvent(event);
        
        #if DEBUG_TOUCH
        Serial.printf("Touch press: %d, %d\n", touch.x, touch.y);
        #endif
    }
    else if (touch.wasReleased()) {
        InputEvent event;
        event.type = INPUT_TOUCH_RELEASE;
        event.touchX = touch.x;
        event.touchY = touch.y;
        event.timestamp = millis();
        enqueueEvent(event);
        
        // Check for tap (quick press/release)
        if (millis() - touchPressTime < 300) {
            InputEvent tapEvent;
            tapEvent.type = INPUT_TOUCH_TAP;
            tapEvent.touchX = touch.x;
            tapEvent.touchY = touch.y;
            tapEvent.timestamp = millis();
            enqueueEvent(tapEvent);
            
            if (onTouchTap) {
                onTouchTap(touch.x, touch.y);
            }
            
            #if DEBUG_TOUCH
            Serial.printf("Touch tap: %d, %d\n", touch.x, touch.y);
            #endif
        }
        
        touchPressed = false;
    }
}

bool InputManager::hasEvent() {
    return eventHead != eventTail;
}

InputEvent InputManager::getNextEvent() {
    InputEvent event;
    event.type = INPUT_NONE;
    dequeueEvent(event);
    return event;
}

int32_t InputManager::getEncoderPosition() {
    return encoder.getCount();
}

void InputManager::resetEncoderPosition() {
    encoder.clearCount();
    encoder.setCount(0);
    lastEncoderPosition = 0;
}

void InputManager::setOnEncoderRotate(void (*callback)(int32_t)) {
    onEncoderRotate = callback;
}

void InputManager::setOnButtonClick(void (*callback)()) {
    onButtonClick = callback;
}

void InputManager::setOnButtonDoubleClick(void (*callback)()) {
    onButtonDoubleClick = callback;
}

void InputManager::setOnButtonLongPress(void (*callback)()) {
    onButtonLongPress = callback;
}

void InputManager::setOnTouchTap(void (*callback)(uint16_t, uint16_t)) {
    onTouchTap = callback;
}

bool InputManager::enqueueEvent(InputEvent& event) {
    uint8_t next = (eventHead + 1) % 8;
    if (next == eventTail) return false;
    
    eventQueue[eventHead] = event;
    eventHead = next;
    return true;
}

bool InputManager::dequeueEvent(InputEvent& event) {
    if (eventHead == eventTail) return false;
    
    event = eventQueue[eventTail];
    eventTail = (eventTail + 1) % 8;
    return true;
}

void InputManager::buttonClickCallback() {
    if (instance && instance->onButtonClick) {
        instance->onButtonClick();
    }
    
    if (instance) {
        InputEvent event;
        event.type = INPUT_BUTTON_CLICK;
        event.timestamp = millis();
        instance->enqueueEvent(event);
    }
}

void InputManager::buttonDoubleClickCallback() {
    if (instance && instance->onButtonDoubleClick) {
        instance->onButtonDoubleClick();
    }
    
    if (instance) {
        InputEvent event;
        event.type = INPUT_BUTTON_DOUBLE_CLICK;
        event.timestamp = millis();
        instance->enqueueEvent(event);
    }
}

void InputManager::buttonLongPressCallback() {
    if (instance && instance->onButtonLongPress) {
        instance->onButtonLongPress();
    }
    
    if (instance) {
        InputEvent event;
        event.type = INPUT_BUTTON_LONG_PRESS;
        event.timestamp = millis();
        instance->enqueueEvent(event);
    }
}
