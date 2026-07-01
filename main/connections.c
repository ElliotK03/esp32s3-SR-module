/*
* WiFi Setup and Google Firebase communication code
*
*/

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>

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
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_heap_caps.h"

#include "app_logic.h"
#include "connections.h"

static const char *TAG = "provisioning";

// Event bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define START_PAIRING_BIT  BIT2
static EventGroupHandle_t s_wifi_events;
#define FIRESTORE_PROJECT_ID  "lock-in-81e21"
#define FIREBASE_TASK_PRIORITY 2
#define FIREBASE_TASK_STACK_SIZE (1024 * 5)
static bool s_wifi_sta_netif_created = false;
static bool s_wifi_ap_netif_created = false;
static bool s_wifi_event_handlers_registered = false;
static bool s_network_ready = false;
static char s_active_ssid[64] = {0};
static TaskHandle_t s_firebase_task_handle = NULL;

#define WIFI_RETRY_DELAY_MS 5000

void get_device_id(char *buf, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void update_wifi_status(const char *status) {
    set_var_wifi_status_str(status ? status : "Not connected");
}

static void sync_time(void);
static void firebase_task(void *arg);

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
    update_wifi_status("Pairing...");
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
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
        return NULL;
    }

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
    httpd_uri_t device_id_uri = { 
        .uri = "/device_id",
        .method = HTTP_GET,
        .handler = device_id_handler
    };

    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &wifi_uri);
    httpd_register_uri_handler(server, &device_id_uri);
    return server;
}

static void run_pairing_session(char *ssid, char *password, size_t len) {
    s_got_credentials = false;
    memset(&s_pending_creds, 0, sizeof(s_pending_creds));

    esp_wifi_disconnect();
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(200));

    start_ap();
    httpd_handle_t server = start_http_server();
    if (server == NULL) {
        update_wifi_status("Not connected");
        return;
    }

    ESP_LOGI(TAG, "Waiting for Wi-Fi credentials from app...");
    while (!s_got_credentials) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    httpd_stop(server);
    esp_wifi_stop();

    strlcpy(ssid, s_pending_creds.ssid, len);
    strlcpy(password, s_pending_creds.password, len);
    nvs_save_credentials(ssid, password);
    update_wifi_status("Not connected");
    ESP_LOGI(TAG, "Pairing credentials received for SSID '%s'", ssid);
}

// ── STA mode (connect to home Wi-Fi) ─────────────────────────────────────

static void sta_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_network_ready = false;
        update_wifi_status("Not connected");
        if (s_wifi_events != NULL) {
            xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_network_ready = true;
        update_wifi_status(s_active_ssid);
        if (s_wifi_events != NULL) {
            xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        }
    }
}

static void ensure_sta_ready(void) {
    if (!s_wifi_sta_netif_created) {
        esp_netif_create_default_wifi_sta();
        s_wifi_sta_netif_created = true;
    }

    if (!s_wifi_event_handlers_registered) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_event_handler, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_event_handler, NULL);
        s_wifi_event_handlers_registered = true;
    }
}

static bool ssid_is_present(const char *ssid) {
    wifi_scan_config_t scan_cfg = {
        .ssid = (uint8_t *)ssid,
        .show_hidden = true,
    };

    ESP_LOGI(TAG, "Scanning for saved SSID '%s'", ssid);
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(err));
        return false;
    }

    uint16_t ap_count = 0;
    err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan result count failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Scan found %u AP(s) matching '%s'", (unsigned)ap_count, ssid);
    return ap_count > 0;
}

