// #include "esp32s3_custom_board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#include "nvs_helper.h"
#include "pinout.h"
#include "audio-sr.h"
#include "backlight.h"
#include "settings_manager.h"
#include "st7789.h"
#include "lvgl.h"
#include "ui.h"
#include "screen_swipe.h"
#include "app_logic.h"
#include "connections.h"
#include "i2c_handlers.h"
#include "motor_control.h"

#define ENABLE_AUDIO_SR 1
#define SUSPEND_AUDIO_GPIO 0
#define ENABLE_DEBUG_MEM 0

static void display_task(void *arg);

static const char *TAG_DISPLAY = "ST7789";
static const char *TAG_TOUCH = "FT6236";
static const char *TAG_MAIN = "app_main";
static const char *TAG_MEM = "memory";

#define DISPLAY_TASK_STACK_SIZE  (10 * 1024)
#define DISPLAY_TASK_PRIORITY    10
#define CONNECTIONS_TASK_STACK_SIZE  (7 * 1024)
#define CONNECTIONS_TASK_PRIORITY    2

#define CONFIG_WIDTH  240
#define CONFIG_HEIGHT 320
#define CONFIG_OFFSETX 0
#define CONFIG_OFFSETY 0

// --- I2C Touch Driver (CST816S / FT6236) ---
#define I2C_MASTER_NUM              0               /*!< I2C master i2c port number */
#define I2C_MASTER_FREQ_HZ          400000          /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */

#define FT6236_ADDR                 0x38            /*!< Slave address of FT6236 */

// I2C Bus 0
i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t ft6236_handle;
static SemaphoreHandle_t touch_interrupt_sem = NULL;
static volatile bool touch_data_ready = false;
static bool g_touch_ready = false;

