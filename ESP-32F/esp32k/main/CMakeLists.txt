idf_component_register(
        SRCS
        "esp32k_main.c"
        "message_center.c"
        "system_status.c"
        "led_output.c"
        "ble_ancs_manager.c"
        "ancs_notification_parser.c"
        "notification_filter.c"
        "notification_rules.c"
        "wifi_manager.c"
        "http_server_app.c"
        "device_config.c"
        "recent_apps.c"
        "phase_effect.c"
        "display_service.c"
        PRIV_REQUIRES spi_flash
        INCLUDE_DIRS "."
        REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_spi esp_wifi esp_event esp_netif nvs_flash esp_http_server bt json esp_timer
)
