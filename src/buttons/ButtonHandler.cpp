#include "ButtonHandler.h"

ButtonHandler::ButtonHandler(uint8_t pinA, uint8_t pinB)
    : pinA(pinA), pinB(pinB), buttonAState(false), buttonBState(false),
      longPressActiveA(false), lastPressTimeA(0), lastPressTimeB(0),
      longPressRepeatTimeA(0), moveCallback(nullptr), isRunning(true) {
    // Initialize button pins
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
}

void ButtonHandler::setMoveCallback(std::function<void()> moveCallback) {
    this->moveCallback = moveCallback;
}

void ButtonHandler::start() {
    isRunning = true;
    while (isRunning) {
        handleButtonA();
        handleButtonB();
        delay(10); // Polling interval
    }
}

void ButtonHandler::handleButtonA() {
    bool currentState = !digitalRead(pinA); // Button is LOW when pressed
    unsigned long currentTime = millis();

    // Debouncing
    if (currentState != buttonAState && (currentTime - lastPressTimeA > debounceDelay)) {
        buttonAState = currentState;
        lastPressTimeA = currentTime;

        if (buttonAState) { // Button A was pressed
            longPressActiveA = false;
        } else { // Button A has been released
            if (!longPressActiveA && moveCallback) {
                moveCallback(); // Single execution with short click
            }
        }
    }

    // Check long print
    if (buttonAState && !longPressActiveA && (currentTime - lastPressTimeA > longPressDelay)) {
        longPressActiveA = true; // Long press started
        if (moveCallback) {
            moveCallback(); // First call for long press
        }
    }

    // Repetition rate for long print (every 100 ms)
    if (buttonAState && longPressActiveA && (currentTime - longPressRepeatTimeA >= 100)) { // Every 100 ms
        longPressRepeatTimeA = currentTime; // Set next repetition time
        if (moveCallback) {
            moveCallback(); // Repetition with long press
        }
    }
}

void ButtonHandler::handleButtonB() {
    bool currentState = !digitalRead(pinB); // Button is LOW when pressed
    unsigned long currentTime = millis();

    // Debouncing
    if (currentState != buttonBState && (currentTime - lastPressTimeB > debounceDelay)) {
        buttonBState = currentState;
        lastPressTimeB = currentTime;

        if (buttonBState) { // Button B pressed
            isRunning = false; // End loop
        }
    }
}