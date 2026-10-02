idf_component_register(SRCS "main.c" "controller.c" "board.c" "phone.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_uart esp_adc esp_timer
             esp_wifi esp_event esp_netif esp_http_server nvs_flash
    EMBED_TXTFILES "web.html")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
