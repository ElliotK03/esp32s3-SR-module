#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include "misc/lv_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POMO_EV_TICK,
    POMO_EV_WORK_DONE,
    POMO_EV_REST_DONE,
    POMO_EV_PLAY_CHIME_WAKE,
    POMO_EV_PLAY_CHIME_ACK,
} pomo_worker_event_t;

void app_play_chime(pomo_worker_event_t chime_ev);

extern void app_logic_set_work_duration(uint32_t secs);
// ============= Public API =============

/**
 * Start a pomodoro timer with the given duration (in seconds)
 */
void start_timer(uint32_t duration_seconds);

/**
 * Stop the running timer
 */
void stop_timer();

/**
 * Get the current pomodoro period duration in seconds
 */
uint32_t get_pomo_period();

/**
 * Check if timer is currently running
 */
bool is_timer_running();

/**
 * Check if timer is currently paused
 */
bool is_timer_paused();

/**
 * Pause the timer
 */
void pause_timer();

/**
 * Resume the timer
 */
void resume_timer();

/**
 * Get the remaining time in seconds
 */
uint32_t get_remaining_time();

/**
 * Initialize app logic (call on startup)
 */
void app_logic_init();

// ============= UI Variable Getters/Setters =============

int32_t get_var_timer_arc_value();
void set_var_timer_arc_value(int32_t value);
const char *get_var_display_tim_str();
void set_var_display_tim_str(const char *value);
const char *get_var_start_end_str();
void set_var_start_end_str(const char *value);
int32_t get_var_screen_brightness_val();
void set_var_screen_brightness_val(int32_t value);
int32_t get_var_volume();
void set_var_volume(int32_t value);

const char *get_var_wifi_status_str();
void set_var_wifi_status_str(const char *value);

const char *get_var_session_start_stop_button_str();
void set_var_session_start_stop_button_str(const char *value);
const char *get_var_tim_user_text_str();
void set_var_tim_user_text_str(const char *value);

const char *get_var_curr_streak_str();
void set_var_curr_streak_str(const char *value);
bool get_var_start_pomo_container_enable();
void set_var_start_pomo_container_enable(bool value);
bool get_var_pomo_running_container_enable();
void set_var_pomo_running_container_enable(bool value);
bool get_var_plus_5_button_disabled();
void set_var_plus_5_button_disabled(bool value);
bool get_var_pomo_resting_container_enable();
void set_var_pomo_resting_container_enable(bool value);
bool get_var_start_pomo_again_container_enable();
void set_var_start_pomo_again_container_enable(bool value);

void app_logic_register_persist_brightness_cb(void (*cb)(int32_t));
void app_logic_register_volume_released_cb(void (*cb)(int32_t));
void app_logic_register_brightness_released_cb(void (*cb)(int32_t));

void app_logic_register_lock_cb(void (*cb)(void));
void app_logic_register_unlock_cb(void (*cb)(void));
void app_logic_register_reset_cb(void (*cb)(void));
void app_logic_register_start_pairing_cb(void (*cb)(void));

// Timer functions for external code
void toggle_pomo_timer();

// ============= EEZ Studio Action Handlers =============
// These are called by the generated UI

/**
 * Increment pomodoro period by 1 minute
 */
void action_button_plus_pressed(lv_event_t *e);

/**
 * Decrement pomodoro period by 1 minute
 */
void action_button_minus_pressed(lv_event_t *e);

/**
 * Start the pomodoro timer with selected period
 */
void action_button_start_pomo_pressed(lv_event_t *e);
void action_button_start_resume_pressed(lv_event_t *e);
void action_button_start_pairing_pressed(lv_event_t *e);
void action_plus_5_button_pressed(lv_event_t *e);
void action_button_fast_forward_pressed(lv_event_t *e);
void action_button_end_session_pressed(lv_event_t *e);


/**
 * Write to NVS flash when volume slider released
 */
void action_slider_volume_released(lv_event_t * e);

/**
 * Write to NVS flash when brightness slider released
 */
void action_slider_brightness_released(lv_event_t * e);


#ifdef __cplusplus
}
#endif

#endif // APP_LOGIC_H
