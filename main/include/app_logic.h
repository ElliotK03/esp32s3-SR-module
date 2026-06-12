#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include "misc/lv_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
int32_t get_var_screen_brightness();
void set_var_screen_brightness(int32_t value);
int32_t get_var_volume();
void set_var_volume(int32_t value);

void app_logic_register_persist_volume_cb(void (*cb)(int32_t));
void app_logic_persist_volume(int32_t val);
void app_logic_register_lock_cb(void (*cb)(void));
void app_logic_register_unlock_cb(void (*cb)(void));

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

#ifdef __cplusplus
}
#endif

#endif // APP_LOGIC_H
