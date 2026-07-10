# LVGL Data Flow

LVGL v9 · EEZ Studio generated UI · ESP-IDF v5.3.2

---

## EEZ Studio Pattern

The UI is defined visually in EEZ Studio and exported as C code into:

```
components/lockin_capstone/src/ui/
  screens.c      — create_screen_*() and tick_screen_*() for each screen
  screens.h
  vars.h         — extern declarations for all getters/setters
  ui.h / ui.c    — loadScreen(), objects struct
```

EEZ Studio regenerates these files when you export. Since the user has permitted direct edits to this directory, `screens.c` and `vars.h` are modified manually going forward — no EEZ Studio regeneration needed.

---

## Getter / Setter Contract

Every dynamic UI value follows this pattern:

**In `app_logic.c` (or the relevant module):**
```c
static char display_tim_str[16] = "25:00";

const char *get_var_display_tim_str(void) {
    return display_tim_str;
}
void set_var_display_tim_str(const char *val) {
    strncpy(display_tim_str, val, sizeof(display_tim_str) - 1);
}
```

**In `vars.h`:**
```c
extern const char *get_var_display_tim_str(void);
extern void set_var_display_tim_str(const char *val);
```

**In `tick_screen_main()` (screens.c):**
```c
const char *new_val = get_var_display_tim_str();
const char *cur_val = lv_label_get_text(objects.display_tim);
if (strcmp(new_val, cur_val) != 0)
    lv_label_set_text(objects.display_tim, new_val);
```

The `strcmp` guard is critical — `lv_label_set_text` triggers a redraw. Calling it every tick even when the value hasn't changed causes unnecessary redraws and wastes CPU.

---

## `tick_screen_*` Polling Loop

EEZ Studio generates a `tick_screen_<name>()` function for each screen. These are called by the LVGL task every frame while that screen is active:

```c
// In ui.c (generated)
void ui_tick(void) {
    switch (currentScreen) {
        case SCREEN_ID_MAIN:     tick_screen_main();     break;
        case SCREEN_ID_CLOCK:    tick_screen_clock();    break;
        case SCREEN_ID_SETTINGS: tick_screen_settings(); break;
    }
}
```

Each `tick_screen_*` function reads getters and updates LVGL widgets only when values have changed.

---

## `lv_async_call` for Cross-Task UI

LVGL is **not thread-safe**. Any code running outside the LVGL task (e.g. audio detection task, FreeRTOS timer callbacks, motor task) must not call LVGL functions directly.

**Pattern:**
```c
// From audio-sr.c (audio task context):
lv_async_call((lv_async_cb_t)clock_ambient_brighten, NULL);
```

`lv_async_call` queues a callback to be executed on the next LVGL task iteration. This is the only safe way to trigger UI changes from non-LVGL tasks.

Functions called via `lv_async_call` must match the signature `void fn(void *arg)`. If the target function has a different signature, cast to `lv_async_cb_t` only if the calling convention is compatible (no pointer arg used).

---

## Clock Screen Layout (`screens.c: create_screen_clock`)

The clock screen is a custom design (not EEZ Studio generated). Layout on a 240×240 display:

```
y=52   Day of week      clock_text_2   font22  #7b8fa1
y=86   Accent bar       60×3px         #2196f3
y=106  HH:MM (time)     clock_text     font48  white
y=162  :SS (seconds)    s_clock_sec_label font20 #6b9ab8  (x offset +52)
y=194  Separator line   140×1px        #2a2a2a
y=208  DD Mon YYYY      clock_text_1   font20  #555e66

bottom-24  Hint label   220px wide     font16  #3d5263  (center aligned, wrap)
```

`s_clock_sec_label` is a `static lv_obj_t *` declared in `screens.c`, not part of the EEZ `objects` struct, since it was added manually.

### Seconds Update (`tick_screen_clock`)

```c
if (s_clock_sec_label) {
    const char *new_val = get_var_clock_seconds_str();  // e.g. ":45"
    const char *cur_val = lv_label_get_text(s_clock_sec_label);
    if (strcmp(new_val, cur_val) != 0)
        lv_label_set_text(s_clock_sec_label, new_val);
}
```

`get_var_clock_seconds_str()` (in `app_logic.c`) uses `strftime(..., ":%S", ...)` on the current local time.

---

## Ambient Clock Mode (`screen_swipe.c`)

The ambient clock is managed by three LVGL timers (all run in the LVGL task context):

| Timer | Period | Purpose |
|---|---|---|
| `s_idle_timer` | 5000ms | Check if idle ≥ 20s → enter ambient |
| `s_ramp_timer` | 40ms | Step brightness by ±3% toward target (paused when idle) |
| `s_quote_timer` | 6000ms | Cycle hint label through 9 strings |

### State Transitions

```
Normal mode
  ↓ (no activity for 20s, timer not running)
Ambient mode: dim to 10%, switch to clock screen
  ↓ (wakeword detected)
clock_ambient_brighten(): ramp up, stay on clock, clear s_ambient_active
  ↓ (voice command confirmed, or touch, or gesture)
clock_ambient_wake(): ramp up + navigate to MAIN if on clock
```

### Brightness Ramp

`start_ramp_to(target)` sets `s_ramp_target` and resumes `s_ramp_timer`. Each tick:
- If `s_current_dim < target`: increment by 3, clamp at target
- If `s_current_dim > target`: decrement by 3, clamp at target
- At target: set `s_ramp_target = -1`, pause timer

The saved brightness (`get_var_screen_brightness_val()`) is read at ramp start — so if the user changes brightness while in ambient, the ramp restores to the new value.

---

## Key Invariants

1. **Never call LVGL from a non-LVGL task directly** — use `lv_async_call`.
2. **Always strcmp before lv_label_set_text** — avoid unnecessary redraws.
3. **`tick_screen_*` must be fast** — avoid heap allocation or blocking calls here.
4. **`loadScreen()` is the only safe way to switch screens** — it handles LVGL screen lifecycle (delete old, load new).
5. **`s_clock_sec_label` is only valid after `create_screen_clock()` has run** — check for NULL before use.