static bool connect_to_wifi_once(const char *ssid, const char *password) {
    ensure_sta_ready();

    if (s_wifi_events == NULL) {
        ESP_LOGE(TAG, "Wi-Fi event group is not initialized");
        return false;
    }

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password));
    strlcpy(s_active_ssid, ssid, sizeof(s_active_ssid));

    xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_network_ready = false;
    update_wifi_status("Not connected");

    esp_wifi_disconnect();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (!ssid_is_present(ssid)) {
        ESP_LOGW(TAG, "Saved SSID '%s' not present; will keep scanning", ssid);
        return false;
    }

    ESP_LOGI(TAG, "Connecting to SSID '%s'", ssid);
    xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_events,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE, pdFALSE,
        pdMS_TO_TICKS(15000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected to '%s'", ssid);
        return true;
    }

    ESP_LOGW(TAG, "Wi-Fi connection attempt failed for '%s'", ssid);
    return false;
}

static void start_network_services(void) {
    if (!s_network_ready) {
        return;
    }

    sync_time();

    if (s_firebase_task_handle == NULL) {
        xTaskCreate(firebase_task, "firebase task", FIREBASE_TASK_STACK_SIZE, NULL, FIREBASE_TASK_PRIORITY, &s_firebase_task_handle);
    }
}

static void connect_saved_wifi_loop(const char *ssid, const char *password) {
    while (true) {
        if (connect_to_wifi_once(ssid, password)) {
            start_network_services();
            while (s_network_ready) {
                EventBits_t bits = xEventGroupWaitBits(
                    s_wifi_events,
                    WIFI_FAIL_BIT | START_PAIRING_BIT,
                    pdTRUE, pdFALSE,
                    pdMS_TO_TICKS(1000)
                );

                if (bits & START_PAIRING_BIT) {
                    return;
                }
            }
        }

        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_events,
            START_PAIRING_BIT,
            pdTRUE, pdFALSE,
            pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS)
        );
        if (bits & START_PAIRING_BIT) {
            return;
        }
    }
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

// ── Firestore push ────────────────────────────────────────────────────────

