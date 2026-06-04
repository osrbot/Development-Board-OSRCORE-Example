idf_component_register(
    SRCS
        "main.c"
        "qmi8658.c"
        "madgwick.c"
        "pid.c"
        "imu_heater.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_adc
        esp_timer
        esp_system
        nvs_flash
        driver
        freertos
)
