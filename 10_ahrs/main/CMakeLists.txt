idf_component_register(
    SRCS "main.c" "qmi8658.c" "madgwick.c" "imu_heater.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_i2c
        esp_driver_gpio
        esp_timer
        osrcore_fw_update
)
