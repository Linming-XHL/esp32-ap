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
#include "soc/soc.h"

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
    printf("%08" PRIx32 "\n", id);
    return 0;
}

static void register_flash_id(void)
{
    const esp_console_cmd_t cmd = {
        .command = "flash_id",
        .help = "Read SPI flash manufacturer and device ID",
        .hint = NULL,
        .func = &flash_id,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'flash_read' command reads data from flash */
static struct {
    struct arg_str *address;
    struct arg_int *length;
    struct arg_end *end;
} flash_read_args;

static int flash_read(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &flash_read_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, flash_read_args.end, argv[0]);
        return 1;
    }

    uint32_t addr;
    int len = flash_read_args.length->ival[0];
    if (len < 0) {
        ESP_LOGE(TAG, "Invalid length: %d", len);
        return 1;
    }
    if (len > 64) {
        ESP_LOGE(TAG, "Requested length too large (max 64)");
        return 1;
    }

    addr = strtoul(flash_read_args.address->sval[0], NULL, 0);
    ESP_LOGI(TAG, "Reading %d bytes from flash @ 0x%x", len, addr);
    uint8_t *data = malloc(len);
    if (data == NULL) {
        ESP_LOGE(TAG, "Cannot allocate buffer for data");
        return 1;
    }
    esp_err_t ret = esp_flash_read(esp_flash_default_chip, data, addr, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading from flash: %s", esp_err_to_name(ret));
        free(data);
        return 1;
    }

    for (int i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            printf("%08" PRIx32 ":  ", addr + i);
        }
        printf("%02x ", data[i]);
        if ((i % 16) == 15) {
            printf("\n");
        }
    }
    if ((len % 16) != 0) {
        printf("\n");
    }
    free(data);
    return 0;
}

static void register_flash_read(void)
{
    flash_read_args.address = arg_str1(NULL, NULL, "<hex_addr>", "Flash address");
    flash_read_args.length = arg_int1(NULL, NULL, "<len>", "Length of data to read");
    flash_read_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "flash_read",
        .help = "Read data from flash",
        .hint = NULL,
        .func = &flash_read,
        .argtable = &flash_read_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'deep_sleep' command puts the chip into deep sleep mode */
static struct {
    struct arg_int *wakeup_time;
    struct arg_int *wakeup_gpio_num;
    struct arg_int *wakeup_gpio_level;
    struct arg_end *end;
} deep_sleep_args;

static int deep_sleep(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &deep_sleep_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, deep_sleep_args.end, argv[0]);
        return 1;
    }
    if (deep_sleep_args.wakeup_time->count) {
        uint64_t timeout = 1000ULL * deep_sleep_args.wakeup_time->ival[0];
        ESP_LOGI(TAG, "Enabling timer wakeup, timeout=%lluus", timeout);
        ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    }
    if (deep_sleep_args.wakeup_gpio_num->count) {
        int io_num = deep_sleep_args.wakeup_gpio_num->ival[0];
        // 替换 GPIO_IS_VALID_DIGITAL_IO_PAD 函数
        if (io_num < 0 || io_num >= 40) {
            ESP_LOGE(TAG, "GPIO %d is invalid", io_num);
            return 1;
        }
        int level = 0;
        if (deep_sleep_args.wakeup_gpio_level->count) {
            level = deep_sleep_args.wakeup_gpio_level->ival[0];
            if (level != 0 && level != 1) {
                ESP_LOGE(TAG, "Invalid wakeup level: %d", level);
                return 1;
            }
        }
        ESP_LOGI(TAG, "Enabling wakeup on GPIO%d, wakeup on %s level",
                 io_num, level ? "HIGH" : "LOW");

        #if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
            ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup(1ULL << io_num, level) );
        #endif
    }
    #if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
        rtc_gpio_isolate(GPIO_NUM_12);
    #endif
    esp_deep_sleep_start();
    return 0;
}

