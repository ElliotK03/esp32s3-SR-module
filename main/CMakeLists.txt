set(requires
    hardware_driver
    player
    led_strip          # <-- add this
    driver             # sometimes needed explicitly
    esp_codec_dev
)

idf_component_register(SRCS main.c audio-sr.c chime.c speech_commands_action.c
                       REQUIRES ${requires}
                       INCLUDE_DIRS "." "include")
