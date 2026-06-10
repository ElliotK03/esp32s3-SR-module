idf_component_register(SRCS "ui.c" "screens.c" "styles.c" "images.c" "../../../../main/app_logic.c"
                    INCLUDE_DIRS "." "../../../../main/include"
                    REQUIRES lvgl freertos)