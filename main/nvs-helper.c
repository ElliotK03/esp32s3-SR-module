// Helper functions for settings persistence
#include "nvs_helper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "user-settings.h"

#define TAG "nvs-helper"

#define NVS_NAMESPACE "user_settings"
#define NVS_KEY_SETTINGS "settings"
static nvs_handle_t my_handle;

void nvs_helper_init() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
}

esp_err_t write_settings_to_nvs(user_settings_t *p_settings) {
  esp_err_t ret;
  ret = nvs_set_blob(my_handle, NVS_KEY_SETTINGS, p_settings,
                     sizeof(user_settings_t));

  if (ret != ESP_OK)
    return ret;
  return nvs_commit(my_handle);
}

esp_err_t get_settings_from_nvs(user_settings_t *p_settings) {
  size_t len = sizeof(user_settings_t);

  esp_err_t ret = nvs_get_blob(my_handle, NVS_KEY_SETTINGS, p_settings, &len);

  if (ret == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "No settings found, loading defaults");
    *p_settings = (user_settings_t)USER_SETTINGS_DEFAULT;
    return write_settings_to_nvs(p_settings); // persist defaults immediately
  }
  return ret;
}
