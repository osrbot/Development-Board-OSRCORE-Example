idf_component_register(
    SRCS "osrcore_fw_update.c" "osrcore_network.c" "osrcore_params.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update mbedtls esp_wifi esp_event esp_netif esp_http_server nvs_flash esp_app_format
)