static void firestore_push_blink(int blink_count, bool led_on)
{
    // Timestamp
    time_t now;
    time(&now);
    long long timestamp_ms = (long long)now * 1000;

    // Wi-Fi RSSI
    int rssi = 0;
    esp_wifi_sta_get_rssi(&rssi);

    // Device ID
    char device_id[18];
    get_device_id(device_id, sizeof(device_id));

    // JSON body
    char body[512];
    snprintf(body, sizeof(body),
        "{"
        "\"fields\":{"
            "\"timestamp\":{\"integerValue\":\"%lld\"},"
            "\"blink_count\":{\"integerValue\":\"%d\"},"
            "\"led_state\":{\"booleanValue\":%s},"
            "\"wifi_rssi\":{\"integerValue\":\"%d\"},"
            "\"free_heap_bytes\":{\"integerValue\":\"%u\"},"
            "\"uptime_seconds\":{\"integerValue\":\"%llu\"}"
        "}"
        "}",
        timestamp_ms,
        blink_count,
        led_on ? "true" : "false",
        rssi,
        (unsigned int)esp_get_free_heap_size(),
        (unsigned long long)(esp_timer_get_time() / 1000000ULL));

    // Firestore document URL
    char url[512];
    snprintf(url, sizeof(url),
        "https://firestore.googleapis.com/v1/projects/%s"
        "/databases/(default)/documents/diagnostics/%s",
        FIRESTORE_PROJECT_ID,
        device_id);

    // ESP_LOGI(TAG, "Firestore URL: %s", url);
    // ESP_LOGI(TAG, "Request Body: %s", body);

    esp_http_client_config_t config = {
        .url               = url,
        .method            = HTTP_METHOD_PATCH,
        .timeout_ms        = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");

    // Uncomment if your Firestore requires authentication:
    //
    // esp_http_client_set_header(client,
    //     "Authorization",
    //     "Bearer YOUR_ACCESS_TOKEN");

    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK){
        int status = esp_http_client_get_status_code(client);

        if (status >= 200 && status < 300){
            ESP_LOGI(TAG, "Diagnostics upload ✓");
        }
        else{
            ESP_LOGE(TAG, "Diagnostics upload ✗ (HTTP %d)", status);
        }
    }
    else{
        ESP_LOGE(TAG, "Diagnostics upload ✗ (%s)", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// ── Blink task ────────────────────────────────────────────────────────────
static void firebase_task(void *arg) {

    int blink_count = 0;

    while (true) {
        if (!s_network_ready) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        blink_count++;
        firestore_push_blink(blink_count, true);
        vTaskDelay(pdMS_TO_TICKS(5000));

    }
}

void connections_start_pairing(void) {
    if (s_wifi_events == NULL) {
        ESP_LOGW(TAG, "Pairing requested before Wi-Fi task initialized");
        return;
    }

    update_wifi_status("Pairing...");
    xEventGroupSetBits(s_wifi_events, START_PAIRING_BIT);
}

bool connections_is_network_ready(void) {
    return s_network_ready;
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
    s_wifi_events = xEventGroupCreate();
    if (s_wifi_events == NULL) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        vTaskDelete(NULL);
        return;
    }

    char saved_ssid[64] = {0};
    char saved_pass[64] = {0};
    bool has_saved_credentials = nvs_load_credentials(saved_ssid, saved_pass, sizeof(saved_ssid));
    update_wifi_status("Not connected");

    while (true) {
        if (has_saved_credentials) {
            ESP_LOGI(TAG, "Saved credentials found; scanning/retrying SSID '%s'", saved_ssid);
            connect_saved_wifi_loop(saved_ssid, saved_pass);
        } else {
            ESP_LOGI(TAG, "No saved Wi-Fi credentials; waiting for GUI pairing request");
            xEventGroupWaitBits(
                s_wifi_events,
                START_PAIRING_BIT,
                pdTRUE, pdFALSE,
                portMAX_DELAY
            );
        }

        run_pairing_session(saved_ssid, saved_pass, sizeof(saved_ssid));
        has_saved_credentials = saved_ssid[0] != '\0';
    }
}

// ── Firestore session push ──────────────────────────────────────────────────

static void escape_json_string(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; i++) {
        if (src[i] == '\n') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = 'n';
            } else {
                break;
            }
        } else if (src[i] == '\r') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = 'r';
            } else {
                break;
            }
        } else if (src[i] == '"') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = '"';
            } else {
                break;
            }
        } else if (src[i] == '\\') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = '\\';
            } else {
                break;
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

void generate_session_id(char *buf, size_t len)
{
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();

    // UUID version 4: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    r2 = (r2 & 0x0FFFFFFFUL) | 0x40000000UL;
    r3 = (r3 & 0x3FFFFFFFUL) | 0x80000000UL;

    snprintf(buf, len,
             "%08" PRIx32 "-%04x-%04x-%04x-%012llx",
             r1,
             (uint16_t)(r2 >> 16),
             (uint16_t)r2,
             (uint16_t)(r3 >> 16),
             ((unsigned long long)(r3 & 0xFFFFU) << 32) | r4);
}

typedef struct {
    char session_id[37];
    char start_date[16];
    char start_time[16];
    int num_rounds;
    char *rounds_str; // Allocated in PSRAM, will be freed in this task
    uint32_t total_work_sec;
    uint32_t avg_work_sec;
    uint32_t total_rest_sec;
    uint32_t avg_rest_sec;
} session_push_data_t;

static void session_push_task(void *pvParameters) {
    session_push_data_t *data = (session_push_data_t *)pvParameters;
    if (data == NULL) {
        vTaskDelete(NULL);
        return;
    }

    size_t rounds_len = strlen(data->rounds_str);
    size_t escaped_size = (rounds_len * 2) + 1;
    char *escaped_rounds = heap_caps_malloc(escaped_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    size_t body_size = escaped_size + 512;
    char *body = heap_caps_malloc(body_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (escaped_rounds == NULL || body == NULL) {
        ESP_LOGE(TAG, "Failed to allocate temporary buffers in PSRAM for session push");
        if (escaped_rounds) heap_caps_free(escaped_rounds);
        if (body) heap_caps_free(body);
        if (data->rounds_str) heap_caps_free(data->rounds_str);
        heap_caps_free(data);
        vTaskDelete(NULL);
        return;
    }

    escape_json_string(data->rounds_str, escaped_rounds, escaped_size);

    snprintf(body, body_size,
        "{"
          "\"fields\":{"
            "\"sesstionStartDate\":{\"stringValue\":\"%s\"},"
            "\"sessionStartTime\":{\"stringValue\":\"%s\"},"
            "\"numberofRounds\":{\"integerValue\":\"%d\"},"
            "\"rounds\":{\"stringValue\":\"%s\"},"
            "\"totalWorkSec\":{\"integerValue\":\"%" PRIu32 "\"},"
            "\"avgWorkSec\":{\"integerValue\":\"%" PRIu32 "\"},"
            "\"totalRestSec\":{\"integerValue\":\"%" PRIu32 "\"},"
            "\"avgRestSec\":{\"integerValue\":\"%" PRIu32 "\"}"
          "}"
        "}",
        data->start_date,
        data->start_time,
        data->num_rounds,
        escaped_rounds,
        data->total_work_sec,
        data->avg_work_sec,
        data->total_rest_sec,
        data->avg_rest_sec
    );

    char device_id[18];
    get_device_id(device_id, sizeof(device_id));

    char url[512];
    snprintf(url, sizeof(url),
        "https://firestore.googleapis.com/v1/projects/%s"
        "/databases/(default)/documents/devices/%s/sessions?documentId=%s",
        FIRESTORE_PROJECT_ID, device_id, data->session_id);

    ESP_LOGI(TAG, "Pushing session to Firestore (async task) URL: %s", url);

    esp_http_client_config_t config = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client != NULL) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, strlen(body));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Firestore session push ✓ HTTP=%d",
                     esp_http_client_get_status_code(client));
        } else {
            ESP_LOGE(TAG, "Firestore session push ✗ %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
    } else {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
    }

    heap_caps_free(escaped_rounds);
    heap_caps_free(body);
    if (data->rounds_str) heap_caps_free(data->rounds_str);
    heap_caps_free(data);

    vTaskDelete(NULL);
}

