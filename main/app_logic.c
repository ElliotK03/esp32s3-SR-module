#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "app_logic.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "ui.h"
#include "images.h"
#include "chime.h"
#include "settings_manager.h"
#include "connections.h"
#include "esp_heap_caps.h"
#include "misc/lv_async.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Staging variable in DRAM so the NVS write task can safely read it
// even if the LVGL task (PSRAM stack) that set it has moved on.
static DRAM_ATTR uint32_t s_pending_period_sec = 0;

static void save_period_to_nvs_task(void *arg) {
    settings_manager_set_pomodoro_work(s_pending_period_sec);
    vTaskDelete(NULL);
}

static void persist_period(uint32_t secs) {
    s_pending_period_sec = secs;
    xTaskCreate(save_period_to_nvs_task, "save_period_nvs", 3000, NULL, 5, NULL);
}

static void (*g_persist_brightness_cb)(int32_t) = NULL;
static void (*g_lock_cb)(void) = NULL;
static void (*g_unlock_cb)(void) = NULL;
static void (*g_reset_cb)(void) = NULL;
static void (*g_start_pairing_cb)(void) = NULL;
static void (*g_volume_release)(int32_t) = NULL;
static void (*g_brightness_release)(int32_t) = NULL;
static void (*g_locker_box_connected_cb)(void) = NULL;
static void (*g_locker_box_disconnected_cb)(void) = NULL;

void app_logic_set_work_duration(uint32_t secs);
static void locker_box_switch_event_cb(lv_event_t *e);
static void write_nvs_manual_override_task(void *arg);

// DRAM: these must not be in PSRAM so NVS tasks can safely read them
DRAM_ATTR static bool locker_box_nvs_val;
DRAM_ATTR static bool manual_override_nvs_val;

static const char *TAG = "APP_LOGIC";

// ============= Timer State =============
typedef enum {
    POMO_STATE_IDLE,
    POMO_STATE_WORKING,
    POMO_STATE_RESTING
} pomo_mode_t;

typedef struct {
    uint32_t duration_sec;       // Total duration in seconds
    uint32_t remaining_sec;      // Seconds remaining
    bool running;                // Is timer currently running?
    bool paused;                 // Is timer currently paused?
    TimerHandle_t timer_handle;  // FreeRTOS timer handle
    pomo_mode_t mode;            // Current mode (idle, working, resting)
    bool has_added_5_min;        // Track if user already added 5 min in this resting session
} pomodoro_state_t;

static pomodoro_state_t pomodoro = {
    .duration_sec = 0,
    .remaining_sec = 0,
    .running = false,
    .paused = false,
    .timer_handle = NULL,
    .mode = POMO_STATE_IDLE,
    .has_added_5_min = false
};

// Forward declarations
static void timer_callback(TimerHandle_t xTimer);
void stop_timer();
static void start_resting_timer(uint32_t duration_seconds);
static void pomo_worker_task(void *arg);
static void update_pomo_period_display(void);
static void update_arc_display(void);
static void launch_work_session(uint32_t duration_sec);

// Queue and task variables
static QueueHandle_t pomo_worker_queue = NULL;

// ── Locker box sequence ───────────────────────────────────────────────────
#define LOCKER_INSERT_WAIT_MS   5000   // ms to wait for user to insert phone
#define LOCKER_PROMPT_FLASH_MS  400    // ms per flash toggle during wait

// ============= Variable Storage =============
static int32_t timer_arc_value = 0;
static char display_tim_str[16] = "25:00";
static char start_end_str[16] = "Start";
static char wifi_status_str[50] = "Not connected";
static char session_start_stop_button_str[32] = "Start focus";
static char tim_user_text_str[32] = "Select focus period";

// Session tracking state
typedef struct {
    uint32_t work_duration;
    uint32_t break_duration;
} round_info_t;

#define MAX_ROUNDS 100
static round_info_t *session_rounds = NULL;
static bool session_active = false;
static char session_id[37] = "";
static char session_start_date[16] = "";
static char session_start_time[16] = "";
static int session_rounds_count = 0;

// Pomodoro period (duration to set), in seconds
static uint32_t pomo_tim_period_sec = 25 * 60;  // default 25 minutes

static bool start_pomo_container_enable_val = false;
static bool pomo_running_container_enable_val = true;
static bool plus_5_button_disabled_val = false;
static bool pomo_resting_container_enable_val = true;
static bool start_pomo_again_container_enable_val = true;
static char curr_streak_str[32] = "Streak: 0";
static uint32_t streak_count = 0;

// Getter/Setter for timer_arc_value (called by generated UI)
int32_t get_var_timer_arc_value() {
    return timer_arc_value;
}

void set_var_timer_arc_value(int32_t value) {
    timer_arc_value = value;
}

const char *get_var_display_tim_str() {
    return display_tim_str;
}

void set_var_display_tim_str(const char *value) {
    if (value == NULL) {
        display_tim_str[0] = '\0';
        return;
    }
    strncpy(display_tim_str, value, sizeof(display_tim_str) - 1);
    display_tim_str[sizeof(display_tim_str) - 1] = '\0';
}

const char *get_var_start_end_str() {
    return start_end_str;
}

void set_var_start_end_str(const char *value) {
    if (value == NULL) {
        start_end_str[0] = '\0';
        return;
    }
    strncpy(start_end_str, value, sizeof(start_end_str) - 1);
    start_end_str[sizeof(start_end_str) - 1] = '\0';
}

