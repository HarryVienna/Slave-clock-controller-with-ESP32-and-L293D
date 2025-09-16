
#include <sdkconfig.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"

#include "lvgl.h"
#include "ui/ui.h"

#include "config.h"



static const char *TAG = "TFT";

static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf;

SemaphoreHandle_t lvgl_mux = NULL;

bool lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

void set_backlight_brightness(uint16_t brightness) 
{

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void show_message(const char *message, bool is_error)
{
    if (lvgl_lock(-1)) {

        if (message && message[0] != '\0') {
            // Farbe basierend auf dem is_error-Flag setzen
            lv_obj_set_style_text_color(objects.message_label, is_error ? lv_color_hex(0xFF0000) : lv_color_black(), 0);
            // Text setzen
            lv_label_set_text(objects.message_label, message);
            // Label sichtbar machen
            lv_obj_clear_flag(objects.message_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Label ausblenden, wenn die Nachricht leer ist
            lv_obj_add_flag(objects.message_label, LV_OBJ_FLAG_HIDDEN);
        }

        lvgl_unlock();
    }
}

void show_time(int stunde, int minute) 
{

    if (lvgl_lock(-1)) {
        char buffer[6];
        snprintf(buffer, sizeof(buffer), "%02d:%02d", stunde, minute);

        // Zugriff über die globale 'objects'-Struktur
        lv_label_set_text(objects.time_label, buffer);

        lvgl_unlock();
    }
}

static void display_push_colors(uint16_t x, uint16_t y, uint16_t width, uint16_t hight, uint16_t *data)
{
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, width, hight, data);
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
#if DISPLAY_FULLRESH
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );
    display_push_colors(area->x1, area->y1, w, h, (uint16_t *)color_map);
    lv_disp_flush_ready( drv );
#else
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    display_push_colors(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, (uint16_t *)color_map);
#endif
}

static void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static bool display_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_flush_ready(&disp_drv);
    return false;
}



void display_init()
{

    ESP_LOGI(TAG, "============ Init Display ============");

    ESP_LOGI(TAG, "Turn on board power pin");
    gpio_config_t poweron_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BOARD_POWERON
    };
    ESP_ERROR_CHECK(gpio_config(&poweron_gpio_config));
    gpio_set_level(BOARD_POWERON, 1);

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_SPI_SCK,
        .mosi_io_num = BOARD_SPI_MOSI,
        .miso_io_num = BOARD_SPI_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_HEIGHT * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_TFT_DC,
        .cs_gpio_num = BOARD_TFT_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = display_notify_lvgl_flush_ready
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_TFT_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    
    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    esp_lcd_panel_swap_xy(panel_handle, true);
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    esp_lcd_panel_set_gap(panel_handle, 40, 53);


    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));


    ESP_LOGI(TAG, "Initialize LCD backlight");
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL,
        .duty = 0,
        .hpoint = 0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_PIN_NUM_BK_LIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER,
        .flags.output_invert = LEDC_OUTPUT_INVERT
    };

    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel);

    set_backlight_brightness(8);



    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();



    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISPLAY_HEIGHT * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t *) heap_caps_malloc(DISPLAY_HEIGHT * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISPLAY_HEIGHT * 20);


    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISPLAY_HEIGHT;
    disp_drv.ver_res = DISPLAY_WIDTH;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.full_refresh = DISPLAY_FULLRESH;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = false
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Create LVGL Semaphore");
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
}

