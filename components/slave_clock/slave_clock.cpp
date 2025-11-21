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
      _clock_hour(0),
      _clock_minute(0),
      _clock_day(0),
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
    
    // Setze die drei Werte direkt
    _clock_hour = hour;
    _clock_minute = minute;
    _clock_day = real_time_info.tm_yday;  // Tag des Jahres (0-365)
    
    ESP_LOGI(TAG, "SlaveClock Startzeit gesetzt auf: Tag %d, %02d:%02d", 
             _clock_day, _clock_hour, _clock_minute);
}

// Prüft die Zeit und aktualisiert die Uhr
void SlaveClock::update() {
    time_t now;
    time(&now);
    struct tm timeinfo_now;
    localtime_r(&now, &timeinfo_now);
    
    // Aktuelle lokale Zeit
    int current_total_minutes = timeinfo_now.tm_yday * 1440 + 
                                 timeinfo_now.tm_hour * 60 + 
                                 timeinfo_now.tm_min;
    
    // Was die Uhr anzeigt
    int clock_total_minutes = _clock_day * 1440 + 
                              _clock_hour * 60 + 
                              _clock_minute;
    
    int minutes_diff = current_total_minutes - clock_total_minutes;
    
    if (minutes_diff > 0) {
        ESP_LOGI(TAG, "Sende %d Impulse", minutes_diff);
        _send_pulses_internal(minutes_diff);
        
        // Aktualisiere Uhr-Position
        _clock_hour = timeinfo_now.tm_hour;
        _clock_minute = timeinfo_now.tm_min;
        _clock_day = timeinfo_now.tm_yday;
    } else if (minutes_diff < 0) {
        ESP_LOGI(TAG, "Warte %d Minuten", -minutes_diff);
    }
}