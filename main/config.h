#pragma once

#include <sdkconfig.h>

#define BOARD_POWERON        GPIO_NUM_14
#define BOARD_SPI_MISO       GPIO_NUM_21
#define BOARD_SPI_MOSI       GPIO_NUM_19
#define BOARD_SPI_SCK        GPIO_NUM_18
#define BOARD_TFT_CS         GPIO_NUM_5
#define BOARD_TFT_RST        GPIO_NUM_23
#define BOARD_TFT_DC         GPIO_NUM_16


#define PWM_FREQ               200 
#define PWM_RESOLUTION         LEDC_TIMER_10_BIT 
#define LEDC_CHANNEL           LEDC_CHANNEL_0  
#define LEDC_TIMER             LEDC_TIMER_0 
#define LEDC_PIN_NUM_BK_LIGHT  GPIO_NUM_4
#define LEDC_OUTPUT_INVERT     0

#define LCD_PIXEL_CLOCK_HZ     (27 * 1000 * 1000)
#define LCD_HOST               SPI2_HOST

#define DISPLAY_WIDTH         (135)
#define DISPLAY_HEIGHT        (240)
#define DISPLAY_FULLRESH       false

#define LVGL_TICK_PERIOD_MS 2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE (4 * 1024)
#define LVGL_TASK_PRIORITY 2


#define PULSE_WIDTH_MS    350  // Pulse duration in milliseconds
#define PULSE_INTERVAL_MS 150  // Time between pulses
#define PULSE_GPIO_ENABLE GPIO_NUM_25 // Pin for Enable of LM293D
#define PULSE_GPIO_INPUT1 GPIO_NUM_26 // Pin for Input1 of LM293D
#define PULSE_GPIO_INPUT2 GPIO_NUM_27 // Pin for Input2 of LM293D




















