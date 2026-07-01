#ifndef SCREEN_SWIPE_H
#define SCREEN_SWIPE_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void swipe_init(void);

/* Wakeword detected: restore brightness but stay on whatever screen is active. */
void clock_ambient_brighten(void);

/* Command confirmed: restore brightness and switch to main screen. */
void clock_ambient_wake(void);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_SWIPE_H
