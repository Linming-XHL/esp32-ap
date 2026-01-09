idf_component_register(SRCS "cmd_system.c"
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash driver
                    PRIV_REQUIRES esp_wifi nvs_flash)