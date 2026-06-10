#pragma once

#include "esp_err.h"
#include "esp_wn_iface.h"
#include "led_strip_types.h"
#include <stdint.h>

/* initializes the audio codec, speech recognition pipeline
 * and the addressable RGB on the ESP32S3 dev board
 *
 * It will also load all the saved variable from NVS
 */
void audio_sr_init();

bool voice_module_get_status();

void voice_module_set_enabled(bool enabled);

/* Sets the volume to the desired level
 *
 */
esp_err_t set_output_vol(uint8_t vol);

/* Sets the level of mic sensitivity (in clicks/levels)
 *
 * Internally this adjusts the mic's gain
 *
 * There's some mapping, level 6 = 30dB gain
 *
 */
esp_err_t set_input_sensitivity(uint8_t level);

/*
 * Modifies the timeout parameter for Multinet.
 *
 * Multinet detection stops after a certain amount of time if wake word
 * is detected but not a subsequent command word.
 * 
 * The parameter is stored in NVS, updating the timeout requires a reboot to take effect. 
 */
esp_err_t set_command_recognition_timeout(uint16_t timeout);

/*
 * Sets command recognition threshold (value stored in NVS)
 *
 * Expects a float between 0 and 1
 */
esp_err_t multinet_set_detection_threshold(float threshold);
