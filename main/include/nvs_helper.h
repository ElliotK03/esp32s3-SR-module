#ifndef NVS_HELPER_H
#define NVS_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------
 *  Includes
 * ------------------------------------------------------------- */
#include <stdbool.h>
#include "esp_err.h"          /* esp_err_t */
#include "user-settings.h"    /* user_settings_t and USER_SETTINGS_DEFAULT */

/* -------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------- */

/**
 * @brief Initialise the NVS flash subsystem and open the
 *        "user_settings" namespace.
 *
 * This function must be called once (typically from app_main) before
 * any of the other helpers are used.
 *
 * @return ESP_OK on success, otherwise an esp_err_t error code.
 */
void nvs_helper_init(void);

/**
 * @brief Write a complete user‑settings structure to NVS.
 *
 * The data is stored as a binary blob under the key "settings".
 *
 * @param[in] p_settings  Pointer to a fully‑initialised user_settings_t.
 *
 * @return ESP_OK on success, otherwise an esp_err_t error code.
 */
esp_err_t write_settings_to_nvs(user_settings_t *p_settings);

/**
 * @brief Read the user‑settings structure from NVS.
 *
 * If the key does not exist yet, the function loads the default
 * settings (USER_SETTINGS_DEFAULT), writes those defaults back to NVS
 * and returns ESP_OK.
 *
 * @param[out] p_settings  Pointer to a user_settings_t that will receive
 *                         the stored (or default) values.
 *
 * @return ESP_OK on success, otherwise an esp_err_t error code.
 */
esp_err_t get_settings_from_nvs(user_settings_t *p_settings);

#ifdef __cplusplus
}
#endif

#endif /* NVS_HELPER_H */