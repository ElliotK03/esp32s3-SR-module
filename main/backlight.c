#include "backlight.h"

#include <stdbool.h>

#include "driver/ledc.h"
#include "esp_log.h"

#include "pinout.h"

static const char *TAG_BACKLIGHT = "backlight";

static volatile bool g_backlight_init = false;

void set_backlight_brightness(int32_t percent) {
    if (!g_backlight_init) {
        return;
    }

    if (percent < 5) percent = 5;
    if (percent > 100) percent = 100;

    uint32_t duty = (percent * 1023) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ESP_LOGI(TAG_BACKLIGHT, "Brightness updated to %d%% (duty: %lu)", (int)percent, (unsigned long)duty);
}

void backlight_init(int32_t initial_brightness) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .freq_hz          = 1000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    uint32_t duty = (initial_brightness * 1023) / 100;
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LCD_BL,
        .duty           = duty,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    g_backlight_init = true;
}