const char *get_var_wifi_status_str() {
    return wifi_status_str;
}

void set_var_wifi_status_str(const char *value) {
    if (value == NULL) {
        wifi_status_str[0] = '\0';
        return;
    }
    strncpy(wifi_status_str, value, sizeof(wifi_status_str) - 1);
    wifi_status_str[sizeof(wifi_status_str) - 1] = '\0';
}

const char *get_var_session_start_stop_button_str() {
    return session_start_stop_button_str;
}

void set_var_session_start_stop_button_str(const char *value) {
    if (value == NULL) {
        session_start_stop_button_str[0] = '\0';
        return;
    }
    strncpy(session_start_stop_button_str, value, sizeof(session_start_stop_button_str) - 1);
    session_start_stop_button_str[sizeof(session_start_stop_button_str) - 1] = '\0';
}

const char *get_var_tim_user_text_str() {
    return tim_user_text_str;
}

void set_var_tim_user_text_str(const char *value) {
    if (value == NULL) {
        tim_user_text_str[0] = '\0';
        return;
    }
    strncpy(tim_user_text_str, value, sizeof(tim_user_text_str) - 1);
    tim_user_text_str[sizeof(tim_user_text_str) - 1] = '\0';
}

const char *get_var_curr_streak_str() {
    return curr_streak_str;
}

void set_var_curr_streak_str(const char *value) {
    if (value == NULL) {
        curr_streak_str[0] = '\0';
        return;
    }
    strncpy(curr_streak_str, value, sizeof(curr_streak_str) - 1);
    curr_streak_str[sizeof(curr_streak_str) - 1] = '\0';
}

bool get_var_start_pomo_container_enable() {
    return start_pomo_container_enable_val;
}

void set_var_start_pomo_container_enable(bool value) {
    start_pomo_container_enable_val = value;
}

bool get_var_pomo_running_container_enable() {
    return pomo_running_container_enable_val;
}

void set_var_pomo_running_container_enable(bool value) {
    pomo_running_container_enable_val = value;
}

bool get_var_plus_5_button_disabled() {
    return plus_5_button_disabled_val;
}

void set_var_plus_5_button_disabled(bool value) {
    plus_5_button_disabled_val = value;
}

bool get_var_pomo_resting_container_enable() {
    return pomo_resting_container_enable_val;
}

void set_var_pomo_resting_container_enable(bool value) {
    pomo_resting_container_enable_val = value;
}

bool get_var_start_pomo_again_container_enable() {
    return start_pomo_again_container_enable_val;
}

void set_var_start_pomo_again_container_enable(bool value) {
    start_pomo_again_container_enable_val = value;
}

bool get_var_lock_manual_override_enabled() {
    return settings_manager_get()->lock_manual_override;
}

void set_var_lock_manual_override_enabled(bool value) {
    if (settings_manager_get()->lock_manual_override == value) return;
    manual_override_nvs_val = value;
    xTaskCreate(write_nvs_manual_override_task, "nvs_override", 3000, NULL, 5, NULL);
}

void app_play_chime(pomo_worker_event_t chime_ev) {
    if (pomo_worker_queue != NULL) {
        xQueueSend(pomo_worker_queue, &chime_ev, 0);
    }
}

static void update_pomo_period_display() {
    uint32_t mins = pomo_tim_period_sec / 60;
    uint32_t secs = pomo_tim_period_sec % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)mins, (unsigned)secs);
    set_var_display_tim_str(buf);
}

// ============= User Text Update Helper =============
static void update_tim_user_text() {
    if (!pomodoro.running || pomodoro.duration_sec == 0) {
        set_var_tim_user_text_str("Select focus period");
        return;
    }
    
    if (pomodoro.mode == POMO_STATE_RESTING) {
        double fraction = (double)pomodoro.remaining_sec / pomodoro.duration_sec;
        if (fraction > 0.66) {
            set_var_tim_user_text_str("Take a break");
        } else if (fraction > 0.33) {
            set_var_tim_user_text_str("Relax your mind");
        } else {
            set_var_tim_user_text_str("Nearly done resting");
        }
        return;
    }
    
    double fraction = (double)pomodoro.remaining_sec / pomodoro.duration_sec;
    if (fraction > 0.66) {
        set_var_tim_user_text_str("Deep work");
    } else if (fraction > 0.33) {
        set_var_tim_user_text_str("Concentrate");
    } else {
        set_var_tim_user_text_str("Keep going");
    }
}

// ============= Arc Update Helper =============
static void update_arc_display() {
    if (pomodoro.duration_sec == 0) {
        set_var_timer_arc_value(0);
        set_var_display_tim_str("00:00");
        return;
    }
    // Calculate arc value: 0-360 based on remaining time
    int32_t arc_val = (pomodoro.remaining_sec * 360) / pomodoro.duration_sec;
    if (arc_val < 0) arc_val = 0;
    if (arc_val > 360) arc_val = 360;
    set_var_timer_arc_value(arc_val);
    
    // Format remaining time as MM:SS and update display text
    uint32_t mins = pomodoro.remaining_sec / 60;
    uint32_t secs = pomodoro.remaining_sec % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)mins, (unsigned)secs);
    set_var_display_tim_str(buf);
    
    update_tim_user_text();
}

// ── Flash animation (runs on LVGL task via lv_async_call / lv_timer) ─────────
typedef struct {
    lv_timer_t *timer;
    int remaining;
    void (*done_cb)(void);
} flash_ctx_t;