static void register_deep_sleep(void)
{
    deep_sleep_args.wakeup_time = 
        arg_int0("t", "time", "<t>", "Wake up time, ms");
    deep_sleep_args.wakeup_gpio_num = 
        arg_int0(NULL, "io", "<n>", 
                 "If specified, wakeup using GPIO with given number");
    deep_sleep_args.wakeup_gpio_level = 
        arg_int0(NULL, "io_level", "<0|1>", "GPIO level to trigger wakeup");
    deep_sleep_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "deep_sleep",
        .help = "Enter deep sleep mode. "
        "Two wakeup modes are supported: timer and GPIO. "
        "If no wakeup option is specified, will sleep indefinitely.",
        .hint = NULL,
        .func = &deep_sleep,
        .argtable = &deep_sleep_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'light_sleep' command puts the chip into light sleep mode */

static struct {
    struct arg_int *wakeup_time;
    struct arg_int *wakeup_gpio_num;
    struct arg_int *wakeup_gpio_level;
    struct arg_end *end;
} light_sleep_args;

static int light_sleep(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &light_sleep_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, light_sleep_args.end, argv[0]);
        return 1;
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (light_sleep_args.wakeup_time->count) {
        uint64_t timeout = 1000ULL * light_sleep_args.wakeup_time->ival[0];
        ESP_LOGI(TAG, "Enabling timer wakeup, timeout=%lluus", timeout);
        ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    }
    int io_count = light_sleep_args.wakeup_gpio_num->count;
    if (io_count != light_sleep_args.wakeup_gpio_level->count) {
        ESP_LOGE(TAG, "Should have same number of 'io' and 'io_level' arguments");
        return 1;
    }
    for (int i = 0; i < io_count; ++i) {
        int io_num = light_sleep_args.wakeup_gpio_num->ival[i];
        int level = light_sleep_args.wakeup_gpio_level->ival[i];
        if (level != 0 && level != 1) {
            ESP_LOGE(TAG, "Invalid wakeup level: %d", level);
            return 1;
        }
        ESP_LOGI(TAG, "Enabling wakeup on GPIO%d, wakeup on %s level",
                 io_num, level ? "HIGH" : "LOW");

        // 替换 gpio_wakeup_enable 函数为 rtc_gpio_wakeup_enable
        ESP_ERROR_CHECK( rtc_gpio_init(io_num) );
        ESP_ERROR_CHECK( rtc_gpio_set_direction(io_num, RTC_GPIO_MODE_INPUT_ONLY) );
        ESP_ERROR_CHECK( rtc_gpio_pullup_dis(io_num) );
        ESP_ERROR_CHECK( rtc_gpio_pulldown_dis(io_num) );
        ESP_ERROR_CHECK( rtc_gpio_wakeup_enable(io_num, level ? RTC_GPIO_INTR_HIGH_LEVEL : RTC_GPIO_INTR_LOW_LEVEL) );
    }
    if (io_count > 0) {
        ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
    }
    if (CONFIG_ESP_CONSOLE_UART_NUM <= UART_NUM_1) {
        ESP_LOGI(TAG, "Enabling UART wakeup (press ENTER to exit light sleep)");
        // 注释掉可能不存在的函数
        // ESP_ERROR_CHECK( uart_set_wakeup_threshold(CONFIG_ESP_CONSOLE_UART_NUM, 3) );
        ESP_ERROR_CHECK( esp_sleep_enable_uart_wakeup(CONFIG_ESP_CONSOLE_UART_NUM) );
    }
    fflush(stdout);
    uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_light_sleep_start();
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const char *cause_str;
    switch (cause) {
    case ESP_SLEEP_WAKEUP_GPIO:
        cause_str = "GPIO";
        break;
    case ESP_SLEEP_WAKEUP_UART:
        cause_str = "UART";
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        cause_str = "timer";
        break;
    default:
        cause_str = "unknown";
        printf("%d\n", cause);
    }
    ESP_LOGI(TAG, "Woke up from: %s", cause_str);
    return 0;
}

