#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void backlight_init(int32_t initial_brightness);
void set_backlight_brightness(int32_t percent);

#ifdef __cplusplus
}
#endif

#endif // BACKLIGHT_H
