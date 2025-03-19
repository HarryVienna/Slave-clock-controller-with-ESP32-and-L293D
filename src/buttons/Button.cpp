#include "Button.h"

// Debounce time in milliseconds
const unsigned long Button::DEBOUNCE_TIME = 50;

// Definition des Schwellenwerts für einen langen Klick
const unsigned long Button::LONG_PRESS_THRESHOLD = 500;

// Repeat interval for long press in milliseconds
const unsigned long Button::REPEAT_INTERVAL = 200;


// Konstruktor der Button-Klasse
Button::Button(int pin)
    : pin(pin) {
    // Konfiguriert den Pin als Eingang mit Pull-up-Widerstand
    pinMode(pin, INPUT_PULLUP);
    // Erstellt einen neuen Task (Thread), der die buttonTask-Methode ausführt
    // und übergibt die Button-Instanz als Parameter
    xTaskCreatePinnedToCore(
        [](void *parameter) {
            static_cast<Button *>(parameter)->buttonTask();
        },
        "buttonTask", // Name des Tasks
        2048,        // Stackgröße des Tasks
        this,        // Parameter, der dem Task übergeben wird (die Button-Instanz selbst)
        1,           // Priorität des Tasks
        &taskHandle, // Handle, in dem das Task-Handle gespeichert wird
        0            // Core-ID, an den der Task gebunden ist (0 oder 1)
    );
}

// Destruktor der Button-Klasse
Button::~Button() {
    // Überprüft, ob ein Task-Handle vorhanden ist
    if (taskHandle != nullptr) {
        // Löscht den Task, wenn er existiert
        vTaskDelete(taskHandle);
    }
}

// Methoden zum dynamischen Setzen der Callback-Funktionen
void Button::setShortPressCallback(void (*shortPressCallback)()) {
    this->shortPressCallback = shortPressCallback;
}

void Button::setLongPressCallback(void (*longPressCallback)()) {
    this->longPressCallback = longPressCallback;
}

void Button::waitForAnyClick() {
    clickDetected = false;
    while (!clickDetected) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Methode, die im Task ausgeführt wird, um den Button-Status zu überwachen
void Button::buttonTask() {
    // Speichert den letzten bekannten Zustand des Buttons
    int lastState = digitalRead(pin);
    // Zeitpunkt des letzten Entprellungsereignisses
    unsigned long lastDebounceTime = 0;
    // Zeitpunkt, an dem der Button gedrückt wurde
    unsigned long pressStartTime = 0;
    // Zeitpunkt, an dem ein langer Klick zuletzt wiederholt wurde
    unsigned long lastRepeatTime = 0;
    // Flag, um zu kennzeichnen, ob ein langer Klick ausgelöst wurde
    bool longPressTriggered = false;

    // Endlosschleife, die den Button-Status kontinuierlich überwacht
    while (true) {
        // Aktuellen Zustand des Buttons lesen
        int currentState = digitalRead(pin);
        // Aktuelle Zeit in Millisekunden
        unsigned long now = millis();

        // Entprellung: Überprüfen, ob der Zustand sich geändert hat und die Entprellungszeit abgelaufen ist
        if (currentState != lastState && (now - lastDebounceTime) > DEBOUNCE_TIME) {
            // Entprellungszeit aktualisieren
            lastDebounceTime = now;
            // Letzten bekannten Zustand aktualisieren
            lastState = currentState;

            // Button wurde gedrückt
            if (currentState == LOW) {
                // Zeitpunkt des Drückens speichern
                pressStartTime = now;
                // Flag für langen Klick zurücksetzen
                longPressTriggered = false;
            }
            // Button wurde losgelassen
            else {
                // Dauer des Drückens berechnen
                unsigned long pressDuration = now - pressStartTime;
                // Wenn kein langer Klick ausgelöst wurde und die Dauer kurz ist, kurzen Klick auslösen
                if (!longPressTriggered && pressDuration < LONG_PRESS_THRESHOLD) {
                    // Kurzen Klick auslösen
                    callShortPressCallback();
                }
                // Flag für langen Klick zurücksetzen
                longPressTriggered = false;
            }
        }

        // Long press and repeat
        // Überprüfen, ob der Button gedrückt ist und die Dauer des Drückens den Schwellenwert überschreitet
        if (currentState == LOW && (now - pressStartTime) > LONG_PRESS_THRESHOLD) {
            // Wenn noch kein langer Klick ausgelöst wurde
            if (!longPressTriggered) {
                // Flag setzen, um zu kennzeichnen, dass ein langer Klick ausgelöst wurde
                longPressTriggered = true;
                // Langen Klick auslösen
                callLongPressCallback();
                // Zeitpunkt der letzten Wiederholung aktualisieren
                lastRepeatTime = now;
            }
            // Wenn ein langer Klick bereits ausgelöst wurde und das Wiederholungsintervall abgelaufen ist
            else if ((now - lastRepeatTime) > REPEAT_INTERVAL) {
                // Langen Klick erneut auslösen
                callLongPressCallback();
                // Zeitpunkt der letzten Wiederholung aktualisieren
                lastRepeatTime = now;
            }
        }
        // Kleine Verzögerung, um die CPU-Auslastung zu reduzieren
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void Button::callShortPressCallback() {
    clickDetected = true;
    if (shortPressCallback) {
        shortPressCallback();
    }
}

void Button::callLongPressCallback() {
    clickDetected = true;
    if (longPressCallback) {
        longPressCallback();
    }
}