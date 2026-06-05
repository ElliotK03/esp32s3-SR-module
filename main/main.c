// #include "esp32s3_custom_board.h"

#include "audio-sr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hal/gpio_types.h"

void i2c_bus_recovery(gpio_num_t scl, gpio_num_t sda) {
  gpio_set_direction(scl, GPIO_MODE_OUTPUT);
  gpio_set_direction(sda, GPIO_MODE_INPUT);

  for (int i = 0; i < 9; i++) {
    gpio_set_level(scl, 0);
    esp_rom_delay_us(5);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(5);
    if (gpio_get_level(sda))
      break; // SDA released, bus free
  }

  // Issue STOP condition
  gpio_set_direction(sda, GPIO_MODE_OUTPUT);
  gpio_set_level(sda, 0);
  esp_rom_delay_us(5);
  gpio_set_level(scl, 1);
  esp_rom_delay_us(5);
  gpio_set_level(sda, 1); // STOP: SDA rising while SCL high
}

void app_main() {

  // init modules
  audio_sr_init();

  // ------------------------------------------------------------
  // Configure GPIO40 as an input with an internal pull‑up resistor.
  // The board uses GPIO numbers directly, so we use the enum value
  // GPIO_NUM_40.  The pin is set to INPUT mode, pull‑up enabled and
  // pull‑down disabled.  No interrupts are required for this simple
  // poll‑once use‑case.
  // ------------------------------------------------------------
  gpio_config_t gpio_cfg = {
      .pin_bit_mask = (1ULL << GPIO_NUM_40),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&gpio_cfg));

  // ------------------------------------------------------------
  // Read the current level of GPIO40.  With the internal pull‑up the
  // line will read high (1) when nothing is driving it low.  Many
  // boards connect a button that pulls the line to ground when
  // pressed, so a low level means "button pressed".  We enable the
  // voice module when the pin is low and disable it when it is high.
  // Adjust the logic if your hardware uses active‑high signalling.
  // ------------------------------------------------------------
  int level = gpio_get_level(GPIO_NUM_40);
  bool enable_voice = (level == 1);  // active‑low button
  // voice_module_set_enabled(enable_voice);

  ESP_LOGI("app_main", "GPIO40 level=%d → voice module %s",
           level, enable_voice ? "ENABLED" : "DISABLED");

  // Forever loop: poll GPIO40 and toggle voice module when state changes
  bool last_state = enable_voice;
  while (1) {
      vTaskDelay(pdMS_TO_TICKS(200)); // 200 ms poll interval
      int cur_level = gpio_get_level(GPIO_NUM_40);
      bool cur_enabled = (cur_level == 1);
      if (cur_enabled != last_state) {
          voice_module_set_enabled(cur_enabled);
          ESP_LOGI("app_main", "GPIO40 changed: %d → voice module %s",
                   cur_level, cur_enabled ? "ENABLED" : "DISABLED");
          last_state = cur_enabled;
      }
  }
}
