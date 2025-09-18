#include "slave_clock.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <time.h> 

static const char* TAG = "SLAVE_CLOCK";

// Konstruktor
SlaveClock::SlaveClock(gpio_num_t enable_pin, gpio_num_t input1_pin, gpio_num_t input2_pin,
                     int pulse_width_ms, int pulse_interval_ms)
    : _enable_pin(enable_pin),
      _input1_pin(input1_pin),
      _input2_pin(input2_pin),
      _pulse_width_ms(pulse_width_ms),
      _pulse_interval_ms(pulse_interval_ms),
      _clock_time(0),
      _polarity_level(0)
{
    ESP_LOGI(TAG, "SlaveClock-Objekt wird erstellt.");
    _init_gpio();

    // Dummy-Impulse senden, um den Motor zu initialisieren
    ESP_LOGI(TAG, "Sende 2 Initialisierungsimpulse zur Motor-Polarisierung...");
    sendPulses(2);
}

// Private Helferfunktion zur GPIO-Initialisierung
void SlaveClock::_init_gpio() {
    gpio_reset_pin(_enable_pin);
    gpio_set_direction(_enable_pin, GPIO_MODE_OUTPUT);
    gpio_reset_pin(_input1_pin);
    gpio_set_direction(_input1_pin, GPIO_MODE_OUTPUT);
    gpio_reset_pin(_input2_pin);
    gpio_set_direction(_input2_pin, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "GPIOs initialisiert.");
}

// Öffentliche Methode zum Senden von Impulsen
void SlaveClock::sendPulses(int count) {
    _send_pulses_internal(count);
}

// Interne Methode, die die eigentliche Arbeit macht
void SlaveClock::_send_pulses_internal(int count) {
    if (count <= 0) return;
    ESP_LOGI(TAG, "Sende %d Impuls(e)...", count);

    for (int i = 0; i < count; i++) {
        // Polarität für Schrittmotor umkehren
        gpio_set_level(_input1_pin, _polarity_level);
        gpio_set_level(_input2_pin, !_polarity_level);

        // Enable-Pin für die Dauer des Impulses aktivieren
        gpio_set_level(_enable_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(_pulse_width_ms));
        
        gpio_set_level(_enable_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(_pulse_interval_ms));  

        // Polarität für den nächsten Impuls wechseln
        _polarity_level = !_polarity_level;
    }
}

// Setzt die Startzeit der Uhr
void SlaveClock::setTime(uint8_t hour, uint8_t minute) {
    struct tm real_time_info;
    time_t now;
    time(&now);
    localtime_r(&now, &real_time_info);

    struct tm clock_time_info = real_time_info;
    clock_time_info.tm_hour = hour;
    clock_time_info.tm_min = minute;
    clock_time_info.tm_sec = 0;
    
    _clock_time = mktime(&clock_time_info);

    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%d.%m.%Y %H:%M:%S", &clock_time_info);
    ESP_LOGI(TAG, "SlaveClock Start-Zeitstempel gesetzt auf: %s", time_buf);
}

// Prüft die Zeit und aktualisiert die Uhr
void SlaveClock::update() {
    time_t now;
    time(&now);
    
    // Prüfen, ob die Zeit gültig ist (nach 2023)
    if (now < 1672531200) {
        return; 
    }

    // Puffer, um Datum und Zeit aufzunehmen (z.B. "DD.MM.YYYY HH:MM:SS\0")
    char real_time_str[30];
    char clock_time_str[30];
    struct tm timeinfo;

    // 1. Reale Systemzeit mit Datum formatieren
    localtime_r(&now, &timeinfo);
    strftime(real_time_str, sizeof(real_time_str), "%d.%m.%Y %H:%M:%S", &timeinfo);

    // 2. Intern gespeicherte Zeit der Uhr mit Datum formatieren
    localtime_r(&_clock_time, &timeinfo);
    strftime(clock_time_str, sizeof(clock_time_str), "%d.%m.%Y %H:%M:%S", &timeinfo);

    // 3. Beide Zeiten ausgeben
    ESP_LOGI(TAG, "Reale Zeit = %s, Angezeigte Zeit = %s", real_time_str, clock_time_str);

    time_t current_minute_floored = (now / 60) * 60;

    if (_clock_time < current_minute_floored) {
        int minutes_to_move = (current_minute_floored - _clock_time) / 60;
        ESP_LOGI(TAG, "Uhr geht %d Minute(n) nach. Sende Impuls(e)...", minutes_to_move);
        
        _send_pulses_internal(minutes_to_move);
        
        _clock_time = current_minute_floored;
        ESP_LOGI(TAG, "Zeit ist wieder synchron.");
    }
}