static flash_ctx_t s_flash_ctx;

static void flash_timer_cb(lv_timer_t *t)
{
    flash_ctx_t *ctx = (flash_ctx_t *)lv_timer_get_user_data(t);
    if (ctx->remaining <= 0) {
        lv_timer_delete(t);
        ctx->timer = NULL;
        if (objects.time_text != NULL)
            lv_obj_remove_flag(objects.time_text, LV_OBJ_FLAG_HIDDEN);
        if (ctx->done_cb)
            ctx->done_cb();
        return;
    }
    ctx->remaining--;
    if (objects.time_text != NULL) {
        if (lv_obj_has_flag(objects.time_text, LV_OBJ_FLAG_HIDDEN))
            lv_obj_remove_flag(objects.time_text, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.time_text, LV_OBJ_FLAG_HIDDEN);
    }
}

static void start_flash_then(int toggles, void (*done_cb)(void))
{
    if (s_flash_ctx.timer != NULL) {
        lv_timer_delete(s_flash_ctx.timer);
        s_flash_ctx.timer = NULL;
    }
    s_flash_ctx.remaining = toggles;
    s_flash_ctx.done_cb   = done_cb;
    s_flash_ctx.timer     = lv_timer_create(flash_timer_cb, 300, &s_flash_ctx);
}

static void work_done_post_flash(void)
{
    streak_count++;
    snprintf(curr_streak_str, sizeof(curr_streak_str), "Streak: %"PRIu32, streak_count);

    if (settings_manager_get()->locker_box_connected) {
        if (g_unlock_cb) g_unlock_cb();
        set_var_tim_user_text_str("Take your phone!");
    }

    start_resting_timer(5 * 60);
}

