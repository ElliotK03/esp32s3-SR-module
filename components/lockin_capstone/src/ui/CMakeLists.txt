idf_component_register(
    SRCS
        "ui_image_stop_bitmap.c"
        "ui_image_pause_bitmap.c"
        "ui_image_play_arrow_bitmap.c"
        "ui_image_brightness_bitmap.c"
        "ui_image_wifi_bitmap.c"
        "ui_image_volume_bitmap.c"
        "ui_image_plus_bitmap.c"
        "ui_image_minus_bitmap.c"
        "ui_image_ff_bitmap.c"
        "ui.c"
        "screens.c"
        "styles.c"
        "images.c"
        "../../../../main/app_logic.c"

    INCLUDE_DIRS
        "."
        "../../../../main/include"

    REQUIRES
        lvgl
        freertos
        hardware_driver
)