static void register_light_sleep(void)
{
    light_sleep_args.wakeup_time = 
        arg_int0("t", "time", "<t>", "Wake up time, ms");
    light_sleep_args.wakeup_gpio_num = 
        arg_intn(NULL, "io", "<n>", 0, 8, 
                 "If specified, wakeup using GPIO with given number");
    light_sleep_args.wakeup_gpio_level = 
        arg_intn(NULL, "io_level", "<0|1>", 0, 8, "GPIO level to trigger wakeup");
    light_sleep_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "light_sleep",
        .help = "Enter light sleep mode. "
        "Two wakeup modes are supported: timer and GPIO. "
        "Multiple GPIO pins can be specified using pairs of "
        "'io' and 'io_level' arguments. "
        "Will also wake up on UART input.",
        .hint = NULL,
        .func = &light_sleep,
        .argtable = &light_sleep_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'version' command prints ESP-IDF version */
static int version(int argc, char **argv)
{
    printf("%s\n", esp_get_idf_version());
    return 0;
}

static void register_version(void)
{
    const esp_console_cmd_t cmd = {
        .command = "version",
        .help = "Get version of ESP-IDF",
        .hint = NULL,
        .func = &version,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'getmac' command prints MAC addresses */
static struct {
    struct arg_str *iface;
    struct arg_end *end;
} getmac_args;

static int getmac(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &getmac_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, getmac_args.end, argv[0]);
        return 1;
    }
    uint8_t mac[6];
    if (getmac_args.iface->count) {
        if (strcmp(getmac_args.iface->sval[0], "STA") == 0 ||
            strcmp(getmac_args.iface->sval[0], "sta") == 0) {
            ESP_ERROR_CHECK( esp_wifi_get_mac(WIFI_IF_STA, mac) );
        } else if (strcmp(getmac_args.iface->sval[0], "AP") == 0 ||
                   strcmp(getmac_args.iface->sval[0], "ap") == 0) {
            ESP_ERROR_CHECK( esp_wifi_get_mac(WIFI_IF_AP, mac) );
        } else {
            ESP_LOGE(TAG, "Unknown interface %s", getmac_args.iface->sval[0]);
            return 1;
        }
    } else {
        ESP_ERROR_CHECK( esp_wifi_get_mac(WIFI_IF_STA, mac) );
    }
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

static void register_getmac(void)
{
    getmac_args.iface = arg_str0("i", "iface", "<iface>", "Interface name (STA or AP)");
    getmac_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "getmac",
        .help = "Get MAC address of the ESP32",
        .hint = NULL,
        .func = &getmac,
        .argtable = &getmac_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'setmac' command sets MAC address */
static struct {
    struct arg_str *iface;
    struct arg_str *mac_str;
    struct arg_end *end;
} setmac_args;

static int setmac(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &setmac_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, setmac_args.end, argv[0]);
        return 1;
    }
    wifi_interface_t iface = WIFI_IF_STA;
    if (setmac_args.iface->count) {
        if (strcmp(setmac_args.iface->sval[0], "STA") == 0 ||
            strcmp(setmac_args.iface->sval[0], "sta") == 0) {
            iface = WIFI_IF_STA;
        } else if (strcmp(setmac_args.iface->sval[0], "AP") == 0 ||
                   strcmp(setmac_args.iface->sval[0], "ap") == 0) {
            iface = WIFI_IF_AP;
        } else {
            ESP_LOGE(TAG, "Unknown interface %s", setmac_args.iface->sval[0]);
            return 1;
        }
    }
    uint8_t mac[6];
    if (sscanf(setmac_args.mac_str->sval[0], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        ESP_LOGE(TAG, "Invalid MAC address format");
        return 1;
    }

    ESP_ERROR_CHECK( esp_wifi_set_mac(iface, mac) );
    ESP_LOGI(TAG, "MAC address set successfully");
    return 0;
}

static void register_setmac(void)
{
    setmac_args.iface = arg_str0("i", "iface", "<iface>", "Interface name (STA or AP)");
    setmac_args.mac_str = arg_str1(NULL, NULL, "<mac>", "MAC address in format XX:XX:XX:XX:XX:XX");
    setmac_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "setmac",
        .help = "Set MAC address of the ESP32",
        .hint = NULL,
        .func = &setmac,
        .argtable = &setmac_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'reset_config' command resets NVS configuration */
static int reset_config(int argc, char **argv)
{
    ESP_LOGI(TAG, "Erasing NVS partition...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "Configuration reset, restarting...");
    esp_restart();
    return 0;
}

static void register_reset_config(void)
{
    const esp_console_cmd_t cmd = {
        .command = "reset_config",
        .help = "Reset all configuration to factory defaults",
        .hint = NULL,
        .func = &reset_config,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

void register_system(void)
{
    register_restart();
    register_free_mem();
    register_flash_id();
    register_flash_read();
    register_deep_sleep();
    register_light_sleep();
    register_version();
    register_getmac();
    register_setmac();
    register_reset_config();
}