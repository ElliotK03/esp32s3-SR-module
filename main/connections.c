/*
* WiFi Setup and Google Firebase communication code
*
*/

#include <string.h>
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
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include <time.h>
#include <stdlib.h>

#include "connections.h"

extern const char google_root_ca_pem_start[] asm("_binary_google_root_ca_pem_start");
extern const char google_root_ca_pem_end[]   asm("_binary_google_root_ca_pem_end");
static const char *TAG = "provisioning";

// Event bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t s_wifi_events;
#define FIRESTORE_PROJECT_ID  "lock-in-81e21"
static bool s_wifi_sta_netif_created = false;
static bool s_wifi_ap_netif_created = false;
static bool s_wifi_event_handlers_registered = false;

// ── NVS helpers ───────────────────────────────────────────────────────────

static void nvs_save_credentials(const char *ssid, const char *password) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("wifi_creds", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(wifi_creds) failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Saving Wi-Fi credentials to NVS: ssid='%s'", ssid);

    err = nvs_set_str(h, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str(ssid) failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return;
    }

    err = nvs_set_str(h, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str(password) failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return;
    }

    err = nvs_commit(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit() failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return;
    }

    nvs_close(h);
    ESP_LOGI(TAG, "Credentials saved to NVS successfully");

    // Immediate read-back check to prove the data is actually persisted in NVS.
    char verify_ssid[64] = {0};
    char verify_password[64] = {0};
    err = nvs_open("wifi_creds", NVS_READONLY, &h);
    if (err == ESP_OK) {
        size_t verify_ssid_len = sizeof(verify_ssid);
        size_t verify_password_len = sizeof(verify_password);

        esp_err_t ssid_err = nvs_get_str(h, "ssid", verify_ssid, &verify_ssid_len);
        esp_err_t pass_err = nvs_get_str(h, "password", verify_password, &verify_password_len);

        if (ssid_err == ESP_OK && pass_err == ESP_OK) {
            ESP_LOGI(TAG, "NVS read-back OK: ssid='%s', password_len=%u",
                     verify_ssid,
                     (unsigned)strlen(verify_password));
        } else {
            ESP_LOGE(TAG, "NVS read-back failed: ssid=%s password=%s",
                     esp_err_to_name(ssid_err),
                     esp_err_to_name(pass_err));
        }
        nvs_close(h);
    } else {
        ESP_LOGE(TAG, "NVS read-back open failed: %s", esp_err_to_name(err));
    }
}

static bool nvs_load_credentials(char *ssid, char *password, size_t len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("wifi_creds", NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open(wifi_creds, read-only) failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t sl = len, pl = len;
    err = nvs_get_str(h, "ssid", ssid, &sl);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_str(ssid) failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return false;
    }

    err = nvs_get_str(h, "password", password, &pl);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_str(password) failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return false;
    }

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded Wi-Fi credentials from NVS: ssid='%s'", ssid);
    return true;
}

// ── AP mode ───────────────────────────────────────────────────────────────

static void start_ap(void) {
    if (!s_wifi_ap_netif_created) {
        esp_netif_create_default_wifi_ap();
        s_wifi_ap_netif_created = true;
    }

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid            = "PomodoroTimer",   // AP name visible on phone
            .ssid_len        = 0,
            .password        = "",                // open AP, no password
            .max_connection  = 1,
            .authmode        = WIFI_AUTH_OPEN,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
    ESP_LOGI(TAG, "AP started → SSID: PomodoroTimer  IP: 192.168.4.1");
}

// ── HTTP server ───────────────────────────────────────────────────────────

typedef struct {
    char ssid[64];
    char password[64];
} wifi_credentials_t;

static wifi_credentials_t s_pending_creds = {0};
static bool s_got_credentials = false;

// GET /status  → app polls this to check if ESP32 is alive
static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ready\",\"device\":\"PomodoroTimer\"}");
    return ESP_OK;
}

