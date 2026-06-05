#include <math.h>
#include <stdlib.h>
#include "chime.h"
#include "esp_log.h"

#define TAG "Chime"
#define SAMPLE_RATE 16000
#define AMPLITUDE   16000

static void play_note(float freq, int ms) {
    int n = SAMPLE_RATE * ms / 1000;
    int fade = 80;
    int16_t *buf = malloc(n * sizeof(int16_t));
    if (!buf) {
        ESP_LOGE(TAG, "play_note: malloc failed");
        return;
    }
    for (int i = 0; i < n; i++) {
        float env = 1.0f;
        if (i < fade)   env = (float)i / fade;
        if (i > n-fade) env = (float)(n - i) / fade;
        buf[i] = (int16_t)(AMPLITUDE * env * sinf(2.0f * M_PI * freq * i / SAMPLE_RATE));
    }
    esp_audio_play(buf, n * sizeof(int16_t), portMAX_DELAY);
    free(buf);
}

void chime_play_wake(void) {
    play_note(523.25f, 120);  // C5
    play_note(659.25f, 120);  // E5
    play_note(784.00f, 220);  // G5
}

void chime_play_ack(void) {
    play_note(880.00f, 150);  // A5
}