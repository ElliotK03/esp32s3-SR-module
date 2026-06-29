#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_TIMER_ARC_VALUE = 0,
    FLOW_GLOBAL_VARIABLE_DISPLAY_TIM_STR = 1,
    FLOW_GLOBAL_VARIABLE_START_END_STR = 2,
    FLOW_GLOBAL_VARIABLE_CLCOK_STR = 3,
    FLOW_GLOBAL_VARIABLE_DATE_STR = 4,
    FLOW_GLOBAL_VARIABLE_DAY_STR = 5,
    FLOW_GLOBAL_VARIABLE_WIFI_STATUS_STR = 6,
    FLOW_GLOBAL_VARIABLE_SCREEN_BRIGHTNESS_VAL = 7,
    FLOW_GLOBAL_VARIABLE_VOLUME = 8,
    FLOW_GLOBAL_VARIABLE_SESSION_START_STOP_BUTTON_STR = 9,
    FLOW_GLOBAL_VARIABLE_TIM_USER_TEXT_STR = 10
};

// Native global variables

extern int32_t get_var_timer_arc_value();
extern void set_var_timer_arc_value(int32_t value);
extern const char *get_var_display_tim_str();
extern void set_var_display_tim_str(const char *value);
extern const char *get_var_start_end_str();
extern void set_var_start_end_str(const char *value);
extern const char *get_var_clcok_str();
extern void set_var_clcok_str(const char *value);
extern const char *get_var_date_str();
extern void set_var_date_str(const char *value);
extern const char *get_var_day_str();
extern void set_var_day_str(const char *value);
extern const char *get_var_wifi_status_str();
extern void set_var_wifi_status_str(const char *value);
extern int32_t get_var_screen_brightness_val();
extern void set_var_screen_brightness_val(int32_t value);
extern int32_t get_var_volume();
extern void set_var_volume(int32_t value);
extern const char *get_var_session_start_stop_button_str();
extern void set_var_session_start_stop_button_str(const char *value);
extern const char *get_var_tim_user_text_str();
extern void set_var_tim_user_text_str(const char *value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/