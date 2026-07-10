#include "screen_swipe.h"
#include "screens.h"
#include "ui.h"
#include "app_logic.h"
#include "backlight.h"
#include <lvgl.h>

// ============= Screen order =============

// Order of screens left-to-right: clock, main (default), settings
static const enum ScreensEnum screen_order[] = {
    SCREEN_ID_CLOCK,
    SCREEN_ID_MAIN,
    SCREEN_ID_SETTINGS,
};
static const int screen_count = sizeof(screen_order) / sizeof(screen_order[0]);
static int current_index = 1;  // default: SCREEN_ID_MAIN

// ============= Ambient clock constants =============

#define IDLE_TIMEOUT_MS     20000   // idle before dimming to clock
#define CLOCK_DIM_PERCENT   10      // brightness while in ambient mode
#define RAMP_STEP_PERCENT   3       // brightness change per ramp tick
#define RAMP_INTERVAL_MS    40      // ms between ramp ticks (~75ms 0→100)

// ============= State =============

static uint32_t s_last_activity_ms = 0;
static bool     s_ambient_active   = false;
static int32_t  s_ramp_target      = -1;  // -1 = no ramp in progress
static int32_t  s_current_dim      = -1;  // last brightness we wrote

static lv_timer_t *s_ramp_timer  = NULL;
static lv_timer_t *s_idle_timer  = NULL;
static lv_timer_t *s_quote_timer = NULL;

// Label at the bottom of the clock screen for hints / quotes
static lv_obj_t   *s_hint_label  = NULL;
static int         s_hint_index  = 0;

// ============= Hint / quote strings =============

static const char *const s_hints[] = {
    "Try: \"Hi ESP, Start\"",
    "Focus. One task at a time.",
    "Breathe. You've got this.",
    "Try: \"Hi ESP, Pause\"",
    "Progress, not perfection.",
    "Small steps. Big results.",
    "Your best work is ahead.",
    "Rest is part of the process.",
    "Stay present. Stay focused.",
};
#define HINT_COUNT (sizeof(s_hints) / sizeof(s_hints[0]))
#define HINT_CYCLE_MS 6000

// ============= Helpers =============

static void reset_activity(void) {
    s_last_activity_ms = lv_tick_get();
}

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

// ============= Brightness ramp =============

static void ramp_tick_cb(lv_timer_t *t) {
    (void)t;
    if (s_ramp_target < 0) {
        lv_timer_pause(s_ramp_timer);
        return;
    }

    int32_t saved = get_var_screen_brightness_val();
    int32_t target = (s_ramp_target < 0) ? saved : s_ramp_target;
    if (s_current_dim < 0) s_current_dim = CLOCK_DIM_PERCENT;

    if (s_current_dim < target) {
        s_current_dim += RAMP_STEP_PERCENT;
        if (s_current_dim > target) s_current_dim = target;
    } else if (s_current_dim > target) {
        s_current_dim -= RAMP_STEP_PERCENT;
        if (s_current_dim < target) s_current_dim = target;
    }

    set_backlight_brightness(s_current_dim);

    if (s_current_dim == target) {
        s_ramp_target = -1;
        lv_timer_pause(s_ramp_timer);
    }
}

static void start_ramp_to(int32_t target) {
    s_ramp_target = target;
    lv_timer_resume(s_ramp_timer);
}

// ============= Ambient enter / wake =============

static void enter_ambient_clock(void) {
    if (s_ambient_active) return;
    s_ambient_active = true;

    int idx = find_index_for_screen(SCREEN_ID_CLOCK);
    if (idx >= 0) goto_index(idx);

    // Ramp down from current saved level to CLOCK_DIM_PERCENT
    s_current_dim = get_var_screen_brightness_val();
    start_ramp_to(CLOCK_DIM_PERCENT);
}

// Wakeword detected — restore brightness, stay on current screen
void clock_ambient_brighten(void) {
    reset_activity();
    if (!s_ambient_active) return;
    s_ambient_active = false;
    start_ramp_to(get_var_screen_brightness_val());
}

// Command confirmed — restore brightness and navigate to main screen
void clock_ambient_wake(void) {
    reset_activity();
    if (s_ambient_active) {
        s_ambient_active = false;
        start_ramp_to(get_var_screen_brightness_val());
    }
    if (screen_order[current_index] == SCREEN_ID_CLOCK) {
        int idx = find_index_for_screen(SCREEN_ID_MAIN);
        if (idx >= 0) goto_index(idx);
    }
}

// ============= Timer callbacks =============

static void idle_timer_cb(lv_timer_t *t) {
    (void)t;
    if (is_timer_running()) {
        // Session is active — keep display awake at full brightness
        reset_activity();
        return;
    }
    if (s_ambient_active) return;
    if (lv_tick_elaps(s_last_activity_ms) >= IDLE_TIMEOUT_MS) {
        enter_ambient_clock();
    }
}

static void hint_timer_cb(lv_timer_t *t) {
    (void)t;
    if (!s_hint_label) return;
    s_hint_index = (s_hint_index + 1) % HINT_COUNT;
    lv_label_set_text(s_hint_label, s_hints[s_hint_index]);
}

// ============= Event callbacks =============

static void gesture_event_cb(lv_event_t *e) {
    (void)e;
    // Any gesture wakes from ambient if needed
    if (s_ambient_active) {
        clock_ambient_wake();
        return;
    }
    reset_activity();

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    if (dir == LV_DIR_LEFT) {
        int next = current_index + 1;
        if (next >= screen_count) next = screen_count - 1;
        if (next != current_index) goto_index(next);
    } else if (dir == LV_DIR_RIGHT) {
        int prev = current_index - 1;
        if (prev < 0) prev = 0;
        if (prev != current_index) goto_index(prev);
    }
}

static void touch_event_cb(lv_event_t *e) {
    (void)e;
    if (s_ambient_active) {
        clock_ambient_wake();
    } else {
        reset_activity();
    }
}

// ============= Public init =============

void swipe_init(void) {
    lv_obj_t *scrs[] = { objects.clock, objects.main, objects.settings };

    int idx = find_index_for_screen(SCREEN_ID_MAIN);
    if (idx >= 0) current_index = idx;

    for (size_t i = 0; i < sizeof(scrs) / sizeof(scrs[0]); ++i) {
        if (scrs[i]) {
            lv_obj_add_event_cb(scrs[i], gesture_event_cb, LV_EVENT_GESTURE, NULL);
            lv_obj_add_event_cb(scrs[i], touch_event_cb,   LV_EVENT_PRESSED,  NULL);
        }
    }

    // Hint label at the bottom of the clock screen
    if (objects.clock) {
        lv_obj_t *label = lv_label_create(objects.clock);
        s_hint_label = label;
        lv_obj_set_width(label, 220);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -24);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_hex(0x3d5263), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_label_set_text(label, s_hints[0]);
    }

    reset_activity();

    // Brightness ramp timer — paused until needed
    s_ramp_timer = lv_timer_create(ramp_tick_cb, RAMP_INTERVAL_MS, NULL);
    lv_timer_pause(s_ramp_timer);

    // Idle check every 5s
    s_idle_timer = lv_timer_create(idle_timer_cb, 5000, NULL);

    // Hint rotation every 6s
    s_quote_timer = lv_timer_create(hint_timer_cb, HINT_CYCLE_MS, NULL);
}
