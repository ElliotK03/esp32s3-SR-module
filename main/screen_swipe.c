#include "screen_swipe.h"
#include "screens.h"
#include "ui.h"
#include <lvgl.h>

// Order of screens left-to-right: alarm, clock, main (default), settings
static const enum ScreensEnum screen_order[] = {
    SCREEN_ID_ALARM,
    SCREEN_ID_CLOCK,
    SCREEN_ID_MAIN,
    SCREEN_ID_SETTINGS,
};
static const int screen_count = sizeof(screen_order) / sizeof(screen_order[0]);

// current index in screen_order; default to the index of SCREEN_ID_MAIN
static int current_index = 2;

static int find_index_for_screen(enum ScreensEnum id) {
    for (int i = 0; i < screen_count; ++i) {
        if (screen_order[i] == id) return i;
    }
    return -1;
}

static void goto_index(int idx) {
    if (idx < 0 || idx >= screen_count) return;
    current_index = idx;
    loadScreen(screen_order[current_index]);
}

static void gesture_event_cb(lv_event_t * e) {
    (void)e;
    lv_indev_t * indev = lv_indev_get_act();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    if (dir == LV_DIR_LEFT) {
        // Swipe left -> move to next (to the right)
        int next = current_index + 1;
        if (next >= screen_count) next = screen_count - 1;
        if (next != current_index) goto_index(next);
    } else if (dir == LV_DIR_RIGHT) {
        // Swipe right -> move to previous (to the left)
        int prev = current_index - 1;
        if (prev < 0) prev = 0;
        if (prev != current_index) goto_index(prev);
    }
}

void swipe_init(void) {
    // Ensure screens exist
    // Attach gesture handlers to root screen objects
    lv_obj_t *scrs[] = { objects.alarm, objects.clock, objects.main, objects.settings };

    // determine current index from loaded screen if possible
    int idx = find_index_for_screen(SCREEN_ID_MAIN);
    if (idx >= 0) current_index = idx; // keep default at MAIN index (2)

    for (size_t i = 0; i < sizeof(scrs)/sizeof(scrs[0]); ++i) {
        if (scrs[i]) {
            lv_obj_add_event_cb(scrs[i], gesture_event_cb, LV_EVENT_GESTURE, NULL);
        }
    }
}
