idf_component_register(
    SRCS "main.c" "qmc6309.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_i2c
        esp_driver_gpio
        esp_driver_usb_serial_jtag
        esp_timer
)
