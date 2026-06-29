#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_button_plus_pressed(lv_event_t * e);
extern void action_button_minus_pressed(lv_event_t * e);
extern void action_button_start_pomo_pressed(lv_event_t * e);
extern void action_button_lock_pressed(lv_event_t * e);
extern void action_button_unlock_pressed(lv_event_t * e);
extern void action_button_reset_device_pressed(lv_event_t * e);
extern void action_slider_volume_released(lv_event_t * e);
extern void action_slider_brightness_released(lv_event_t * e);
extern void action_button_start_pairing_pressed(lv_event_t * e);
extern void action_button_end_session_pressed(lv_event_t * e);
extern void action_button_start_resume_pressed(lv_event_t * e);
extern void action_plus_5_button_pressed(lv_event_t * e);
extern void action_button_fast_forward_pressed(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/