/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include "esp_log.h"

#include "esp_board_init.h"
#include "speech_commands_action.h"
#include "app_logic.h"
// #include "led_strip_types.h"

#include "reent.h"

#include "audio-sr.h"
extern led_strip_handle_t strip;
extern int wakeup_flag;
extern int detect_flag;
extern int task_flag;

#include "speech_commands_action.h"

#include "led_strip.h"

#define LED_STRIP_LED_NUMBERS 1
#define LED_STRIP_GPIO_PIN 48

#define LED_STRIP_MEMORY_BLOCK_WORDS 0    // let the driver choose a proper memory block size automatically
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)
#define LED_STRIP_USE_DMA 0

#define EXAMPLE_CHASE_SPEED_MS (10)
// static led_strip_handle_t strip = NULL;
extern led_strip_handle_t strip;

led_strip_handle_t configure_led(void) {
  const led_strip_config_t strip_config = {
      .strip_gpio_num = LED_STRIP_GPIO_PIN, // The GPIO that connected to the
                                            // LED strip's data line
      .max_leds = LED_STRIP_LED_NUMBERS,    // The number of LEDs in the strip,
      .led_model = LED_MODEL_WS2812,        // LED strip model
      .color_component_format =
          LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip:
                                             // GRB
      .flags = {
          .invert_out = false, // don't invert the output signal
      }};
  
  const led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
      .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
      .mem_block_symbols =
          LED_STRIP_MEMORY_BLOCK_WORDS, // the memory block size used by the RMT channel
      .flags = {
          .with_dma = LED_STRIP_USE_DMA, // Using DMA can improve performance when driving more LEDs
      }}; // default
  
  led_strip_handle_t led_strip;
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  ESP_LOGI("LED_TASK", "Created LED strip object with RMT backend");

  return led_strip;
}
  
void led_Task(void *arg) {
  // Clear LED strip (turn off all LEDs)
  ESP_ERROR_CHECK(led_strip_clear(strip));
  for (int j = 0; j < LED_STRIP_LED_NUMBERS; j += 1) {
    ESP_ERROR_CHECK(led_strip_set_pixel(strip, j, 50, 50, 50));
  }
  
  // Flush RGB values to LEDs
  ESP_ERROR_CHECK(led_strip_refresh(strip));

  while (1) {
    if (!task_flag) {
      ESP_ERROR_CHECK(led_strip_clear(strip));
      continue;
    }
    for (int i = 0; i < 100; i++) {
      for (int j = 0; j < LED_STRIP_LED_NUMBERS; j += 1) {
        // Build RGB values
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, j, 100 * (detect_flag ^ wakeup_flag),
                                            0.5 * i * wakeup_flag,
                                            0.5 * i * (1 - detect_flag)));
        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(led_strip_refresh(strip));
      }
      vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
    }

    for (int i = 100; i > 0; i--) {
      for (int j = 0; j < LED_STRIP_LED_NUMBERS; j += 1) {
        // Build RGB values
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, j, 100 * (detect_flag ^ wakeup_flag),
                                            0.5 * i * wakeup_flag,
                                            0.5 * i * (1 - detect_flag)));
        ESP_ERROR_CHECK(led_strip_refresh(strip));
      }
      vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
    }
    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
  }
}
// #endif

void speech_commands_action(int command_id) {
  ESP_LOGI("Speech_commands_action", "Recognized command, ID: %d", command_id+1);
  
  // Tie voice commands to the GUI Pomodoro timer:
  // ID 14: "TURN ON THE LIGHT" -> START
  // ID 15: "TURN OFF THE LIGHT" / "TURN OF THE LIGHT" -> STOP

  // Experimental: Voice control the timer

  // if (command_id + 1 == 2) {
  //     ESP_LOGI("Speech_commands_action", "Voice command: START TIMER");
  //     start_timer(get_pomo_period());
  // } else if (command_id + 1 == 3) {
  //     ESP_LOGI("Speech_commands_action", "Voice command: STOP TIMER");
  //     stop_timer();
  // }
}
