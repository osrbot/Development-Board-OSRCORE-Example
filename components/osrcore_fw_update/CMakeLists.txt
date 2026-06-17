idf_component_register(
    SRCS "osrcore_fw_update.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update mbedtls
)
