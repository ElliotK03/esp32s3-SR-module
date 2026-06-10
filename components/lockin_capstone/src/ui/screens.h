#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_CLOCK = 2,
    SCREEN_ID_ALARM = 3,
    SCREEN_ID_SETTINGS = 4,
    _SCREEN_ID_LAST = 4
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *clock;
    lv_obj_t *alarm;
    lv_obj_t *settings;
    lv_obj_t *minus_button;
    lv_obj_t *plus_button;
    lv_obj_t *obj0;
    lv_obj_t *time_text;
    lv_obj_t *pomo_start_end_button;
    lv_obj_t *clock_text;
    lv_obj_t *clock_text_1;
    lv_obj_t *clock_text_2;
    lv_obj_t *screen_brightness_slider;
    lv_obj_t *volume_slider;
    lv_obj_t *locker_lock;
    lv_obj_t *locker_unlock;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_clock();
void tick_screen_clock();

void create_screen_alarm();
void tick_screen_alarm();

void create_screen_settings();
void tick_screen_settings();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/