// I2C Bus for touch panel, I2C number 0
static esp_err_t i2c_master_init(void) {
    esp_err_t err; 

#if ENABLE_AUDIO_SR
#else
    // I2C bus already configured by bsp_board.c

    if (i2c_bus_handle != NULL && ft6236_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t touch_bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = TP_SDA,
        .scl_io_num = TP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    err = i2c_new_master_bus(&touch_bus_config, &i2c_bus_handle);
    if (err != ESP_OK) {
        return err;
    }
#endif

    i2c_device_config_t ft6236_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT6236_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(i2c_bus_handle, &ft6236_config, &ft6236_handle);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

static bool tp_read_point(uint16_t *x, uint16_t *y) {
    if (!g_touch_ready || ft6236_handle == NULL) {
        return false;
    }

    uint8_t data[6];
    uint8_t start_reg = 0x02;
    
    // Read registers starting from 0x02 (Status, touch ID, X, Y)
    esp_err_t ret = i2c_master_transmit_receive(ft6236_handle, &start_reg, 1, data, sizeof(data), 10);

    if (ret != ESP_OK || data[0] == 0) { // If no touch points
        touch_data_ready = false;
        return false;
    }

    // Extract X and Y
    *x = ((data[1] & 0x0F) << 8) | data[2];
    *y = ((data[3] & 0x0F) << 8) | data[4];
    ESP_LOGD(TAG_TOUCH, "X: %u, Y: %u", *x, *y);
    touch_data_ready = false;
    
    return true;
}

// ISR handler for touch interrupt pin (TP_INT)
static void IRAM_ATTR tp_interrupt_handler(void *arg)
{
    (void)arg;
    touch_data_ready = true;
    if (touch_interrupt_sem != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(touch_interrupt_sem, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

static void tp_init(void) {
    g_touch_ready = false;

    if (touch_interrupt_sem == NULL) {
        touch_interrupt_sem = xSemaphoreCreateBinary();
        if (touch_interrupt_sem == NULL) {
            ESP_LOGE(TAG_TOUCH, "Failed to create touch interrupt semaphore");
        }
    }

    gpio_install_isr_service(0);

    gpio_reset_pin(TP_INT);
    gpio_reset_pin(TP_RST);
    
    // Hardware Reset Pin
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << TP_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&out_conf);
    
    // Interrupt Pin - configured for falling edge (active low)
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << TP_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&in_conf);

    // Reset Sequence
    gpio_set_level(TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Init I2C bus
    if (i2c_master_init() == ESP_OK) {
        ESP_LOGI(TAG_TOUCH, "I2C Touch Panel Initialized.");

        esp_err_t ret = ESP_FAIL;
        int max_retries = 5;
        for (int attempt = 1; attempt <= max_retries; attempt++) {
            ret = i2c_master_probe(i2c_bus_handle, FT6236_ADDR, 100);
            if (ret == ESP_OK) {
                break;
            }
            if (attempt < max_retries) {
                ESP_LOGW(TAG_TOUCH, "Touch panel probe attempt %d/%d failed, retrying...", attempt, max_retries);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        
        if (ret == ESP_OK) {
            g_touch_ready = true;
            ESP_LOGI(TAG_TOUCH, "Touch panel detected at address 0x%02x", FT6236_ADDR);
            
            gpio_isr_handler_add(TP_INT, tp_interrupt_handler, NULL);
            gpio_intr_enable(TP_INT);
            ESP_LOGI(TAG_TOUCH, "Touch interrupt enabled on GPIO %d", TP_INT);
        } else {
            ESP_LOGW(TAG_TOUCH, "Touch panel not detected at address 0x%02x after %d attempts (err=%s)", FT6236_ADDR, max_retries, esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG_TOUCH, "I2C Init Failed.");
    }
}

static void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    TFT_t *dev = (TFT_t *)lv_display_get_user_data(display);
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t size = w * h;

    uint16_t _x1 = area->x1 + dev->_offsetx;
    uint16_t _x2 = area->x2 + dev->_offsetx;
    uint16_t _y1 = area->y1 + dev->_offsety;
    uint16_t _y2 = area->y2 + dev->_offsety;

    // Send high-speed hardware window bounds
    spi_master_write_command(dev, 0x2A);
    spi_master_write_addr(dev, _x1, _x2);
    spi_master_write_command(dev, 0x2B);
    spi_master_write_addr(dev, _y1, _y2);
    spi_master_write_command(dev, 0x2C);

    // LVGL stores RGB565 in native order; the LCD expects big-endian bytes.
    lv_draw_sw_rgb565_swap(px_map, size);

    // Blast the swapped LVGL color buffer over SPI
    gpio_set_level(dev->_dc, 1);
    spi_master_write_byte(dev->_SPIHandle, px_map, size * 2);

    lv_display_flush_ready(display);
}

static void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    (void)indev;

    if (!g_touch_ready) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    if (!touch_data_ready) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint16_t x_raw, y_raw;
    bool touched = tp_read_point(&x_raw, &y_raw);
    if(touched) {
        data->state = LV_INDEV_STATE_PR;
        
        // For Portrait (90 degrees clockwise rotation relative to previous 320x240 landscape):
        // Map directly to portrait dimensions (240x320)
        data->point.x = x_raw;
        data->point.y = y_raw;
        
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static TFT_t s_dev;

static void display_task(void *arg)
{
    (void)arg;
    
    ESP_LOGI(TAG_DISPLAY, "Entered Display Task");
    // Give the serial monitor time to connect over USB before printing important logs
#if ENABLE_AUDIO_SR
#else
    vTaskDelay(pdMS_TO_TICKS(3000));
#endif
    // Initialize Touch Controller
    tp_init();

    // Change SPI Clock Frequency
    spi_clock_speed(40000000); // 40MHz

    // Initialize SPI Hardware Pins (passing -1 for BL pin so driver doesn't touch it)
    spi_master_init(&s_dev, LCD_SDA, LCD_SCK, LCD_CS, LCD_WR, LCD_RESET, -1);

    // Initialize the display using vertical dimensions (240x320)
    lcdInit(&s_dev, 240, 320, CONFIG_OFFSETX, CONFIG_OFFSETY);

    // Force Hardware Portrait Rotation (MADCTL) with RGB mapping
    spi_master_write_command(&s_dev, 0x36);
    spi_master_write_data_byte(&s_dev, 0x00);

    // Force Color Inversion OFF
    lcdInversionOff(&s_dev);

    lv_init();

    // Create display in portrait mode
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_default(display);
    lv_display_set_user_data(display, &s_dev);

    // Allocate and set draw buffers
    static lv_color_t *buf1 = NULL;
    if (!buf1){
        buf1 = heap_caps_malloc(
            240 * 40 * sizeof(lv_color_t),
            MALLOC_CAP_SPIRAM);
        // buf1 = heap_caps_malloc(240 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
        assert(buf1 != NULL);
    }
    lv_display_set_buffers(display, buf1, NULL, 240 * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Set color format and flush callback
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, my_flush_cb);

    // Create and register touch input device
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Start the EEZ Studio UI
    ui_init();
    // Initialize swipe navigation (attach gesture handlers)
    swipe_init();
    // Initialize app logic (pomodoro period display, etc.)
    app_logic_init();

    set_backlight_brightness(get_var_screen_brightness_val());

    // Main LVGL loop
    while (1) {
        lv_timer_handler();
        ui_tick();
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
}

/* NVS Writing Callbacks START */

// This variable has to be placed in DRAM
DRAM_ATTR static uint8_t temp_vol;
// This task has to be created as main display code is executing in PSRAM
// SPI cache will be disabled while NVS transaction is occuring
void write_nvs_volume_task(void * args){
    // Write to NVS flash
    settings_manager_set_volume(temp_vol);
    vTaskDelete(NULL);
}

// Write volume setting to NVS
static void set_volume_nvs_callback(int32_t volume_value){
    temp_vol = volume_value;

    // Set the current volume
    set_output_vol(temp_vol);
    xTaskCreate(write_nvs_volume_task, "Write volume NVS task", 3000, NULL, 5, NULL);
}

// This variable has to be placed in DRAM
DRAM_ATTR static int32_t temp_bri;
void write_nvs_brightness_task(void * args){
    
    // Write to NVS flash
    settings_manager_set_brightness(temp_bri);
    vTaskDelete(NULL);
}

// Write volume setting to NVS
static void set_brightness_nvs_callback(int32_t brightness){
    temp_bri = brightness;
    xTaskCreate(write_nvs_brightness_task, "Write brightness NVS task", 3000, NULL, 5, NULL);
}

/* NVS Writing Callbacks END*/

static void reset_device_callback(void){
    // TODO: Implement device wifi reset code
}

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

// #define CODEC_DEV_ADDR 0x18
// #define TP_DEV_ADDR 0x38

// static const uint8_t probe_addresses[] = {
//     CODEC_DEV_ADDR,
//     TP_DEV_ADDR,
// };

// static const char *probe_names[] = {
//     "codec",
//     "touch panel",
// };

// // debug task to probe status of devices on I2C bus
// void i2c_probe_task(void *args){

//     while(1){
//         vTaskDelay(pdMS_TO_TICKS(1000));

//         const char *TAG = "I2C";
//         for (size_t i = 0; i < sizeof(probe_addresses) / sizeof(probe_addresses[0]); i++) {
//             esp_err_t err = i2c_master_probe(i2c_bus_handle, probe_addresses[i], 1000);

//             if (err == ESP_OK) {
//                 ESP_LOGI(TAG, "%s device 0x%02X detected and responding", probe_names[i], probe_addresses[i]);
//             } else {
//                 ESP_LOGE(TAG, "%s device 0x%02X not responding (err=0x%x)", probe_names[i], probe_addresses[i], err);
//             }
//         }
//     }

// }
#if ENABLE_DEBUG_MEM

void debug_mem_task(void *args){
    while (1){
        size_t internal_free =
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

        size_t internal_largest =
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

        size_t psram_free =
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        size_t psram_largest =
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        size_t dma_free =
            heap_caps_get_free_size(MALLOC_CAP_DMA);

        size_t total_free =
            esp_get_free_heap_size();

        ESP_LOGI(TAG_MEM,
                 "INT: free=%u largest=%u | "
                 "PSRAM: free=%u largest=%u | "
                 "DMA=%u | TOTAL=%u",
                 (unsigned)internal_free,
                 (unsigned)internal_largest,
                 (unsigned)psram_free,
                 (unsigned)psram_largest,
                 (unsigned)dma_free,
                 (unsigned)total_free);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

void app_main() {
    ESP_LOGI(TAG_MAIN, "Reset reason: %d\n", esp_reset_reason());
    
    xTaskCreatePinnedToCore(connections_init, "connection_task", CONNECTIONS_TASK_STACK_SIZE, NULL, CONNECTIONS_TASK_PRIORITY, NULL, 0);

    // Initialize peripherals
    motor_init();
    backlight_init(0);

#if ENABLE_DEBUG_MEM
    xTaskCreate(debug_mem_task, "debug_mem_task", 4096, NULL, 4, NULL);
#endif

    // Register UI logic callbacks
    app_logic_register_lock_cb(motor_lock);
    app_logic_register_unlock_cb(motor_unlock);
    app_logic_register_reset_cb(reset_device_callback);
    app_logic_register_persist_brightness_cb(set_backlight_brightness);

    // NVS writing callbacks
    app_logic_register_volume_released_cb(set_volume_nvs_callback);
    app_logic_register_brightness_released_cb(set_brightness_nvs_callback);

    settings_manager_init();

    // Initiate audio pipeline
#if ENABLE_AUDIO_SR
    audio_sr_init();
#endif

    // Allocate display task into PSRAM
    xTaskCreatePinnedToCoreWithCaps(
        display_task,
        "display_task",
        DISPLAY_TASK_STACK_SIZE,
        NULL,
        DISPLAY_TASK_PRIORITY,
        NULL,
        0,
        MALLOC_CAP_SPIRAM
    );

    // xTaskCreate(display_task, "display_task", DISPLAY_TASK_STACK_SIZE, NULL, DISPLAY_TASK_PRIORITY, NULL);

#if ENABLE_AUDIO_SR
#if SUSPEND_AUDIO_GPIO
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
    bool enable_voice = (level == 1); // active‑low button
    // voice_module_set_enabled(enable_voice);

    ESP_LOGI(TAG_MAIN, "GPIO40 level=%d → voice module %s",
             level, enable_voice ? "ENABLED" : "DISABLED");

    // Forever loop: poll GPIO40 and toggle voice module when state changes
    bool last_state = enable_voice;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(200)); // 200 ms poll interval
        int cur_level = gpio_get_level(GPIO_NUM_40);
        bool cur_enabled = (cur_level == 1);
        if (cur_enabled != last_state)
        {
            voice_module_set_enabled(cur_enabled);
            ESP_LOGI(TAG_MAIN, "GPIO40 changed: %d → voice module %s",
                     cur_level, cur_enabled ? "ENABLED" : "DISABLED");
            last_state = cur_enabled;
        }
    }
#endif
#endif
    vTaskDelete(NULL);

}
