/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "audio-sr.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp32s3_custom_board_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdlib.h>

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_board_init.h"
#include "esp_codec_dev.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_process_sdkconfig.h"
#include "esp_wn_iface.h"
#include "model_path.h"
#include "portmacro.h"
#include "speech_commands_action.h"
#include <assert.h>

// Flags for speech_command_actions to read and manipulate LED strip
led_strip_handle_t strip = NULL;
int wakeup_flag = 0;
int detect_flag = 0;

// FreeRTOS task handle
static TaskHandle_t detect_task_handle;
static TaskHandle_t feed_task_handle;

// Globals consumed by ESP-SR
static const esp_afe_sr_iface_t *afe_handle = NULL;
volatile int task_flag = 0;
srmodel_list_t *models = NULL;

static esp_mn_iface_t *multinet;
static model_iface_data_t *model_data;

// include and wrapper for chime
#include "chime.h"
static void chime_wake_task(void *arg) {
  chime_play_wake();
  vTaskDelete(NULL);
}

static void chime_ack_task(void *arg) {
  chime_play_ack();
  vTaskDelete(NULL);
}

// Speech recognition tasks
void feed_Task(void *arg) {
  esp_afe_sr_data_t *afe_data = arg;
  int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
  ESP_LOGI("FEED_TASK", "audio_chunksize = %d", audio_chunksize);
  int nch = afe_handle->get_feed_channel_num(afe_data);
  int feed_channel = esp_get_feed_channel();
  assert(nch == feed_channel);
  ESP_LOGI("feed_Task", "nch = %d", nch);
  int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
  assert(i2s_buff);

  while (1) {
    if (!task_flag) {
      ESP_LOGI("feed_Task", "......Suspending feed_Task......");
      vTaskSuspend(NULL); // sleeps here, zero CPU, until resumed
      continue;           // re-check task_flag on wake
    }

    if (detect_flag) {
      vTaskDelay(pdMS_TO_TICKS(900));
    }

    esp_get_feed_data(true, i2s_buff,
                      audio_chunksize * sizeof(int16_t) * feed_channel);

    afe_handle->feed(afe_data, i2s_buff);
  }
  if (i2s_buff) {
    free(i2s_buff);
    i2s_buff = NULL;
  }
  vTaskDelete(NULL);
}

void detect_Task(void *arg) {
  esp_afe_sr_data_t *afe_data = arg;
  int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);

  char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  ESP_LOGI("detect_Task", "multinet:%s", mn_name);

  multinet = esp_mn_handle_from_name(mn_name);

  model_data = multinet->create(mn_name, 6000);
  int mu_chunksize = multinet->get_samp_chunksize(model_data);
  esp_mn_commands_update_from_sdkconfig(
      multinet, model_data); // Add speech commands from sdkconfig

  assert(mu_chunksize == afe_chunksize);

  // print active speech commands
  multinet->print_active_speech_commands(model_data);

  ESP_LOGI("detect_Task", "------------detect start------------");
  while (1) {
    if (!task_flag) {
      ESP_LOGI("detect_Task", "......Suspending detect_Task......");
      vTaskSuspend(NULL); // sleeps here, zero CPU, until resumed
      continue;           // re-check task_flag on wake
    } else {
      afe_fetch_result_t *res = afe_handle->fetch(afe_data);
      if (!res || res->ret_value == ESP_FAIL) {
        ESP_LOGE("detect", "fetch error!");
        break;
      }

      if (res->wakeup_state == WAKENET_DETECTED) {
        ESP_LOGI("detect_Task", "WAKEWORD DETECTED");
        multinet->clean(model_data);
        xTaskCreate(chime_wake_task, "chimeWake", 4096, NULL, 3, NULL);
      }

      if (res->raw_data_channels == 1 &&
          res->wakeup_state == WAKENET_DETECTED) {
        wakeup_flag = 1;
      } else if (res->raw_data_channels > 1 &&
                 res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
        // For a multi-channel AFE, it is necessary to wait for the channel to
        // be verified.
        ESP_LOGI("detect_Task", "AFE_FETCH_CHANNEL_VERIFIED, channel index: %d",
                 res->trigger_channel_id);
        wakeup_flag = 1;
      }

      if (wakeup_flag == 1) {
        esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

        if (mn_state == ESP_MN_STATE_DETECTING) {
          continue;
        }

        if (mn_state == ESP_MN_STATE_DETECTED) {
          esp_mn_results_t *mn_result = multinet->get_results(model_data);
          for (int i = 0; i < mn_result->num; i++) {
            ESP_LOGI(
                "detect_Task",
                "TOP %d, command_id: %d, phrase_id: %d, string: %s, prob: %f",
                i + 1, mn_result->command_id[i], mn_result->phrase_id[i],
                mn_result->string, mn_result->prob[i]);
          }
          xTaskCreate(&chime_ack_task, "chimeAck", 1024, NULL, 3, NULL);
          speech_commands_action(mn_result->command_id[0]);
          detect_flag = 1;

          // Re-arm WakeNet and stop MultiNet listening
          afe_handle->enable_wakenet(afe_data);
          wakeup_flag = 0;
          ESP_LOGI(
              "detect_Task",
              "\n-----------Wait 1 second before detecting again-----------");
          vTaskDelay(pdMS_TO_TICKS(900));
          detect_flag = 0;
          continue;
        }

        if (mn_state == ESP_MN_STATE_TIMEOUT) {
          esp_mn_results_t *mn_result = multinet->get_results(model_data);
          ESP_LOGI("detect_Task", "timeout, string:%s", mn_result->string);
          afe_handle->enable_wakenet(afe_data);
          wakeup_flag = 0;
          detect_flag = 0;
          ESP_LOGI("detect_Task",
                   "\n-----------awaits to be waken up-----------");
          continue;
        }
      }
    }
  }
  if (model_data) {
    multinet->destroy(model_data);
    model_data = NULL;
  }
  ESP_LOGI("detect_Task", "detect exit");
  vTaskDelete(NULL);
}

