#include "bsp_board.h"
#include "esp32s3_custom_board_api.h"
// #include "esp_codec_dev.h"
#include "reent.h"
#include "string.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/i2s_std.h"
// #include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"
#else
#include "driver/i2s.h"
#endif
// #include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
// #include "esp_codec_dev_os.h"// #include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
// #include "esp_rom_sys.h"
// #include "esp_vfs_fat.h"
// #include "sdmmc_cmd.h"
#if ((SOC_SDMMC_HOST_SUPPORTED) && (FUNC_SDMMC_EN))
// #include "driver/sdmmc_host.h"
#endif /* ((SOC_SDMMC_HOST_SUPPORTED) && (FUNC_SDMMC_EN)) */
// #include "sd_pwr_ctrl_by_on_chip_ldo.h"
// #include "sd_pwr_ctrl_interface.h"
// #include "esp_ldo_regulator.h"

#include "i2c_handlers.h"

#define GPIO_MUTE_NUM GPIO_NUM_1
#define GPIO_MUTE_LEVEL 1
#define ACK_CHECK_EN 0x1 /*!< I2C master will check ack from slave*/
#define ADC_I2S_CHANNEL 2
// static sdmmc_card_t *card;
static const char *TAG = "board";
static int s_play_sample_rate = 16000;
static int s_play_channel_format = 2;
static int s_bits_per_chan = 16;

// static sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
// static esp_ldo_channel_handle_t         ldo_audio_board = NULL;
// static bool esp_ldo_enabled = false;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
static i2s_chan_handle_t tx_handle = NULL; // I2S tx channel handler
static i2s_chan_handle_t rx_handle = NULL; // I2S rx channel handler
#endif

static audio_codec_data_if_t *codec_data_if = NULL;
static audio_codec_ctrl_if_t *codec_ctrl_if = NULL;
static audio_codec_gpio_if_t *codec_gpio_if = NULL;
static audio_codec_if_t *codec_if = NULL;
esp_codec_dev_handle_t codec_dev = NULL;
// static i2c_master_bus_handle_t i2c_bus_handle = NULL;

static esp_codec_dev_sample_info_t fs = {
      .bits_per_sample = 0u,
      .sample_rate = 0u,
      .channel = 0u,
  };

esp_err_t bsp_i2c_init(i2c_port_t i2c_num, uint32_t clk_speed) {

  i2c_master_bus_config_t bus_cfg = {
      .i2c_port = i2c_num,
      .sda_io_num = GPIO_I2C_SDA,
      .scl_io_num = GPIO_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };

  return i2c_new_master_bus(&bus_cfg, &i2c_bus_handle);
}

esp_err_t bsp_audio_set_play_vol(int volume) {
  if (!codec_dev) {
    ESP_LOGE(TAG, "DAC codec init fail");
    return ESP_FAIL;
  }
  esp_codec_dev_set_out_vol(codec_dev, volume);
  return ESP_OK;
}

esp_err_t bsp_audio_get_play_vol(int *volume) {
  if (!codec_dev) {
    ESP_LOGE(TAG, "DAC codec init fail");
    return ESP_FAIL;
  }
  esp_codec_dev_get_out_vol(codec_dev, volume);
  return ESP_OK;
}

