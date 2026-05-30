set(requires
    hardware_driver
    player
    led_strip          # <-- add this
    driver             # sometimes needed explicitly
)

idf_component_register(SRCS main.c
                       REQUIRES ${requires}
                       INCLUDE_DIRS "." "include")
