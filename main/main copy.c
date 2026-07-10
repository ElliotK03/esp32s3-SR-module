#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "pinout.h"

extern const char google_root_ca_pem_start[] asm("_binary_google_root_ca_pem_start");
extern const char google_root_ca_pem_end[]   asm("_binary_google_root_ca_pem_end");

static const char *TAG = "provisioning";

// ── Defines ───────────────────────────────────────────────────────────────

#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAIL_BIT         BIT1
#define FIRESTORE_PROJECT_ID  "lock-in-81e21"
#define NVS_HW_NAMESPACE      "hw_prefs"
#define LEDC_BACKLIGHT_CH     LEDC_CHANNEL_0
#define LEDC_BACKLIGHT_TIMER  LEDC_TIMER_0
#define RESET_BUTTON_GPIO     GPIO_NUM_0

// ── Globals ───────────────────────────────────────────────────────────────

static EventGroupHandle_t s_wifi_events;
static char  last_lock_status[16]  = "UNLOCKED";
static int32_t last_speaker_volume   = 50;
static int32_t last_screen_brightness = 50;
static TaskHandle_t motor_task_handle = NULL;
static bool s_motor_is_busy = false;

typedef struct {
    char ssid[64];
    char password[64];
} wifi_credentials_t;

static wifi_credentials_t s_pending_creds = {0};
static bool s_got_credentials = false;

// ── Device ID ─────────────────────────────────────────────────────────────

