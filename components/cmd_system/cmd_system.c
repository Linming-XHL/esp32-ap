/*
 * SPDX-FileCopyrightText: 2015-2020 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_pm.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "cmd_system.h"
#include "router_globals.h"

static const char *TAG = "cmd_system";

/** 'restart' command restarts the chip */
static int restart(int argc, char **argv)
{
    ESP_LOGI(TAG, "Restarting...");
    esp_restart();
    return 0;
}

static void register_restart(void)
{
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Restart the program",
        .hint = NULL,
        .func = &restart,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'free' command prints available heap memory */
static int free_mem(int argc, char **argv)
{
    printf("%u\n", esp_get_free_heap_size());
    return 0;
}

static void register_free_mem(void)
{
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "Get the current size of free heap memory",
        .hint = NULL,
        .func = &free_mem,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'flash_id' command reads flash ID */
static int flash_id(int argc, char **argv)
{
    uint32_t id = 0;
    esp_err_t ret = esp_flash_read_id(esp_flash_default_chip, &id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading flash ID: %s", esp_err_to_name(ret));
        return 1;
    }
    printf("%08