#pragma once
#include "user-settings.h"
#include "esp_err.h"

/* Initialise the manager – reads NVS (or defaults) and applies them.
 * Must be called early (e.g. from app_main()).
 */
void settings_manager_init(void);

/* Return a *read‑only* pointer to the cached settings.
 * Callers must not modify the struct directly.
 */
const user_settings_t *settings_manager_get(void);

/* Update the whole settings blob (e.g. after a network config download).
 * The function writes the new blob to NVS, updates the cache and
 * calls the appropriate “apply” callbacks.
 *
 * Returns ESP_OK on success, otherwise an ESP‑error code.
 */
esp_err_t settings_manager_set(const user_settings_t *new_settings);

esp_err_t settings_manager_set_dirty(const user_settings_t *new_settings);

/* Convenience helpers for the two sub‑domains – they just forward
 * to `settings_manager_set()` after changing the relevant field.
 */
esp_err_t settings_manager_set_voice_enabled(bool en);
esp_err_t settings_manager_set_wakenet_threshold(float th);
esp_err_t settings_manager_set_volume(uint8_t vol);
esp_err_t settings_manager_set_pomodoro_work(uint32_t secs);
