set(requires
    hardware_driver
    player
    led_strip
    driver
    esp_codec_dev
    lvgl
    ui
    esp_driver_i2c
    esp_driver_spi
    esp_driver_gpio
    esp_driver_mcpwm
    network_provisioning 
    nvs_flash
    esp_wifi
    esp_event
    esp_netif
    cjson
    esp_http_server
    esp_http_client
    esp_driver_ledc
)

set(srcs
    main.c
    audio-sr.c
    chime.c
    speech_commands_action.c
    nvs-helper.c
    screen_swipe.c
    st7789.c 
    app_logic.c 
    connections.c
)

idf_component_register(SRCS ${srcs}
                       REQUIRES ${requires}
                       EMBED_TXTFILES "certs/google_root_ca.pem"
                       INCLUDE_DIRS "." "include")
