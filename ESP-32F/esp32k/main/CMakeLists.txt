idf_component_register(
        SRCS "esp32k_main.c"
        PRIV_REQUIRES spi_flash
        INCLUDE_DIRS ""
        REQUIRES esp_driver_gpio
)