// init speech recognition module
void audio_sr_init() {
  models =
      esp_srmodel_init("model"); // partition label defined in partitions.csv
  ESP_ERROR_CHECK(esp_board_init(16000, 2, 16));
  // ESP_ERROR_CHECK(esp_sdcard_init("/sdcard", 10));

#if CONFIG_IDF_TARGET_ESP32
  printf("This demo only support ESP32S3\n");
  return;
#else
  afe_config_t *afe_config = afe_config_init(esp_get_input_format(), models,
                                             AFE_TYPE_SR, AFE_MODE_LOW_COST);
  afe_handle = esp_afe_handle_from_config(afe_config);
  esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
  afe_config_free(afe_config);
#endif

  task_flag = 1; // for speech detection task toggling
  // extern led_strip_handle_t strip;
  strip = configure_led();
  static TaskHandle_t ledTaskHandle = NULL;
  xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void *)afe_data, 5,
                          &detect_task_handle, 1);
  xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5,
                          &feed_task_handle, 0);
  xTaskCreatePinnedToCore(&led_Task, "led", 1500, NULL, 5, &ledTaskHandle, 1);

  int en = 0;

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(8000));
    UBaseType_t highWater = uxTaskGetStackHighWaterMark(detect_task_handle);
    ESP_LOGI("audio_sr_init()", "detect task high‑water: %u bytes left",
             (uint16_t)(highWater * sizeof(StackType_t)));

    // en = ((task_flag + 1) % 2);
    // voice_module_set_enabled(en);
  }
}

// resume or suspend voice module
void voice_module_set_enabled(bool enabled) {
  task_flag = enabled ? 1 : 0;
  if (enabled) {
    ESP_ERROR_CHECK(bsp_codec_resume());
    ESP_LOGI("SR_dispatcher", "resuming voice module...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI("SR_dispatcher", "Resuming tasks...");
    vTaskResume(feed_task_handle);
    vTaskResume(detect_task_handle);
  } else {
    // tasks will self-suspend on next loop iteration
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI("SR_dispatcher", "Suspending codec");
    ESP_ERROR_CHECK(bsp_codec_suspend());
  }
}

bool voice_module_get_status() { return task_flag; }

esp_err_t multinet_set_detection_threshold(float threshold) {

  if (threshold >= 0 && threshold <= 1) {
    multinet->set_det_threshold(model_data, threshold);
    return ESP_OK;
  } else {
    multinet_set_detection_threshold(0.6f);
    return ESP_ERR_INVALID_ARG;
  }
}

esp_err_t set_input_sensitivity(uint8_t level) {
  if (level > 30)
    level = 30;
  return esp_codec_dev_set_in_gain(codec_dev, (float)level);
}

esp_err_t set_output_vol(uint8_t vol) {
  if (vol > 100)
    vol = 100;

  return esp_codec_dev_set_out_vol(codec_dev, vol);
}

// esp_err_t set_command_recognition_timeout(uint16_t timeout) {
// // Use NVS
// }
