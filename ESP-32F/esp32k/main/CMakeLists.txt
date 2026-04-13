idf_component_register(
        SRCS
        "esp32k_main.c"
        "message_center.c"
        "led_output.c"
        "wifi_service.c"
        "http_server_app.c"
        PRIV_REQUIRES spi_flash
        INCLUDE_DIRS "."
        REQUIRES esp_driver_gpio esp_driver_ledc esp_wifi esp_event esp_netif nvs_flash esp_http_server
)