// static esp_err_t bsp_i2s_init(i2s_port_t i2s_num, uint32_t sample_rate,
// i2s_channel_fmt_t channel_format, i2s_bits_per_chan_t bits_per_chan)
static esp_err_t bsp_i2s_init(i2s_port_t i2s_num, uint32_t sample_rate,
                              int channel_format, int bits_per_chan) {
  esp_err_t ret_val = ESP_OK;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  i2s_slot_mode_t channel_fmt = I2S_SLOT_MODE_STEREO;
  if (channel_format != 2) {
    ESP_LOGW(TAG, "Unable to configure channel_format %d, reset to 2",
             channel_format);
    channel_format = 2;
    channel_fmt = I2S_SLOT_MODE_STEREO;
  }

  if (bits_per_chan != 16) {
    ESP_LOGW(TAG, "Unable to configure bits_per_chan %d, reset to 16 ",
             bits_per_chan);
    bits_per_chan = 16;
  }

  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  ret_val |= i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
  i2s_std_config_t std_cfg =
      I2S_CONFIG_DEFAULT(sample_rate, channel_fmt, bits_per_chan);
  ret_val |= i2s_channel_init_std_mode(tx_handle, &std_cfg);
  ret_val |= i2s_channel_init_std_mode(rx_handle, &std_cfg);
  ret_val |= i2s_channel_enable(tx_handle);
  ret_val |= i2s_channel_enable(rx_handle);
#else
  ESP_LOGE(TAG, "P4 don't support IDF version < V5.3");
#endif

  return ret_val;
}

static esp_err_t bsp_i2s_deinit(i2s_port_t i2s_num) {
  esp_err_t ret_val = ESP_OK;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  if (rx_handle) {
    ret_val |= i2s_channel_disable(rx_handle);
    ret_val |= i2s_del_channel(rx_handle);
    rx_handle = NULL;
  }
  if (tx_handle) {
    ret_val |= i2s_channel_disable(tx_handle);
    ret_val |= i2s_del_channel(tx_handle);
    tx_handle = NULL;
  }
#else
  ESP_LOGE(TAG, "P4 don't support IDF version < V5.3");
#endif

  return ret_val;
}

static esp_err_t bsp_codec_init(int adc_sample_rate, int dac_sample_rate,
                                int dac_channel_format, int dac_bits_per_chan) {
  esp_err_t ret_val = ESP_OK;

  // ret_val |= bsp_codec_adc_init(adc_sample_rate);
  // ret_val |= bsp_codec_dac_init(dac_sample_rate, dac_channel_format,
  // dac_bits_per_chan);

  // Do initialize of related interface: data_if, ctrl_if and gpio_if
  audio_codec_i2s_cfg_t i2s_cfg = {
      .port = I2S_NUM_1,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
      .rx_handle = rx_handle,
      .tx_handle = tx_handle,
#endif
  };
  codec_data_if = audio_codec_new_i2s_data(&i2s_cfg);

  audio_codec_i2c_cfg_t i2c_cfg = {
      .addr = 0x30, // Verified on pulseview: It tries to use (.addr >> 1) as I2C address
      .bus_handle = i2c_bus_handle,
  };
  codec_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
  codec_gpio_if = audio_codec_new_gpio();
  // New output codec interface
  es8311_codec_cfg_t es8311_cfg = {
      .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
      .ctrl_if = codec_ctrl_if,
      .gpio_if = codec_gpio_if,
      .pa_pin = GPIO_PWR_CTRL,
      .use_mclk = false,
  };
  codec_if = es8311_codec_new(&es8311_cfg);
  // New output codec device
  esp_codec_dev_cfg_t dev_cfg = {
      .codec_if = codec_if,
      .data_if = codec_data_if,
      .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
  };
  codec_dev = esp_codec_dev_new(&dev_cfg);

  esp_codec_dev_sample_info_t dummy = {
      .bits_per_sample = dac_bits_per_chan,
      .sample_rate = dac_sample_rate,
      .channel = dac_channel_format,
  };
  fs = dummy;
  
  esp_codec_dev_set_in_gain(codec_dev, RECORD_VOLUME);
  esp_codec_dev_set_out_vol(codec_dev, PLAYER_VOLUME);
  esp_codec_dev_open(codec_dev, &fs);

  return ret_val;
}