static void rest_done_post_flash(void)
{
    pomodoro.paused          = false;
    pomodoro.mode            = POMO_STATE_IDLE;
    pomodoro.has_added_5_min = false;

    if (session_rounds_count < MAX_ROUNDS)
        session_rounds_count++;

    if (objects.obj0 != NULL) {
        lv_obj_remove_local_style_prop(objects.obj0, LV_STYLE_ARC_COLOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_invalidate(objects.obj0);
    }

    set_var_timer_arc_value(0);
    set_var_session_start_stop_button_str("Start focus");
    set_var_tim_user_text_str("Start another focus?");
    set_var_start_end_str("Start");
    update_pomo_period_display();

    start_pomo_container_enable_val       = true;
    pomo_running_container_enable_val     = true;
    pomo_resting_container_enable_val     = true;
    start_pomo_again_container_enable_val = false;

    if (objects.icon_start_resume != NULL)
        lv_image_set_src(objects.icon_start_resume, &img_play_arrow_bitmap);

    if (objects.pomo_start_end_button != NULL) {
        lv_obj_remove_local_style_prop(objects.pomo_start_end_button, LV_STYLE_BG_COLOR, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_invalidate(objects.pomo_start_end_button);
        lv_obj_t *label = lv_obj_get_child(objects.pomo_start_end_button, 0);
        if (label != NULL)
            lv_label_set_text(label, "Start focus");
    }
}

static void async_start_work_done_flash(void *arg)
{
    (void)arg;
    start_flash_then(10, work_done_post_flash);
}

static void async_start_rest_done_flash(void *arg)
{
    (void)arg;
    start_flash_then(10, rest_done_post_flash);
}

// ============= Unified pomo Worker Task & Helpers =============
static void pomo_worker_task(void *arg) {
    pomo_worker_event_t ev;
    
    while (1) {
        if (xQueueReceive(pomo_worker_queue, &ev, portMAX_DELAY) == pdTRUE) {
            switch (ev) {
                case POMO_EV_PLAY_CHIME_WAKE:
                    chime_play_wake();
                    break;
                case POMO_EV_PLAY_CHIME_ACK:
                    chime_play_ack();
                    break;
                case POMO_EV_TICK:
                    if (pomodoro.running && !pomodoro.paused) {
                        if (pomodoro.remaining_sec > 0) {
                            pomodoro.remaining_sec--;
                            update_arc_display();
                            if (pomodoro.remaining_sec % 60 == 0) {
                                ESP_LOGI(TAG, "Timer remaining: %"PRIu32"s", pomodoro.remaining_sec);
                            }
                        } else {
                            if (pomodoro.mode == POMO_STATE_WORKING) {
                                ESP_LOGI(TAG, "Work session finished!");
                                xTimerStop(pomodoro.timer_handle, 0);
                                pomodoro.running = false;
                                chime_play_work_done();
                                // Hand off flash + post-flash transitions to the LVGL task
                                lv_async_call(async_start_work_done_flash, NULL);

                            } else if (pomodoro.mode == POMO_STATE_RESTING) {
                                ESP_LOGI(TAG, "Resting session finished!");
                                xTimerStop(pomodoro.timer_handle, 0);
                                pomodoro.running = false;
                                chime_play_rest_done();
                                // Hand off flash + post-flash transitions to the LVGL task
                                lv_async_call(async_start_rest_done_flash, NULL);
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

// ============= Start Resting Timer =============
static void start_resting_timer(uint32_t duration_seconds) {
    if (session_rounds != NULL && session_rounds_count < MAX_ROUNDS) {
        session_rounds[session_rounds_count].break_duration = duration_seconds;
    }

    pomodoro.duration_sec = duration_seconds;
    pomodoro.remaining_sec = duration_seconds;
    pomodoro.running = true;
    pomodoro.paused = false;
    pomodoro.mode = POMO_STATE_RESTING;
    pomodoro.has_added_5_min = false;
    
    // Enable the +5 minute button
    plus_5_button_disabled_val = false;

    // Change arc indicator color to #7b68ee
    if (objects.obj0 != NULL) {
        lv_obj_set_style_arc_color(objects.obj0, lv_color_hex(0x7b68ee), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_invalidate(objects.obj0);
    }

    // Toggle container visibilities: hide start, hide running, show resting, hide pomo again
    start_pomo_container_enable_val = true;   // True = hidden
    pomo_running_container_enable_val = true; // True = hidden
    pomo_resting_container_enable_val = false; // False = shown
    start_pomo_again_container_enable_val = true; // True = hidden

    // Make sure FreeRTOS timer is running
    if (pomodoro.timer_handle == NULL) {
        pomodoro.timer_handle = xTimerCreate(
            "pomodoro_timer",
            pdMS_TO_TICKS(1000),  // 1 second callback interval
            pdTRUE,               // auto-reload
            NULL,                 // timer ID
            timer_callback        // callback function
        );
    }
    
    if (pomodoro.timer_handle != NULL) {
        xTimerStart(pomodoro.timer_handle, 0);
        update_arc_display();
        update_tim_user_text();
        ESP_LOGI(TAG, "Resting timer started: %"PRIu32" seconds", duration_seconds);
    } else {
        ESP_LOGE(TAG, "Failed to create FreeRTOS timer for resting");
        pomodoro.running = false;
    }
}

// ============= FreeRTOS Timer Callback =============
static void timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    if (pomo_worker_queue != NULL) {
        pomo_worker_event_t ev = POMO_EV_TICK;
        xQueueSend(pomo_worker_queue, &ev, 0);
    }
}

// ============= Locker box sequence =============

static void async_set_prompt_insert(void *arg) {
    set_var_tim_user_text_str("Insert phone");
}

static void async_restore_prompt(void *arg) {
    update_tim_user_text();
}

// Flashes the user text label: called from a separate task via lv_async_call
typedef struct { bool visible; } flash_toggle_arg_t;
static DRAM_ATTR flash_toggle_arg_t s_flash_toggle;

static void async_flash_toggle(void *arg) {
    flash_toggle_arg_t *a = (flash_toggle_arg_t *)arg;
    if (objects.time_text != NULL) {
        if (a->visible)
            lv_obj_remove_flag(objects.time_text, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.time_text, LV_OBJ_FLAG_HIDDEN);
    }
}

static void async_start_timer_deferred(void *arg) {
    uint32_t dur = (uint32_t)(uintptr_t)arg;
    start_timer(dur);
}

// Runs on its own FreeRTOS task so the LVGL task is never blocked
static void locker_box_sequence_task(void *arg) {
    uint32_t duration_sec = (uint32_t)(uintptr_t)arg;

    // 1. Unlock the box so the user can insert their phone
    if (g_unlock_cb) g_unlock_cb();

    // 2. Prompt the user
    lv_async_call(async_set_prompt_insert, NULL);

    // 3. Flash the prompt for LOCKER_INSERT_WAIT_MS
    int toggles = LOCKER_INSERT_WAIT_MS / LOCKER_PROMPT_FLASH_MS;
    for (int i = 0; i < toggles; i++) {
        s_flash_toggle.visible = (i % 2 == 0);
        lv_async_call(async_flash_toggle, &s_flash_toggle);
        vTaskDelay(pdMS_TO_TICKS(LOCKER_PROMPT_FLASH_MS));
    }
    // Ensure label is visible again
    s_flash_toggle.visible = true;
    lv_async_call(async_flash_toggle, &s_flash_toggle);

    // 4. Lock the box
    if (g_lock_cb) g_lock_cb();

    // 5. Start the actual timer (must run on LVGL task)
    lv_async_call(async_start_timer_deferred, (void *)(uintptr_t)duration_sec);

    vTaskDelete(NULL);
}

// ============= Public API =============

/**
 * Start a pomodoro timer with the given duration (in seconds)
 * For testing: 10s button = 10 real seconds
 */
void start_timer(uint32_t duration_seconds) {
    if (pomodoro.running) {
        ESP_LOGW(TAG, "Timer already running, stopping it first");
        stop_timer();
    }

    if (!session_active) {
        session_active = true;
        session_rounds_count = 0;
        memset(session_id, 0, sizeof(session_id));
        generate_session_id(session_id, sizeof(session_id));

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        strftime(session_start_date, sizeof(session_start_date), "%d/%m/%Y", &timeinfo);
        strftime(session_start_time, sizeof(session_start_time), "%H:%M", &timeinfo);
        
        ESP_LOGI(TAG, "New session started: ID=%s, Date=%s, Time=%s",
                 session_id, session_start_date, session_start_time);
    }

    if (session_rounds != NULL && session_rounds_count < MAX_ROUNDS) {
        session_rounds[session_rounds_count].work_duration = duration_seconds;
        session_rounds[session_rounds_count].break_duration = 0;
    }
    
    pomodoro.duration_sec = duration_seconds;
    pomodoro.remaining_sec = duration_seconds;
    pomodoro.running = true;
    pomodoro.paused = false;
    pomodoro.mode = POMO_STATE_WORKING;
    pomodoro.has_added_5_min = false;

    // Persist the confirmed period so it survives reboot.
    // Skip the 5-second debug value — only save real minutes.
    if (duration_seconds >= 60) {
        persist_period(duration_seconds);
    }

    // Restore arc indicator color to default
    if (objects.obj0 != NULL) {
        lv_obj_remove_local_style_prop(objects.obj0, LV_STYLE_ARC_COLOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_invalidate(objects.obj0);
    }

    // Toggle container visibilities: hide start, show running, hide resting, hide pomo again
    start_pomo_container_enable_val = true;     // True = hidden
    pomo_running_container_enable_val = false;   // False = shown
    pomo_resting_container_enable_val = true;   // True = hidden
    start_pomo_again_container_enable_val = true; // True = hidden

    // Set icon_start_resume to pause bitmap since it's running
    if (objects.icon_start_resume != NULL) {
        lv_image_set_src(objects.icon_start_resume, &img_pause_bitmap);
    }
    
    // Create a FreeRTOS software timer if not already created
    if (pomodoro.timer_handle == NULL) {
        pomodoro.timer_handle = xTimerCreate(
            "pomodoro_timer",
            pdMS_TO_TICKS(1000),  // 1 second callback interval
            pdTRUE,               // auto-reload
            NULL,                 // timer ID
            timer_callback        // callback function
        );
    }
    
    // Start the timer
    if (pomodoro.timer_handle != NULL) {
        xTimerStart(pomodoro.timer_handle, 0);
        update_arc_display();
        update_tim_user_text();
        set_var_session_start_stop_button_str("Stop");
        set_var_start_end_str("Stop");
        
        // Set button color to red and text to Stop
        if (objects.pomo_start_end_button != NULL) {
            lv_obj_set_style_bg_color(objects.pomo_start_end_button, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_t *label = lv_obj_get_child(objects.pomo_start_end_button, 0);
            if (label != NULL) {
                lv_label_set_text(label, "Stop");
            }
        }
        
        ESP_LOGI(TAG, "Timer started: %"PRIu32" seconds", duration_seconds);
    } else {
        ESP_LOGE(TAG, "Failed to create FreeRTOS timer");
        pomodoro.running = false;
    }
}

/**
 * Stop the running timer
 */
void stop_timer() {
    if (pomodoro.timer_handle != NULL) {
        xTimerStop(pomodoro.timer_handle, 0);
    }
    pomodoro.running = false;
    pomodoro.paused = false;
    pomodoro.remaining_sec = 0;
    pomodoro.duration_sec = 0;
    pomodoro.mode = POMO_STATE_IDLE;
    pomodoro.has_added_5_min = false;

    // Restore arc indicator color to default
    if (objects.obj0 != NULL) {
        lv_obj_remove_local_style_prop(objects.obj0, LV_STYLE_ARC_COLOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_invalidate(objects.obj0);
    }

    set_var_timer_arc_value(0);
    set_var_session_start_stop_button_str("Start focus");
    set_var_tim_user_text_str("Select focus period");
    set_var_start_end_str("Start");
    update_pomo_period_display();

    // Toggle container visibilities: show start, hide running, hide resting, hide pomo again
    start_pomo_container_enable_val = false;   // False = shown
    pomo_running_container_enable_val = true;  // True = hidden
    pomo_resting_container_enable_val = true;  // True = hidden
    start_pomo_again_container_enable_val = true; // True = hidden

    // Set icon_start_resume back to play arrow
    if (objects.icon_start_resume != NULL) {
        lv_image_set_src(objects.icon_start_resume, &img_play_arrow_bitmap);
    }
    
    // Restore button color and text
    if (objects.pomo_start_end_button != NULL) {
        lv_obj_remove_local_style_prop(objects.pomo_start_end_button, LV_STYLE_BG_COLOR, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_invalidate(objects.pomo_start_end_button);
        lv_obj_t *label = lv_obj_get_child(objects.pomo_start_end_button, 0);
        if (label != NULL) {
            lv_label_set_text(label, "Start focus");
        }
    }
    
    ESP_LOGI(TAG, "Timer stopped");
}

/**
 * Pause the timer
 */
void pause_timer() {
    if (pomodoro.running && !pomodoro.paused) {
        if (pomodoro.timer_handle != NULL) {
            xTimerStop(pomodoro.timer_handle, 0);
        }
        pomodoro.paused = true;
        if (objects.icon_start_resume != NULL) {
            lv_image_set_src(objects.icon_start_resume, &img_play_arrow_bitmap);
        }
        ESP_LOGI(TAG, "Timer paused midway");
    }
}

/**
 * Resume the timer
 */
void resume_timer() {
    if (pomodoro.running && pomodoro.paused) {
        pomodoro.paused = false;
        if (objects.icon_start_resume != NULL) {
            lv_image_set_src(objects.icon_start_resume, &img_pause_bitmap);
        }
        if (pomodoro.timer_handle != NULL) {
            xTimerStart(pomodoro.timer_handle, 0);
        }
        ESP_LOGI(TAG, "Timer resumed midway");
    }
}

/**
 * Get current timer status
 */
bool is_timer_running() {
    return pomodoro.running;
}

bool is_timer_paused() {
    return pomodoro.paused;
}

bool is_timer_working() {
    return pomodoro.running && pomodoro.mode == POMO_STATE_WORKING;
}

void app_logic_start_work_session(void) {
    launch_work_session(pomo_tim_period_sec);
}

uint32_t get_remaining_time() {
    return pomodoro.remaining_sec;
}

void app_logic_init() {
    // Allocate session rounds array in PSRAM (SPIRAM)
    if (session_rounds == NULL) {
        session_rounds = heap_caps_malloc(sizeof(round_info_t) * MAX_ROUNDS, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (session_rounds == NULL) {
            ESP_LOGW(TAG, "Failed to allocate session_rounds in PSRAM, falling back to internal RAM");
            session_rounds = malloc(sizeof(round_info_t) * MAX_ROUNDS);
        }
    }

    // Initialize worker task queue and task
    if (pomo_worker_queue == NULL) {
        pomo_worker_queue = xQueueCreate(10, sizeof(pomo_worker_event_t));
        xTaskCreatePinnedToCore(pomo_worker_task, "pomo_worker_task", 4096, NULL, 5, NULL, 1);
    }
    
    // Initialize container visibilities: show start container, hide running/resting/pomo_again containers
    start_pomo_container_enable_val = false; // False = shown
    pomo_running_container_enable_val = true; // True = hidden
    pomo_resting_container_enable_val = true; // True = hidden
    start_pomo_again_container_enable_val = true; // True = hidden
    plus_5_button_disabled_val = false;
    
    pomodoro.mode = POMO_STATE_IDLE;
    pomodoro.has_added_5_min = false;
    
    // Clear the streak
    streak_count = 0;
    snprintf(curr_streak_str, sizeof(curr_streak_str), "Streak: 0");

    // Initialize the period display on startup
    update_pomo_period_display();
    set_var_session_start_stop_button_str("Start focus");
    set_var_tim_user_text_str("Select focus period");

    // Restore locker box switch state and attach its event handler
    if (objects.phone_locker_box_enabled != NULL) {
        bool connected = settings_manager_get()->locker_box_connected;
        if (connected) {
            lv_obj_add_state(objects.phone_locker_box_enabled, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(objects.phone_locker_box_enabled, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(objects.phone_locker_box_enabled,
                            locker_box_switch_event_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
    }
}

// ============= EEZ Studio Action Handlers =============

/**
 * Increment the pomo period by 5 minutes (min 5 min, max 60 min)
 */
void action_button_plus_pressed(lv_event_t * e) {
    (void)e;  // unused
    
    if (pomodoro.running) {
        ESP_LOGW(TAG, "Cannot change period while timer is running");
        return;
    }
    
    if (pomo_tim_period_sec == 5) {
        pomo_tim_period_sec = 5 * 60;
        update_pomo_period_display();
        ESP_LOGI(TAG, "Pomo period increased to %"PRIu32" minutes", pomo_tim_period_sec / 60);
    } else if (pomo_tim_period_sec + 5 * 60 <= 60 * 60) {
        pomo_tim_period_sec += 5 * 60;
        update_pomo_period_display();
        ESP_LOGI(TAG, "Pomo period increased to %"PRIu32" minutes", pomo_tim_period_sec / 60);
    }
}

/**
 * Decrement the pomo period by 5 minutes (min 5 min, max 60 min)
 * Supports 5 seconds for testing when minus pressed at 5 minutes
 */
void action_button_minus_pressed(lv_event_t * e) {
    (void)e;  // unused
    
    if (pomodoro.running) {
        ESP_LOGW(TAG, "Cannot change period while timer is running");
        return;
    }
    
    if (pomo_tim_period_sec > 5 * 60) {
        pomo_tim_period_sec -= 5 * 60;
        update_pomo_period_display();
        ESP_LOGI(TAG, "Pomo period decreased to %"PRIu32" minutes", pomo_tim_period_sec / 60);
    } else if (pomo_tim_period_sec == 5 * 60) {
        pomo_tim_period_sec = 5;  // 5 seconds for testing
        update_pomo_period_display();
        ESP_LOGI(TAG, "Pomo period decreased to 5 seconds for testing");
    }
}

/**
 * Start/stop the pomodoro timer with the selected period
 */
static void launch_work_session(uint32_t duration_sec) {
    if (settings_manager_get()->locker_box_connected) {
        ESP_LOGI(TAG, "Locker box enabled — running insert sequence before starting timer");
        xTaskCreate(locker_box_sequence_task, "locker_seq", 4096,
                    (void *)(uintptr_t)duration_sec, 5, NULL);
    } else {
        start_timer(duration_sec);
    }
}

void action_button_start_pomo_pressed(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "Start focus button pressed; duration=%"PRIu32" seconds", pomo_tim_period_sec);
    launch_work_session(pomo_tim_period_sec);
}

/**
 * End the pomodoro session
 */
void action_button_end_session_pressed(lv_event_t * e) {
    (void)e;  // unused
    ESP_LOGI(TAG, "End session button pressed");

    if (session_active) {
        // If we are currently in resting mode, the work duration has finished.
        // We include this round in the push.
        if (pomodoro.running && pomodoro.mode == POMO_STATE_RESTING) {
            if (session_rounds_count < MAX_ROUNDS) {
                session_rounds_count++;
            }
        }

        // Only format and push if at least one full work session has completed!
        if (session_rounds_count > 0) {
            // Compute totals and averages
            uint32_t total_work_sec = 0, total_rest_sec = 0;
            for (int i = 0; i < session_rounds_count; i++) {
                total_work_sec += session_rounds[i].work_duration;
                total_rest_sec += session_rounds[i].break_duration;
            }
            uint32_t avg_work_sec = total_work_sec / (uint32_t)session_rounds_count;
            uint32_t avg_rest_sec = total_rest_sec / (uint32_t)session_rounds_count;

            // Format the rounds in PSRAM
            size_t rounds_buf_size = 4096;
            char *rounds_str = heap_caps_malloc(rounds_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (rounds_str != NULL) {
                size_t offset = 0;
                int len = snprintf(rounds_str + offset, rounds_buf_size - offset, " \n");
                if (len > 0) offset += len;

                for (int i = 0; i < session_rounds_count; i++){
                    len = snprintf(rounds_str + offset,
                                   rounds_buf_size - offset,
                                   "        %d: {%" PRIu32 ", %" PRIu32 "}\n",
                                   i,
                                   session_rounds[i].work_duration,
                                   session_rounds[i].break_duration);
                    if (len > 0){
                        offset += len;
                    }
                }

                len = snprintf(rounds_str + offset, rounds_buf_size - offset, "    ");
                if (len > 0) offset += len;

                // connections_push_session takes ownership of rounds_str and frees it
                connections_push_session(session_id, session_start_date, session_start_time, session_rounds_count, rounds_str,
                                         total_work_sec, avg_work_sec, total_rest_sec, avg_rest_sec);
            } else {
                ESP_LOGE(TAG, "Failed to allocate rounds_str in PSRAM for session push");
            }
        } else {
            ESP_LOGI(TAG, "Not even one full work session completed. Discarding session.");
        }

        session_active = false;
    }

    streak_count = 0;
    snprintf(curr_streak_str, sizeof(curr_streak_str), "Streak: 0");

    if (settings_manager_get()->locker_box_connected && g_unlock_cb != NULL) {
        g_unlock_cb();
        ESP_LOGI(TAG, "Session terminated — unlocking locker box");
    }

    stop_timer();
}

/**
 * Start/Resume (Pause/Resume) button pressed
 */
void action_button_start_resume_pressed(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "Start/Resume (Pause/Resume) button pressed");
    if (!pomodoro.running) {
        launch_work_session(pomo_tim_period_sec);
        return;
    }
    if (pomodoro.paused) {
        resume_timer();
    } else {
        pause_timer();
    }
}

/**
 * Plus 5 minutes button pressed (resting mode only, allowed once)
 */
void action_plus_5_button_pressed(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "Plus 5 button pressed");
    if (pomodoro.running && pomodoro.mode == POMO_STATE_RESTING && !pomodoro.has_added_5_min) {
        pomodoro.remaining_sec += 5 * 60;
        pomodoro.duration_sec += 5 * 60;
        pomodoro.has_added_5_min = true;
        plus_5_button_disabled_val = true;
        update_arc_display();
        ESP_LOGI(TAG, "Added 5 minutes to resting timer. New remaining: %"PRIu32"s", pomodoro.remaining_sec);

        if (session_rounds != NULL && session_rounds_count < MAX_ROUNDS) {
            session_rounds[session_rounds_count].break_duration = pomodoro.duration_sec;
        }
    }
}

/**
 * Fast forward resting period button pressed
 */
void action_button_fast_forward_pressed(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "Fast forward resting period button pressed");
    if (pomodoro.running && pomodoro.mode == POMO_STATE_RESTING) {
        // Record only the time actually spent resting, not the full planned duration
        uint32_t elapsed = pomodoro.duration_sec - pomodoro.remaining_sec;
        if (session_rounds != NULL && session_rounds_count < MAX_ROUNDS) {
            session_rounds[session_rounds_count].break_duration = elapsed;
        }
        pomodoro.remaining_sec = 0;
        if (pomo_worker_queue != NULL) {
            pomo_worker_event_t ev = POMO_EV_TICK;
            xQueueSend(pomo_worker_queue, &ev, 0);
        }
    }
}

// ============= Clock/Time Variables for GUI =============
static char clock_str[32] = "";
static char clock_seconds_str[8] = "";
static char date_str[32] = "";
static char day_str[32] = "";

static void clock_fill_timeinfo(struct tm *out) {
    time_t now;
    time(&now);
    localtime_r(&now, out);
}

const char *get_var_clcok_str() {
    struct tm t;
    clock_fill_timeinfo(&t);
    strftime(clock_str, sizeof(clock_str), "%I:%M %p", &t);
    return clock_str;
}

const char *get_var_clock_seconds_str() {
    struct tm t;
    clock_fill_timeinfo(&t);
    strftime(clock_seconds_str, sizeof(clock_seconds_str), ":%S", &t);
    return clock_seconds_str;
}

void set_var_clcok_str(const char *value) {
    if (value != NULL) {
        strncpy(clock_str, value, sizeof(clock_str) - 1);
        clock_str[sizeof(clock_str) - 1] = '\0';
    }
}

const char *get_var_date_str() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo); 

    strftime(date_str, sizeof(date_str), "%d %b %Y", &timeinfo);
    return date_str;
}

void set_var_date_str(const char *value) {
    if (value != NULL) {
        strncpy(date_str, value, sizeof(date_str) - 1);
        date_str[sizeof(date_str) - 1] = '\0';
    }
}

const char *get_var_day_str() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // day of the week
    strftime(day_str, sizeof(day_str), "%A", &timeinfo);
    return day_str;
}

void set_var_day_str(const char *value) {
    if (value != NULL) {
        strncpy(day_str, value, sizeof(day_str) - 1);
        day_str[sizeof(day_str) - 1] = '\0';
    }
}

// ============= Screen Brightness Variable =============
static int32_t screen_brightness = 70; // Default to 70%

int32_t get_var_screen_brightness_val() {
    return screen_brightness;
}

void set_var_screen_brightness_val(int32_t value) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    
    screen_brightness = value;
    
    // Call the screen brightness callback registered in main.c
    g_persist_brightness_cb(value);
}

// ============= Volume Variable =============
static int32_t volume_value = 25; // Default to 25%

int32_t get_var_volume() {
    return volume_value;
}

void set_var_volume(int32_t value) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    volume_value = value;

    ESP_LOGI(TAG, "Volume updated to %d%%", (int)volume_value);
}

// ============= Timer Toggling for External Code =============
void toggle_pomo_timer() {
    if (pomodoro.running) {
        ESP_LOGI(TAG, "Toggling timer: stopping timer");
        stop_timer();
    } else {
        ESP_LOGI(TAG, "Toggling timer: starting timer with duration %u seconds", (unsigned int)pomo_tim_period_sec);
        start_timer(pomo_tim_period_sec);
    }
}

void app_logic_register_persist_brightness_cb(void (*cb)(int32_t)) {
    g_persist_brightness_cb = cb;
}

void app_logic_register_lock_cb(void (*cb)(void)) {
    g_lock_cb = cb;
}

void app_logic_register_unlock_cb(void (*cb)(void)) {
    g_unlock_cb = cb;
}

void app_logic_register_reset_cb(void (*cb)(void)) {
    g_reset_cb = cb;
}

void app_logic_register_start_pairing_cb(void (*cb)(void)) {
    g_start_pairing_cb = cb;
}

void app_logic_register_volume_released_cb(void (*cb)(int32_t)) {
    g_volume_release = cb;
}

void app_logic_register_brightness_released_cb(void (*cb)(int32_t)) {
    g_brightness_release = cb;
}

void app_logic_register_locker_box_connected_cb(void (*cb)(void)) {
    g_locker_box_connected_cb = cb;
}

void app_logic_register_locker_box_disconnected_cb(void (*cb)(void)) {
    g_locker_box_disconnected_cb = cb;
}

static void write_nvs_locker_box_task(void *arg) {
    settings_manager_set_locker_box_connected(locker_box_nvs_val);
    vTaskDelete(NULL);
}

static void write_nvs_manual_override_task(void *arg) {
    settings_manager_set_lock_manual_override(manual_override_nvs_val);
    vTaskDelete(NULL);
}

static void locker_box_switch_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t *sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    locker_box_nvs_val = is_on;
    xTaskCreate(write_nvs_locker_box_task, "nvs_locker_box", 3000, NULL, 5, NULL);

    ESP_LOGI(TAG, "Locker box switch → %s", is_on ? "connected" : "disconnected");

    if (is_on && g_locker_box_connected_cb != NULL) {
        g_locker_box_connected_cb();
    } else if (!is_on && g_locker_box_disconnected_cb != NULL) {
        g_locker_box_disconnected_cb();
    }
}

// ============= Lock/Unlock Button Actions =============
void action_button_lock_pressed(lv_event_t * e) {
    (void)e;
    if (settings_manager_get()->locker_box_connected && is_timer_working()) {
        ESP_LOGW(TAG, "Lock ignored — locker box is managed during study session");
        return;
    }
    ESP_LOGI(TAG, "Lock button pressed");
    if (g_lock_cb != NULL) {
        g_lock_cb();
    } else {
        ESP_LOGW(TAG, "Lock callback not registered!");
    }
}

void action_button_unlock_pressed(lv_event_t * e) {
    (void)e;
    if (settings_manager_get()->locker_box_connected && is_timer_working()) {
        ESP_LOGW(TAG, "Unlock ignored — locker box is managed during study session");
        return;
    }
    ESP_LOGI(TAG, "Unlock button pressed");
    if (g_unlock_cb != NULL) {
        g_unlock_cb();
    } else {
        ESP_LOGW(TAG, "Unlock callback not registered!");
    }
}

void app_logic_set_work_duration(uint32_t secs)
{
    /* Update the global period used when the user presses “Start”. */
    pomo_tim_period_sec = secs;
    update_pomo_period_display();   // refresh the UI text
}

uint32_t get_pomo_period()
{
    return pomo_tim_period_sec;
}

void action_button_reset_device_pressed(lv_event_t * e){
    (void)e;
    ESP_LOGI(TAG, "Reset button pressed");
    if (g_reset_cb != NULL) {
        g_reset_cb();
    } else {
        ESP_LOGW(TAG, "Reset callback not registered!");
    }
};

void action_button_start_pairing_pressed(lv_event_t * e){
    (void)e;
    ESP_LOGI(TAG, "Start pairing button pressed");
    if (g_start_pairing_cb != NULL) {
        g_start_pairing_cb();
    } else {
        ESP_LOGW(TAG, "Start pairing callback not registered!");
    }
}

void action_slider_volume_released(lv_event_t * e){
    ESP_LOGI(TAG, "Volume slider released, writing to NVS flash");
    g_volume_release(volume_value);
}

void action_slider_brightness_released(lv_event_t * e){
    g_brightness_release(screen_brightness);
}