// POST /wifi  → app sends { "ssid": "...", "password": "..." }
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

static httpd_handle_t start_http_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    httpd_start(&server, &config);

    httpd_uri_t status_uri = {
        .uri      = "/status",
        .method   = HTTP_GET,
        .handler  = status_handler,
    };
    httpd_uri_t wifi_uri = {
        .uri      = "/wifi",
        .method   = HTTP_POST,
        .handler  = wifi_handler,
    };

    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &wifi_uri);
    return server;
}

// ── STA mode (connect to home Wi-Fi) ─────────────────────────────────────

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
    const int max_attempts = 3;

    if (!s_wifi_sta_netif_created) {
        esp_netif_create_default_wifi_sta();
        s_wifi_sta_netif_created = true;
    }

    if (!s_wifi_event_handlers_registered) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_event_handler, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_event_handler, NULL);
        s_wifi_event_handlers_registered = true;
    }

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password));

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        s_wifi_events = xEventGroupCreate();
        if (s_wifi_events == NULL) {
            ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
            return false;
        }

        ESP_LOGI(TAG, "Wi-Fi connect attempt %d/%d to SSID '%s'", attempt, max_attempts, ssid);

        esp_wifi_disconnect();
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(200));

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
        esp_wifi_start();
        esp_wifi_set_ps(WIFI_PS_NONE); // Ensure radio stays awake
        esp_wifi_connect();

        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_events,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE,
            pdMS_TO_TICKS(15000)   // 15s timeout
        );

        vEventGroupDelete(s_wifi_events);
        s_wifi_events = NULL;

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Wi-Fi connected on attempt %d", attempt);
            return true;
        }

        ESP_LOGW(TAG, "Wi-Fi attempt %d failed; retrying", attempt);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return false;
}

// Button handling removed for boards without a dedicated reset button.
// clear_credentials_and_reboot() retained if needed elsewhere.