static esp_err_t bsp_codec_deinit() {
  esp_err_t ret_val = ESP_OK;

  if (codec_dev) {
    esp_codec_dev_close(codec_dev);
    esp_codec_dev_delete(codec_dev);
    codec_dev = NULL;
  }

  // Delete codec interface
  if (codec_if) {
    audio_codec_delete_codec_if(codec_if);
    codec_if = NULL;
  }

  // Delete codec control interface
  if (codec_ctrl_if) {
    audio_codec_delete_ctrl_if(codec_ctrl_if);
    codec_ctrl_if = NULL;
  }

  if (codec_gpio_if) {
    audio_codec_delete_gpio_if(codec_gpio_if);
    codec_gpio_if = NULL;
  }

  // Delete codec data interface
  if (codec_data_if) {
    audio_codec_delete_data_if(codec_data_if);
    codec_data_if = NULL;
  }

  return ret_val;
}

esp_err_t bsp_codec_resume() {
    return esp_codec_dev_open(codec_dev, &fs);
}

esp_err_t bsp_codec_suspend()
{
  return esp_codec_dev_close(codec_dev);
}

esp_err_t bsp_audio_play(const int16_t *data, int length,
                         TickType_t ticks_to_wait) {
  esp_err_t ret = ESP_OK;
  if (!codec_dev) {
    return ESP_FAIL;
  }
  ret = esp_codec_dev_write(codec_dev, (void *)data, length);

  return ret;
}

esp_err_t bsp_get_feed_data(bool is_get_raw_channel, int16_t *buffer,
                            int buffer_len) {
  esp_err_t ret = ESP_OK;
  if (!codec_dev) {
    return ESP_FAIL;
  }

  ret = esp_codec_dev_read(codec_dev, (void *)buffer, buffer_len);

  return ret;
}

int bsp_get_feed_channel(void) { return ADC_I2S_CHANNEL; }

char *bsp_get_input_format(void) { return "MR"; }

esp_err_t bsp_board_init(uint32_t sample_rate, int channel_format,
                         int bits_per_chan) {
  // Turn on the power for audio board
  // bsp_enable_audio_board_power();

  /*!< Initialize I2C bus, used for audio codec*/
  bsp_i2c_init(I2C_NUM, I2C_CLK);

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = 0x18,
      .scl_speed_hz = 400000,
  };  
  if (sample_rate != 16000) {
    ESP_LOGE(TAG, "Unable to configure sample_rate. It's only support 16000.");
    sample_rate = 16000;
  }
  s_play_sample_rate = sample_rate;

  if (channel_format != 2) {
    ESP_LOGE(TAG, "Unable to configure channel_format. It's only support 2.");
    channel_format = 2;
  }
  s_play_channel_format = channel_format;

  if (bits_per_chan != 16) {
    ESP_LOGE(TAG, "Unable to configure bits_per_chan. It's only support 16.");
    bits_per_chan = 16;
  }
  s_bits_per_chan = bits_per_chan;

  vTaskDelay(pdMS_TO_TICKS(1000)); // let ES8311 finish power-up before I2C

  i2c_master_dev_handle_t dev_handle;
  ESP_ERROR_CHECK(
      i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

  // 3. Write register address, then read 1 byte back
  uint8_t reg_addr = 0x0D;
  uint8_t reg_val = 01;
  esp_err_t ret = i2c_master_transmit_receive(dev_handle, &reg_addr,
                                              1, // write: register address
                                              &reg_val, 1, // read:  1 byte back
                                              pdMS_TO_TICKS(100));

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "[DEBUG] ES8311 reg 0x%02X = 0x%02X", 0x0D, reg_val);
  } else {
    ESP_LOGE(TAG, "[DEBUG] Read failed: %s", esp_err_to_name(ret));
  }

  i2c_master_bus_rm_device(dev_handle);

  ESP_LOGI(TAG, "before i2s_init");
  bsp_i2s_init(I2S_NUM_1, 16000, 2, 16);
  // Because record and play use the same i2s.
  ESP_LOGI(TAG, "before codec_init");
  bsp_codec_init(16000, 16000, 2, 16);
  if (reg_val != 0x01u) {
    bsp_codec_deinit();
    vTaskDelay(pdMS_TO_TICKS(1000));
    bsp_codec_init(16000, 16000, 2, 16);
  }

  ESP_LOGI(TAG, "after codec_init");
  return ESP_OK;
}
