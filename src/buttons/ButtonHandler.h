#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include <functional>

class ButtonHandler {
public:
    // Constructor
    ButtonHandler(uint8_t pinA, uint8_t pinB);

    // Set callback for move function
    void setMoveCallback(std::function<void()> moveCallback);

    // Blocking control of the buttons
    void start();

private:
    // Pins for the buttons
    uint8_t pinA;
    uint8_t pinB;

    // Debouncing and status variables
    bool buttonAState;
    bool buttonBState;
    bool longPressActiveA;

    unsigned long lastPressTimeA;
    unsigned long lastPressTimeB;
    unsigned long longPressRepeatTimeA;

    // Debounce time and long press delay
    const unsigned long debounceDelay = 50;      // 50 ms debounce time
    const unsigned long longPressDelay = 500;    // 500 ms until long press starts

    // Callback function for “Move”
    std::function<void()> moveCallback;

    // Blocking status
    bool isRunning;

    // Private methods for key processing
    void handleButtonA();
    void handleButtonB();
};

#endif // BUTTON_HANDLER_H