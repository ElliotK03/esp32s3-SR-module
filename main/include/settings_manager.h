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
esp_err_t settings_manager_set_brightness(int32_t brightness);
esp_err_t settings_manager_set_lock(bool locked);
esp_err_t settings_manager_set_locker_box_connected(bool connected);
esp_err_t settings_manager_set_lock_manual_override(bool enabled);

/*
 * Return cached settings, do not directly modify the contents
 */
user_settings_t * get_cached_settings();

/* Cloud sync count — incremented on every local change, persisted in NVS */
uint32_t settings_manager_get_sync_count(void);

/* Called by connections.c to apply cloud-sourced settings without
 * triggering a re-push back to the cloud. Updates cache, NVS, and
 * sets sync_count to cloud_count. */
esp_err_t settings_manager_apply_from_cloud(const user_settings_t *s, uint32_t cloud_count);

/* Dirty flag — set true whenever a local change should be pushed to cloud */
bool settings_manager_needs_cloud_push(void);
void settings_manager_clear_cloud_push(void);
void settings_manager_signal_cloud_push(void);
