// #include "esp32s3_custom_board.h"
/* main.c
 *
 * Demo that exercises the NVS helper API:
 *   1. Initialise the NVS flash subsystem.
 *   2. Read the stored user settings (or the defaults if none exist).
 *   3. Print the values to the console.
 *   4. Change a couple of fields, write the new settings back to NVS,
 *      and read them again to verify persistence.
 *
 * Build with the normal ESP‑IDF workflow (idf.py build && idf.py -p <port>
 * flash).
 */

#include "esp_log.h"
#include "nvs_helper.h"
#include "user-settings.h"
#include <inttypes.h>

static const char *TAG = "demo";

/* Helper to pretty‑print a settings structure */
static void print_settings(const user_settings_t *s) {
  ESP_LOGI(TAG, "Pomodoro settings:");
  ESP_LOGI(TAG, "  work_duration_s      = %" PRIu32,
           s->pomodoro.work_duration_s);
  ESP_LOGI(TAG, "  short_break_s        = %" PRIu32, s->pomodoro.short_break_s);
  ESP_LOGI(TAG, "  long_break_s         = %" PRIu32, s->pomodoro.long_break_s);
  ESP_LOGI(TAG, "  sessions_before_long = %u",
           s->pomodoro.sessions_before_long);

  ESP_LOGI(TAG, "Voice settings:");
  ESP_LOGI(TAG, "  enabled              = %s",
           s->voice.enabled ? "true" : "false");
  ESP_LOGI(TAG, "  wakenet_threshold    = %.2f", s->voice.wakenet_threshold);
  ESP_LOGI(TAG, "  volume               = %u", s->voice.volume);
}

/* Entry point for the ESP‑IDF application */
void app_main(void) {
  /*  Initialise NVS */
  ESP_LOGI(TAG, "Initialising NVS helper");
  nvs_helper_init();

  /* Read (or create) the stored settings */
  user_settings_t settings;
  esp_err_t err = get_settings_from_nvs(&settings);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read settings: %s", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "=== Settings after first read ===");
  print_settings(&settings);

  /*  Modify a few fields to prove write‑back works */
  ESP_LOGI(TAG, "Modifying settings for test");
  settings.pomodoro.work_duration_s = 25 * 60; // 10 min work period

  if (settings.voice.volume > 100) {

    settings.voice.volume = 100;
  }
  int diff =
      (settings.voice.volume >= 0 && settings.voice.volume < 35) ? 1 : -1;
  settings.voice.volume += diff; // lower volume

  err = write_settings_to_nvs(&settings);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write settings: %s", esp_err_to_name(err));
    return;
  }

  /*  Read back the modified values */
  user_settings_t verify;
  err = get_settings_from_nvs(&verify);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to re‑read settings: %s", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "=== Settings after write‑back ===");
  print_settings(&verify);
}
