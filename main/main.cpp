#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "lilygo-t-display.h"
#include "slave_clock.hpp"
#include "wifi_provisioner.hpp"

#include <time.h>

#include "lvgl.h"
#include "ui/ui.h"

#include "config.h"


static const char* TAG = "MAIN_APP";

extern SemaphoreHandle_t lvgl_mux;

extern "C" void app_main(void) {

    // Init display and LVGL
    display_init();

    // Init UI
    if (lvgl_lock(-1)) {

        ui_init();

        lvgl_unlock();
    }

    SlaveClock clock(PULSE_GPIO_ENABLE, PULSE_GPIO_INPUT1, PULSE_GPIO_INPUT2,
                    PULSE_WIDTH_MS, PULSE_INTERVAL_MS);

    WifiProvisioner provisioner;

    if (provisioner.is_provisioned()) {
        provisioner.get_credentials();
    } else {
        show_message("Start Provisioning", false);
        provisioner.start_provisioning("Slave Clock Setup", false);
    }

    show_message("Connecting to WiFi", false);
    provisioner.connect_sta("Slave Clock");

    show_message("Synching time", false);
    while(!provisioner.is_time_synchronized()) {

        ESP_LOGI(TAG, "Zeit ist noch nicht mit dem NTP-Server synchronisiert.");

        // Warte eine Sekunde bis zur nächsten Ausgabe
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    show_message("Time synched", false); 
    clock.setTime(provisioner.get_provisioned_hour(), provisioner.get_provisioned_minute());

    // Hauptschleife
    while (true) {

        time_t now;
        time(&now);

        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        show_time(timeinfo.tm_hour, timeinfo.tm_min);

        clock.update();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}