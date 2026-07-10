// Custom ES8311 Controls and Board-related functions
#pragma once

#include "esp_codec_dev.h"
#include "driver/i2c_master.h"

esp_err_t bsp_codec_resume();
esp_err_t bsp_codec_suspend();

// Custom volume control wrapper
extern esp_codec_dev_handle_t codec_dev;