#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_board_init.h"

// Plays a simple frequency sweep or note sequence directly.
// Safe to call from detect_Task — blocks until audio is done (portMAX_DELAY).
void chime_play_wake(void);
void chime_play_ack(void);