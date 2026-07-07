idf_component_register(
    SRCS "esp32_ble_scanner.cpp"
    INCLUDE_DIRS "include" "../../../../app"
    PRIV_REQUIRES bt nvs_flash esp_hosted
)
