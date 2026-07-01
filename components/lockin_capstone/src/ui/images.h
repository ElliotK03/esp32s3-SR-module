#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_wifi_bitmap;
extern const lv_img_dsc_t img_volume_bitmap;
extern const lv_img_dsc_t img_brightness_bitmap;
extern const lv_img_dsc_t img_play_arrow_bitmap;
extern const lv_img_dsc_t img_stop_bitmap;
extern const lv_img_dsc_t img_pause_bitmap;
extern const lv_img_dsc_t img_ff_bitmap;
extern const lv_img_dsc_t img_plus_bitmap;
extern const lv_img_dsc_t img_minus_bitmap;
extern const lv_img_dsc_t img_lock_bitmap;
extern const lv_img_dsc_t img_unlock_bitmap;
extern const lv_img_dsc_t img_unlock_24_px;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[12];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/