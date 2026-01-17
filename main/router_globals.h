#pragma once

#include <stdint.h>
#include <stdbool.h>

// 蓝牙配置结构体
typedef struct {
    bool enabled;               // 蓝牙是否启用
    char device_name[32];       // 蓝牙设备名称
    uint8_t volume;             // 音量 (0-100)
} bluetooth_config_t;

// 全局配置结构体
typedef struct {
    bluetooth_config_t bluetooth;
    // 其他配置项...
} global_config_t;

// 全局配置变量
extern global_config_t g_config;

// 初始化全局配置
void init_global_config(void);

// 保存配置到NVS
esp_err_t save_config_to_nvs(void);

// 从NVS加载配置
esp_err_t load_config_from_nvs(void);