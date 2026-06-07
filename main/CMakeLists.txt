set(requires
    hardware_driver
    player
    led_strip          # <-- add this
    driver             # sometimes needed explicitly
    esp_codec_dev
)

set(priv_requires
nvs_flash
)

idf_component_register(SRCS main.c audio-sr.c chime.c speech_commands_action.c nvs-helper.c
                       REQUIRES ${requires}
                       PRIV_REQUIRES ${priv_requires}
                       INCLUDE_DIRS "." "include")