static void clear_credentials_and_reboot(void) {
    ESP_LOGW(TAG, "Clearing saved credentials and rebooting into AP mode...");
    nvs_handle_t h;
    if (nvs_open("wifi_creds", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

// ── NTP time sync ─────────────────────────────────────────────────────────
static void sync_time(void) {
    // Set timezone to GMT+8 (CST-8 / SGT-8)
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retries = 0;
    
    ESP_LOGI(TAG, "Waiting for NTP time sync...");
    while (timeinfo.tm_year < (2020 - 1900) && retries < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        time(&now);
        localtime_r(&now, &timeinfo);
        retries++;
    }
    
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "FAILED to sync time! Year is still: %d", timeinfo.tm_year + 1900);
        ESP_LOGE(TAG, "SSL will fail because the clock is wrong.");
    } else {
        ESP_LOGI(TAG, "Time synced successfully! Current year: %d", timeinfo.tm_year + 1900);
    }
}

// ── Firestore push ────────────────────────────────────────────────────────

static void firestore_push_blink(int blink_count, bool led_on) {
    // 1. Get current timestamp
    time_t now;
    time(&now);
    long long timestamp_ms = (long long)now * 1000;

    // 2. Get RSSI correctly
    int rssi = 0;
    esp_wifi_sta_get_rssi(&rssi);

    // 3. Prepare strings for the JSON
    char body[512];
    char ts_str[32];
    char count_str[16];
    snprintf(ts_str, sizeof(ts_str), "%lld", timestamp_ms);
    snprintf(count_str, sizeof(count_str), "%d", blink_count);

    // 4. Build the JSON body
    snprintf(body, sizeof(body),
        "{"
          "\"fields\":{"
            "\"timestamp\":{\"integerValue\":\"%s\"},"
            "\"blink_count\":{\"integerValue\":\"%s\"},"
            "\"led_state\":{\"booleanValue\":%s},"
            "\"wifi_rssi\":{\"integerValue\":\"%d\"},"
            "\"free_heap_bytes\":{\"integerValue\":\"%d\"},"
            "\"uptime_seconds\":{\"integerValue\":\"%d\"}"
          "}"
        "}",
        ts_str,
        count_str,
        led_on ? "true" : "false",
        rssi,
        (int)esp_get_free_heap_size(), // heap
        (int)(esp_timer_get_time() / 1000000) // uptime in seconds
    ); 

    // 5. Build URL
    char url[512];
    snprintf(url, sizeof(url),
        "https://firestore.googleapis.com/v1/projects/%s"
        "/databases/(default)/documents/diagnostics",
        FIRESTORE_PROJECT_ID);

    // 6. HTTP POST CONFIGURATION (FIXED: Using crt_bundle_attach and 20s timeout)
    esp_http_client_config_t config = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = 20000,
        .cert_pem          = google_root_ca_pem_start,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Firestore ✓ blink=%d led=%s HTTP=%d",
                 blink_count,
                 led_on ? "ON" : "OFF",
                 esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "Firestore ✗ %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

#define FIREBASE_TASK_PRIORITY 2
#define FIREBASE_TASK_STACK_SIZE (1024 * 5)

// ── Blink task ────────────────────────────────────────────────────────────
static void firebase_task(void *arg) {

    int blink_count = 0;

    while (true) {
        blink_count++;
        firestore_push_blink(blink_count, true);
        vTaskDelay(pdMS_TO_TICKS(5000));

    }
}

void connections_init(void * arg) {
    // NVS init
    esp_err_t ret = nvs_flash_init();
    ESP_LOGI(TAG, "nvs_flash_init(): %s", esp_err_to_name(ret));
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase; clearing and reinitializing");
        nvs_flash_erase();
        ret = nvs_flash_init();
        ESP_LOGI(TAG, "nvs_flash_init() after erase: %s", esp_err_to_name(ret));
    }

    nvs_stats_t stats = {0};
    ret = nvs_get_stats(NULL, &stats);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS stats: used=%u free=%u total=%u namespaces=%u",
                 (unsigned)stats.used_entries,
                 (unsigned)stats.free_entries,
                 (unsigned)stats.total_entries,
                 (unsigned)stats.namespace_count);
    } else {
        ESP_LOGW(TAG, "nvs_get_stats() failed: %s", esp_err_to_name(ret));
    }

    // Button monitor task removed (no hardware button on this board)

    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Check NVS for saved credentials first
    char saved_ssid[64] = {0};
    char saved_pass[64] = {0};
    if (nvs_load_credentials(saved_ssid, saved_pass, sizeof(saved_ssid))) {
        ESP_LOGI(TAG, "Found saved credentials, connecting to: %s", saved_ssid);
        if (connect_to_wifi(saved_ssid, saved_pass)) {
            ESP_LOGI(TAG, "Connected with saved credentials — starting main app");
            sync_time();
            xTaskCreate(firebase_task, "firebase task", FIREBASE_TASK_STACK_SIZE, NULL, FIREBASE_TASK_PRIORITY, NULL);
            vTaskDelete(NULL);
            return;
        }
        ESP_LOGW(TAG, "Saved credentials failed, falling back to AP mode");
    }

    // No credentials or failed → start AP provisioning
    start_ap();
    httpd_handle_t server = start_http_server();

    ESP_LOGI(TAG, "Waiting for Wi-Fi credentials from app...");
    while (!s_got_credentials) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    httpd_stop(server);
    esp_wifi_stop();

    ESP_LOGI(TAG, "Got credentials → SSID: %s", s_pending_creds.ssid);
    nvs_save_credentials(s_pending_creds.ssid, s_pending_creds.password);

    if (connect_to_wifi(s_pending_creds.ssid, s_pending_creds.password)) {
        ESP_LOGI(TAG, "Provisioning complete!");
        sync_time();                                    
        
        // Blink task removed (no onboard LED on this board)
    } else {
        ESP_LOGE(TAG, "Connection failed — rebooting into AP mode");
        esp_restart();
    }
    
    vTaskDelete(NULL);
}