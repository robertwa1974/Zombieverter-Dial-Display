#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <OneButton.h>

enum InputEventType {
    INPUT_NONE,
    INPUT_ENCODER_CW,
    INPUT_ENCODER_CCW,
    INPUT_BUTTON_CLICK,
    INPUT_BUTTON_DOUBLE_CLICK,
    INPUT_BUTTON_TRIPLE_CLICK,
    INPUT_BUTTON_LONG_PRESS,
    INPUT_TOUCH_PRESS,
    INPUT_TOUCH_RELEASE,
    INPUT_TOUCH_TAP
};

struct InputEvent {
    InputEventType type;
    int32_t encoderDelta;
    uint16_t touchX;
    uint16_t touchY;
    uint32_t timestamp;
};

class InputManager {
public:
    InputManager();
    
    bool init();
    void update();
    
    bool hasEvent();
    InputEvent getNextEvent();
    
    // Encoder access
    int32_t getEncoderPosition();
    void resetEncoderPosition();
    
    // Callbacks
    void setOnEncoderRotate(void (*callback)(int32_t delta));
    void setOnButtonClick(void (*callback)());
    void setOnButtonDoubleClick(void (*callback)());
    void setOnButtonTripleClick(void (*callback)());
    void setOnButtonLongPress(void (*callback)());
    void setOnTouchPress(void (*callback)(uint16_t x, uint16_t y));
    void setOnTouchTap(void (*callback)(uint16_t x, uint16_t y));
    
private:
    ESP32Encoder encoder;
    OneButton button;
    
    int32_t lastEncoderPosition;
    bool touchPressed;
    uint16_t lastTouchX, lastTouchY;
    uint32_t touchPressTime;

    // Triple-click detection state
    uint8_t  clickCount;
    uint32_t lastClickTime;
    static const uint32_t TRIPLE_CLICK_WINDOW_MS = 600; // ms window for 3 clicks
    
    InputEvent eventQueue[8];
    uint8_t eventHead, eventTail;
    
    void (*onEncoderRotate)(int32_t delta);
    void (*onButtonClick)();
    void (*onButtonDoubleClick)();
    void (*onButtonTripleClick)();
    void (*onButtonLongPress)();
    void (*onTouchPress)(uint16_t x, uint16_t y);
    void (*onTouchTap)(uint16_t x, uint16_t y);
    
    bool enqueueEvent(InputEvent& event);
    bool dequeueEvent(InputEvent& event);
    
    void checkTouch();
    
    // Static callbacks for OneButton
    static void buttonClickCallback();
    static void buttonDoubleClickCallback();
    static void buttonTripleClickCallback();
    static void buttonLongPressCallback();
    
    static InputManager* instance;
};

#endif // INPUT_MANAGER_H
