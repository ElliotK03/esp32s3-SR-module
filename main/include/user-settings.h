#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t work_duration_s;       // pomodoro work period in seconds
    uint32_t short_break_s;         // short break duration
    uint32_t long_break_s;          // long break duration
    uint8_t  sessions_before_long;  // how many sessions before a long break
} pomodoro_settings_t;

typedef struct {
    float    wakenet_threshold;     // runtime detection threshold
    bool     enabled;
    uint8_t  volume;                // playback volume 0-100
} voice_settings_t;

typedef struct {
    pomodoro_settings_t pomodoro;
    voice_settings_t    voice;
} user_settings_t;

#define USER_SETTINGS_DEFAULT {                 \
    .pomodoro = {                               \
        .work_duration_s      = 25 * 60,        \
        .short_break_s        = 5  * 60,        \
        .long_break_s         = 15 * 60,        \
        .sessions_before_long = 4,              \
    },                                          \
    .voice = {                                  \
        .enabled           = true,              \
        .wakenet_threshold = 0.6f,              \
        .volume            = 70,                \
    },                                          \
}