void connections_push_session(const char *session_id, const char *start_date, const char *start_time, int num_rounds, char *rounds_str,
                              uint32_t total_work_sec, uint32_t avg_work_sec, uint32_t total_rest_sec, uint32_t avg_rest_sec) {
    if (!connections_is_network_ready()) {
        ESP_LOGW(TAG, "Wi-Fi not connected. Discarding session data.");
        if (rounds_str) {
            heap_caps_free(rounds_str);
        }
        return;
    }

    session_push_data_t *data = heap_caps_malloc(sizeof(session_push_data_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate session_push_data_t in PSRAM");
        if (rounds_str) {
            heap_caps_free(rounds_str);
        }
        return;
    }

    strncpy(data->session_id, session_id, sizeof(data->session_id) - 1);
    data->session_id[sizeof(data->session_id) - 1] = '\0';

    strncpy(data->start_date, start_date, sizeof(data->start_date) - 1);
    data->start_date[sizeof(data->start_date) - 1] = '\0';

    strncpy(data->start_time, start_time, sizeof(data->start_time) - 1);
    data->start_time[sizeof(data->start_time) - 1] = '\0';

    data->num_rounds = num_rounds;
    data->rounds_str = rounds_str;
    data->total_work_sec = total_work_sec;
    data->avg_work_sec = avg_work_sec;
    data->total_rest_sec = total_rest_sec;
    data->avg_rest_sec = avg_rest_sec;

    BaseType_t ret = xTaskCreate(session_push_task, "session_push_task", 8192, data, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create session_push_task");
        if (rounds_str) heap_caps_free(rounds_str);
        heap_caps_free(data);
    }
}