static void get_device_id(char *buf, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ── NVS Helpers ───────────────────────────────────────────────────────────

static void nvs_save_credentials(const char *ssid, const char *password) {
    nvs_handle_t h;
    nvs_open("wifi_creds", NVS_READWRITE, &h);
    nvs_set_str(h, "ssid",     ssid);
    nvs_set_str(h, "password", password);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Wi-Fi credentials saved to NVS");
}

static bool nvs_load_credentials(char *ssid, char *password, size_t len) {
    nvs_handle_t h;
    if (nvs_open("wifi_creds", NVS_READONLY, &h) != ESP_OK) return false;
    size_t sl = len, pl = len;
    bool ok = nvs_get_str(h, "ssid",     ssid,     &sl) == ESP_OK &&
              nvs_get_str(h, "password", password, &pl) == ESP_OK;
    nvs_close(h);
    return ok;
}

static void save_hw_preference_str(const char *key, const char *value) {
    nvs_handle_t h;
    if (nvs_open(NVS_HW_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, key, value);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void save_hw_preference_int(const char *key, int32_t value) {
    nvs_handle_t h;
    if (nvs_open(NVS_HW_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, key, value);
        nvs_commit(h);
        nvs_close(h);
    }
}

// ── Hardware Control ──────────────────────────────────────────────────────

static void actuate_motor(const char *status) {
    ESP_LOGI("HW_CONTROL", "Motor → %s", status);

    gpio_set_direction(MOTOR_B_L, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_B_H, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_A_L, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_A_H, GPIO_MODE_OUTPUT);

    if (strcmp(status, "LOCKED") == 0) {
        gpio_set_level(MOTOR_B_L, 1);
        gpio_set_level(MOTOR_A_H, 1);
        gpio_set_level(MOTOR_B_H, 0);
        gpio_set_level(MOTOR_A_L, 0);
        vTaskDelay(pdMS_TO_TICKS(1500));
    } else if (strcmp(status, "UNLOCKED") == 0) {
        gpio_set_level(MOTOR_B_L, 0);
        gpio_set_level(MOTOR_A_H, 0);
        gpio_set_level(MOTOR_B_H, 1);
        gpio_set_level(MOTOR_A_L, 1);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    // Stop motor
    gpio_set_level(MOTOR_B_L, 0);
    gpio_set_level(MOTOR_A_H, 0);
    gpio_set_level(MOTOR_B_H, 0);
    gpio_set_level(MOTOR_A_L, 0);
}

static void adjust_speaker_volume(int volume) {
    ESP_LOGI("HW_CONTROL", "Speaker volume → %d%%", volume);
    // TODO: Hook to  I2S gain registry
}

static void adjust_screen_brightness(int brightness) {
    if (brightness < 0)   brightness = 0;
    if (brightness > 100) brightness = 100;
    uint32_t duty = (brightness * 255) / 100;
    ESP_LOGI("HW_CONTROL", "Brightness → %d%% (duty %lu)", brightness, duty);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_BACKLIGHT_CH, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_BACKLIGHT_CH));
}

static void init_screen_brightness_pwm(void) {
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_BACKLIGHT_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_BACKLIGHT_CH,
        .timer_sel  = LEDC_BACKLIGHT_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = LCD_BL,
        .duty       = 127,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

static void load_hardware_settings_from_nvs(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_HW_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(last_lock_status);
        nvs_get_str(h, "lock_status",   last_lock_status,      &len);
        nvs_get_i32(h, "speaker_volume", &last_speaker_volume);
        nvs_get_i32(h, "screen_bright",  &last_screen_brightness);
        nvs_close(h);
    }
    ESP_LOGI("NVS", "Loaded: Lock=%s Vol=%ld Bright=%ld",
             last_lock_status, last_speaker_volume, last_screen_brightness);
             
    // Bootstrap light peripherals instantly
    adjust_screen_brightness(last_screen_brightness);
    adjust_speaker_volume(last_speaker_volume);

    // REMOVED: actuate_motor(last_lock_status); <-- Stripped out to prevent the Watchdog crash!
}




// ── Firestore HTTP chunk assembler ────────────────────────────────────────

typedef struct {
    char *buffer;
    int   buffer_len;
} http_receive_buffer_t;

static esp_err_t firestore_get_handler(esp_http_client_event_t *evt) {
    http_receive_buffer_t *resp = (http_receive_buffer_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        
            int   new_len = resp->buffer_len + evt->data_len;
            char *new_buf = realloc(resp->buffer, new_len + 1);
            if (new_buf) {
                resp->buffer = new_buf;
                memcpy(resp->buffer + resp->buffer_len, evt->data, evt->data_len);
                resp->buffer_len = new_len;
                resp->buffer[resp->buffer_len] = '\0';
            
        }
    }
    return ESP_OK;
}                               

static void motor_worker_task(void *arg) {
    uint32_t notification_value;

    while (true) {
        // Sleep indefinitely until another task sends a bitwise signal
        xTaskNotifyWait(0x00, ULONG_MAX, &notification_value, portMAX_DELAY);
        
        // 1 = LOCKED, 2 = UNLOCKED
        if (notification_value == 1) {
            actuate_motor("LOCKED");
        } else if (notification_value == 2) {
            actuate_motor("UNLOCKED");
        }

        // ─── RELEASE THE GATE ───
        // The motor has safely stopped spinning. Ready for the next instruction!
        s_motor_is_busy = false;
        
        // Explicitly hand control back to the OS scheduler loop
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}



// ── Firestore JSON parser ─────────────────────────────────────────────────

static void parse_and_apply_settings(const char *json_string) {
    cJSON *root = cJSON_Parse(json_string);
    if (!root) return;

    cJSON *fields = cJSON_GetObjectItem(root, "fields");
    if (!fields) { cJSON_Delete(root); return; }

    // Lock status
    cJSON *lock_obj = cJSON_GetObjectItem(fields, "lock_status");
    if (lock_obj) {
        cJSON *val = cJSON_GetObjectItem(lock_obj, "stringValue");
        if (val && cJSON_IsString(val)) {
            if (strcmp(last_lock_status, val->valuestring) != 0) {
                strlcpy(last_lock_status, val->valuestring, sizeof(last_lock_status));
                save_hw_preference_str("lock_status", last_lock_status);
                
                /// ─── SAFE PROCESSOR NOTIFICATION SIGNAL ───
                if (motor_task_handle != NULL) {
                    uint32_t signal_val = (strcmp(last_lock_status, "LOCKED") == 0) ? 1 : 2;
                    s_motor_is_busy = true; // Lock the interlock gate instantly
                    xTaskNotify(motor_task_handle, signal_val, eSetValueWithOverwrite);
                }
            }
        }
    }

    // Speaker volume
    cJSON *vol_obj = cJSON_GetObjectItem(fields, "speaker_volume");
    if (vol_obj) {
        int volume = -1;
        cJSON *iv = cJSON_GetObjectItem(vol_obj, "integerValue");
        cJSON *dv = cJSON_GetObjectItem(vol_obj, "doubleValue");
        if      (iv && cJSON_IsString(iv)) volume = atoi(iv->valuestring);
        else if (iv && cJSON_IsNumber(iv)) volume = (int)iv->valuedouble;
        else if (dv && cJSON_IsNumber(dv)) volume = (int)dv->valuedouble;
        if (volume >= 0 && volume != last_speaker_volume) {
            last_speaker_volume = volume;
            save_hw_preference_int("speaker_volume", last_speaker_volume);
            adjust_speaker_volume(last_speaker_volume);
        }
    }

    // Screen brightness
    cJSON *bright_obj = cJSON_GetObjectItem(fields, "screen_brightness");
    if (bright_obj) {
        int brightness = -1;
        cJSON *iv = cJSON_GetObjectItem(bright_obj, "integerValue");
        cJSON *dv = cJSON_GetObjectItem(bright_obj, "doubleValue");
        if      (iv && cJSON_IsString(iv)) brightness = atoi(iv->valuestring);
        else if (iv && cJSON_IsNumber(iv)) brightness = (int)iv->valuedouble;
        else if (dv && cJSON_IsNumber(dv)) brightness = (int)dv->valuedouble;
        if (brightness >= 0 && brightness != last_screen_brightness) {
            last_screen_brightness = brightness;
            save_hw_preference_int("screen_bright", last_screen_brightness);
            adjust_screen_brightness(last_screen_brightness);
        }
    }

    cJSON_Delete(root);
}

// ── Firestore listener task ───────────────────────────────────────────────

static void firestore_listener_task(void *arg) {
    char device_id[18];
    get_device_id(device_id, sizeof(device_id));

    char url[256];
    snprintf(url, sizeof(url),
             "https://firestore.googleapis.com/v1/projects/%s"
             "/databases/(default)/documents/devices/%s",
             FIRESTORE_PROJECT_ID, device_id);

    while (true) {
        http_receive_buffer_t response = { .buffer = NULL, .buffer_len = 0 };

        esp_http_client_config_t config = {
            .url           = url,
            .method        = HTTP_METHOD_GET,
            .cert_pem      = google_root_ca_pem_start,
            .event_handler = firestore_get_handler,
            .user_data     = &response,
            .timeout_ms    = 10000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI("HW_CONTROL", "Firestore GET → HTTP %d", status);
            if (status == 200 && response.buffer) {
                //───  CORRUPT HEAP PATCH ───
                // 1. Cleanly close the client network socket first so it releases the heap pointer completely
                esp_http_client_cleanup(client); 
                client = NULL; // Zero out the handle to prevent accidental duplicate cleanup down below
                //After buffer is isolated from the client cleanup, we can safely log and parse it without risking the Watchdog crash due to the heap corruption issue in esp_http_client.
                ESP_LOGI("HW_CONTROL", "Response: %s", response.buffer);
                parse_and_apply_settings(response.buffer);
            }
        } else {
            ESP_LOGE("HW_CONTROL", "Fetch failed: %s", esp_err_to_name(err));
        }

       // Only call cleanup if the client handle hasn't already been wiped out above!
        if (client != NULL) {
            esp_http_client_cleanup(client);
        }

        if (response.buffer) free(response.buffer);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// ── Firestore diagnostics push ────────────────────────────────────────────

static void firestore_push_diagnostics(void) {
    time_t now;
    time(&now);
    long long timestamp_ms = (long long)now * 1000;

    int rssi = 0;
    esp_wifi_sta_get_rssi(&rssi);

    char ts_str[32];
    snprintf(ts_str, sizeof(ts_str), "%lld", timestamp_ms);

    char body[512];
    snprintf(body, sizeof(body),
        "{"
          "\"fields\":{"
            "\"timestamp\":{\"integerValue\":\"%s\"},"
            "\"wifi_rssi\":{\"integerValue\":\"%d\"},"
            "\"free_heap_bytes\":{\"integerValue\":\"%d\"},"
            "\"uptime_seconds\":{\"integerValue\":\"%d\"},"
            "\"firmware_version\":{\"stringValue\":\"1.0.0\"}"
          "}"
        "}",
        ts_str, rssi,
        (int)esp_get_free_heap_size(),
        (int)(esp_timer_get_time() / 1000000)
    );

    char device_id[18];
    get_device_id(device_id, sizeof(device_id));

    char url[256];
    snprintf(url, sizeof(url),
        "https://firestore.googleapis.com/v1/projects/%s"
        "/databases/(default)/documents/diagnostics/%s",
        FIRESTORE_PROJECT_ID, device_id);

    esp_http_client_config_t config = {
        .url        = url,
        .method     = HTTP_METHOD_PATCH,
        .cert_pem   = google_root_ca_pem_start,
        .timeout_ms = 20000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Diagnostics push → HTTP %d",
                 esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "Diagnostics push failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

// ── Diagnostics task ──────────────────────────────────────────────────────

static void diagnostics_task(void *arg) {
    while (true) {
        firestore_push_diagnostics();
        vTaskDelay(pdMS_TO_TICKS(10000)); // push every 10 seconds
    }
}

// ── AP mode ───────────────────────────────────────────────────────────────

static void start_ap(void) {
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = "PomodoroTimer",
            .ssid_len       = 0,
            .password       = "",
            .max_connection = 1,
            .authmode       = WIFI_AUTH_OPEN,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
    ESP_LOGI(TAG, "AP started → SSID: PomodoroTimer  IP: 192.168.4.1");
}

// ── HTTP server ───────────────────────────────────────────────────────────

static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ready\",\"device\":\"PomodoroTimer\"}");
    return ESP_OK;
}

static esp_err_t wifi_handler(httpd_req_t *req) {
    char buf[256] = {0};
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid_j = cJSON_GetObjectItem(json, "ssid");
    cJSON *pass_j = cJSON_GetObjectItem(json, "password");

    if (!cJSON_IsString(ssid_j) || !cJSON_IsString(pass_j)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid/password");
        return ESP_FAIL;
    }

    strlcpy(s_pending_creds.ssid,     ssid_j->valuestring, sizeof(s_pending_creds.ssid));
    strlcpy(s_pending_creds.password, pass_j->valuestring, sizeof(s_pending_creds.password));
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"received\"}");
    s_got_credentials = true;
    return ESP_OK;
}

static esp_err_t device_id_handler(httpd_req_t *req) {
    char device_id[18];
    get_device_id(device_id, sizeof(device_id));
    char response[64];
    snprintf(response, sizeof(response), "{\"device_id\":\"%s\"}", device_id);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

static httpd_handle_t start_http_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    httpd_start(&server, &config);

    httpd_uri_t status_uri    = { .uri = "/status",    .method = HTTP_GET,  .handler = status_handler };
    httpd_uri_t wifi_uri      = { .uri = "/wifi",      .method = HTTP_POST, .handler = wifi_handler };
    httpd_uri_t device_id_uri = { .uri = "/device_id", .method = HTTP_GET,  .handler = device_id_handler };

    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &wifi_uri);
    httpd_register_uri_handler(server, &device_id_uri);
    return server;
}

// ── Wi-Fi STA ─────────────────────────────────────────────────────────────

static void sta_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static bool connect_to_wifi(const char *ssid, const char *password) {
    s_wifi_events = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,   sta_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, sta_event_handler, NULL);

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid,     ssid,     sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_events,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000)
    );
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

// ── Reset button task ─────────────────────────────────────────────────────

static void clear_credentials_and_reboot(void) {
    ESP_LOGW(TAG, "Clearing credentials and rebooting...");
    nvs_handle_t h;
    if (nvs_open("wifi_creds", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static void button_task(void *arg) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << RESET_BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    int hold_ms = 0;
    while (true) {
        if (gpio_get_level(RESET_BUTTON_GPIO) == 0) {
            hold_ms += 100;
            if (hold_ms >= 3000) clear_credentials_and_reboot();
        } else {
            hold_ms = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ── NTP time sync ─────────────────────────────────────────────────────────

static void sync_time(void) {
    ESP_LOGI(TAG, "Syncing time via NTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_init();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retries = 0;
    const int max_retries = 40;

    while (timeinfo.tm_year < (2020 - 1900) && retries < max_retries) {
        ESP_LOGI(TAG, "Waiting for NTP... (%d/%d)", retries + 1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(500));
        time(&now);
        localtime_r(&now, &timeinfo);
        retries++;
    }

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "NTP sync failed — retrying...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_sntp_stop();
        sync_time();
    } else {
        ESP_LOGI(TAG, "Time synced: %s", asctime(&timeinfo));
    }
}

// ── app_main ──────────────────────────────────────────────────────────────

void app_main(void) {



    // NVS init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Button reset task
    xTaskCreate(button_task, "button", 2048, NULL, 10, NULL);

    // ─── SPAWN PERMANENT MOTOR THREAD HERE and pin to core 1───
    xTaskCreate(motor_worker_task, "motor_worker", 4096, NULL, 2, &motor_task_handle);

    // Hardware init
    init_screen_brightness_pwm();
    load_hardware_settings_from_nvs();


    // Network init
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Try saved credentials
    char saved_ssid[64] = {0};
    char saved_pass[64] = {0};
    if (nvs_load_credentials(saved_ssid, saved_pass, sizeof(saved_ssid))) {
        ESP_LOGI(TAG, "Found saved credentials → %s", saved_ssid);
        if (connect_to_wifi(saved_ssid, saved_pass)) {
            ESP_LOGI(TAG, "Connected — starting main app");
            vTaskDelay(pdMS_TO_TICKS(2000));
            sync_time();
            xTaskCreate(diagnostics_task,        "diag",        8192, NULL, 5, NULL);
            xTaskCreate(firestore_listener_task, "hw_listener", 8192, NULL, 5, NULL);
            return;
        }
        ESP_LOGW(TAG, "Saved credentials failed — starting AP mode");
    }

    // AP provisioning
    start_ap();
    httpd_handle_t server = start_http_server();

    ESP_LOGI(TAG, "Waiting for credentials from app...");
    while (!s_got_credentials) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    httpd_stop(server);
    esp_wifi_stop();

    ESP_LOGI(TAG, "Got credentials → SSID: %s", s_pending_creds.ssid);
    nvs_save_credentials(s_pending_creds.ssid, s_pending_creds.password);

    if (connect_to_wifi(s_pending_creds.ssid, s_pending_creds.password)) {
        ESP_LOGI(TAG, "Provisioning complete!");
        vTaskDelay(pdMS_TO_TICKS(2000));
        sync_time();
        xTaskCreate(diagnostics_task,        "diag",        8192, NULL, 5, NULL);
        xTaskCreate(firestore_listener_task, "hw_listener", 8192, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "Connection failed — rebooting");
        esp_restart();
    }
}