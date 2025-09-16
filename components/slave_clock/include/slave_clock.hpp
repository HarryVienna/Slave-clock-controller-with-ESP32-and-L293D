#ifndef SLAVECLOCK_HPP
#define SLAVECLOCK_HPP

#include "driver/gpio.h"
#include <time.h>

class SlaveClock {
public:
    // --- Öffentliche Schnittstelle ---

    // Konstruktor: Initialisiert die Uhr mit den nötigen Pins und Timings
    SlaveClock(gpio_num_t enable_pin, gpio_num_t input1_pin, gpio_num_t input2_pin,
              int pulse_width_ms, int pulse_interval_ms);

    // Setzt die Startzeit der Uhr (z.B. 10:30)
    void setTime(uint8_t hour, uint8_t minute);

    // Die Hauptfunktion, die in der Schleife aufgerufen wird.
    // Prüft, ob die Uhr nachgeht und aktualisiert sie bei Bedarf.
    void update();

    // Sendet eine bestimmte Anzahl von Impulsen (nützlich für Tests/Initialisierung)
    void sendPulses(int count);


private:
    // --- Interne Zustände und Helfer ---

    // Helferfunktion zur Initialisierung der GPIOs
    void _init_gpio();

    // Interne Logik zum Senden der Impulse
    void _send_pulses_internal(int count);

    // PINS & TIMINGS
    gpio_num_t _enable_pin;
    gpio_num_t _input1_pin;
    gpio_num_t _input2_pin;
    int _pulse_width_ms;
    int _pulse_interval_ms;

    // ZUSTAND
    time_t _clock_time;     // Der Zeitstempel, den die Uhr physisch anzeigt
    int _polarity_level;    // Die aktuelle Polarität für den Schrittmotor (0 oder 1)
};

#endif