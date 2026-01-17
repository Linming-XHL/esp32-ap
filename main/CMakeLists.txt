idf_component_register(SRCS "esp32_nat_router.c" "http_server.c" "bt_a2dp_sink.c" "bt_globals.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "../www/index.html"
                    REQUIRES json esp_wifi console nvs_flash esp_timer esp_http_server bt
                             bt/bluedroid bt/bluedroid/api
                             cmd_system cmd_nvs cmd_router driver spi_flash vfs fatfs
                             esp_netif lwip pthread wpa_supplicant freertos)

set_source_files_properties(http_server.c
    PROPERTIES COMPILE_FLAGS
    -Wno-unused-function
)
