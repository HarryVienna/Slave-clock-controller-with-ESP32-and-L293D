#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class Button {
public:
    // Konstruktor der Button-Klasse
    // Initialisiert den Button mit Pin
    Button(int pin);
    // Destruktor der Button-Klasse
    // Löscht den Task, wenn das Button-Objekt zerstört wird
    ~Button();

    // Methoden zum dynamischen Setzen der Callback-Funktionen
    void setShortPressCallback(void (*shortPressCallback)());
    void setLongPressCallback(void (*longPressCallback)());
    void waitForAnyClick();

private:
    // Pin, an dem der Button angeschlossen ist
    int pin;

    // Callback-Funktion für kurze Klicks
    void (*shortPressCallback)();
    // Callback-Funktion für lange Klicks
    void (*longPressCallback)();

    // Flag, das von den Callback-Funktionen gesetzt wird
    volatile bool clickDetected; 


    // Schwellenwert für einen langen Klick in Millisekunden
    static const unsigned long LONG_PRESS_THRESHOLD;
    // Entprellungszeit in Millisekunden
    static const unsigned long DEBOUNCE_TIME;
    // Wiederholungsintervall für lange Klicks in Millisekunden
    static const unsigned long REPEAT_INTERVAL;


    // Handle für den Task, der den Button-Status überwacht
    TaskHandle_t taskHandle;

    // Methode, die in einem separaten Thread ausgeführt wird, um den Button-Status zu überwachen
    void buttonTask();
    void callShortPressCallback();
    void callLongPressCallback();
};

#endif