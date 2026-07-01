#include "settings_manager.h"
#include "app_logic.h" // for pomodoro period UI helpers
#include "audio-sr.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_helper.h"
#include <inttypes.h>

static const char *TAG = "SETTINGS_MGR";

// One-shot guard: skip applying voice settings on the very first init call
static uint8_t s_init_guard;

static uint32_t s_sync_count = 0;
static bool s_needs_cloud_push = false;

/* Cached copy – the only place the rest of the program reads from. */
static user_settings_t cached_settings;

/* ------------------------------------------------------------------ */
/* Forward declarations of the “apply” helpers that push a setting
 * into the concrete subsystem. */
static void apply_voice_settings(const voice_settings_t *vs);
static void apply_pomodoro_settings(const pomodoro_settings_t *ps);

/* ------------------------------------------------------------------ */
user_settings_t * get_cached_settings(){
  return &cached_settings;
}

void settings_manager_init(void) {
  s_init_guard = 0;
  nvs_helper_init();
  /* Load from NVS (or fall back to defaults). */
  esp_err_t err = get_settings_from_nvs(&cached_settings);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "No settings found, loading defaults");
    cached_settings = (user_settings_t)USER_SETTINGS_DEFAULT;
    /* Persist the defaults so the next boot finds them. */
    write_settings_to_nvs(&cached_settings);
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read settings: %s", esp_err_to_name(err));
    cached_settings = (user_settings_t)USER_SETTINGS_DEFAULT;
  }

  s_sync_count = nvs_read_sync_count();

  ESP_LOGI(TAG, "Settings loaded – applying (sync_count=%" PRIu32 ")", s_sync_count);
  apply_voice_settings(&cached_settings.voice);
  apply_pomodoro_settings(&cached_settings.pomodoro);

  ESP_LOGI(TAG, "Volume: %" PRIu8, cached_settings.voice.volume);
  ESP_LOGI(TAG, "Brightness: %" PRId32, cached_settings.brightness);
  ESP_LOGI(TAG, "Locked: %s", cached_settings.locked ? "yes" : "no");

  // Experimental, updating the values displayed by LVGL so value is correct on startup
  set_var_volume(cached_settings.voice.volume);
  set_var_screen_brightness_val(cached_settings.brightness);
}

/* ------------------------------------------------------------------ */
const user_settings_t *settings_manager_get(void) { return &cached_settings; }

/* ------------------------------------------------------------------ */
esp_err_t settings_manager_set(const user_settings_t *new_settings) {
  if (!new_settings) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Write to NVS first – if that fails we keep the old runtime state. */
  esp_err_t err = write_settings_to_nvs((user_settings_t *)new_settings);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS write failed: %s", esp_err_to_name(err));
    return err;
  }

  /* Update the in‑RAM copy and push the changes downstream. */
  cached_settings = *new_settings;
  apply_voice_settings(&cached_settings.voice);
  apply_pomodoro_settings(&cached_settings.pomodoro);

  /* Increment sync count and flag for cloud push */
  s_sync_count++;
  nvs_write_sync_count(s_sync_count);
  s_needs_cloud_push = true;

  ESP_LOGI(TAG, "New settings applied (sync_count=%" PRIu32 ")", s_sync_count);
  return ESP_OK;
}

esp_err_t settings_manager_set_dirty(const user_settings_t *new_settings) {
  if (!new_settings) {
    return ESP_ERR_INVALID_ARG;
  }

  cached_settings = *new_settings;

  return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Helper setters – they only touch the relevant field and then call
 * the generic set‑function.  This keeps the public API tidy for UI /
 * network code that only wants to change one value. */
esp_err_t settings_manager_set_voice_enabled(bool en) {
  user_settings_t upd = cached_settings;
  upd.voice.enabled = en;
  return settings_manager_set(&upd);
}

esp_err_t settings_manager_set_wakenet_threshold(float th) {
  if (th < 0.0f || th > 1.0f) {
    return ESP_ERR_INVALID_ARG;
  }
  user_settings_t upd = cached_settings;
  upd.voice.wakenet_threshold = th;
  return settings_manager_set(&upd);
}

esp_err_t settings_manager_set_brightness(int32_t brightness) {
  if (brightness > 100)
    brightness = 100;
  user_settings_t upd = cached_settings;
  upd.brightness = brightness;
  return settings_manager_set(&upd);
}

esp_err_t settings_manager_set_volume(uint8_t vol) {
  if (vol > 100)
    vol = 100;
  user_settings_t upd = cached_settings;
  upd.voice.volume = vol;
  return settings_manager_set(&upd);
}

esp_err_t settings_manager_set_pomodoro_work(uint32_t secs) {
  user_settings_t upd = cached_settings;
  upd.pomodoro.work_duration_s = secs;
  return settings_manager_set(&upd);
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* Apply helpers – called whenever the cached copy changes.            */

static void apply_voice_settings(const voice_settings_t *vs) {
  /* Enable / disable the whole voice pipeline */
  if (s_init_guard == 0) {
    s_init_guard++;
  } else {
    voice_module_set_enabled(vs->enabled);

    /* Volume – the audio‑sr API already knows how to talk to the codec */
    set_output_vol(vs->volume);

    /* Wake‑net detection threshold – the SR code already exposes a
     * wrapper that stores the value in NVS.  We call it here so the
     * new threshold takes effect immediately (no reboot needed). */
    multinet_set_detection_threshold(vs->wakenet_threshold);
  }
}

static void apply_pomodoro_settings(const pomodoro_settings_t *ps) {
  /* The pomodoro UI uses a *period* (work duration) that is also
   * displayed on the screen.  The UI helpers live in app_logic.c. */
  app_logic_set_work_duration(ps->work_duration_s);

  /* Short / long break and session count are currently only used by
   * the higher‑level pomodoro state machine (not shown here).  If you
   * later need them, just expose similar wrappers. */
}

/* ------------------------------------------------------------------ */

esp_err_t settings_manager_set_lock(bool locked) {
  user_settings_t upd = cached_settings;
  upd.locked = locked;
  return settings_manager_set(&upd);
}

uint32_t settings_manager_get_sync_count(void) {
  return s_sync_count;
}

bool settings_manager_needs_cloud_push(void) {
  return s_needs_cloud_push;
}

void settings_manager_clear_cloud_push(void) {
  s_needs_cloud_push = false;
}

void settings_manager_signal_cloud_push(void) {
  s_needs_cloud_push = true;
}

esp_err_t settings_manager_apply_from_cloud(const user_settings_t *s, uint32_t cloud_count) {
  if (!s) return ESP_ERR_INVALID_ARG;

  esp_err_t err = write_settings_to_nvs((user_settings_t *)s);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS write failed during cloud apply: %s", esp_err_to_name(err));
    return err;
  }

  cached_settings = *s;
  s_sync_count = cloud_count;
  nvs_write_sync_count(cloud_count);
  s_needs_cloud_push = false;

  apply_voice_settings(&cached_settings.voice);
  apply_pomodoro_settings(&cached_settings.pomodoro);
  set_var_volume(cached_settings.voice.volume);
  set_var_screen_brightness_val(cached_settings.brightness);

  ESP_LOGI(TAG, "Cloud settings applied (sync_count=%" PRIu32 ")", cloud_count);
  return ESP_OK;
}

/* ------------------------------------------------------------------ */
