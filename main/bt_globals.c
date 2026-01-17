#include <string.h>
#include "bt_globals.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "BT_GLOBALS"
#define NVS_NAMESPACE "esp32_nat"

// 全局配置变量
global_config_t g_config;

// 初始化全局配置
void init_global_config(void)
{
    // 蓝牙默认配置
    g_config.bluetooth.enabled = true;
    strcpy(g_config.bluetooth.device_name, "ESP32_Audio");
    g_config.bluetooth.volume = 70;
    
    // 从NVS加载配置
    if (load_config_from_nvs() != ESP_OK) {
        ESP_LOGW(TAG, "无法从NVS加载配置，使用默认配置");
        // 保存默认配置到NVS
        save_config_to_nvs();
    }
}

// 保存配置到NVS
esp_err_t save_config_to_nvs(void)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;
    
    // 打开NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "无法打开NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    // 保存蓝牙配置
    err = nvs_set_u8(nvs_handle, "bt_enabled", g_config.bluetooth.enabled ? 1 : 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存bt_enabled失败: %s", esp_err_to_name(err));
        goto exit;
    }
    
    err = nvs_set_str(nvs_handle, "bt_name", g_config.bluetooth.device_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存bt_name失败: %s", esp_err_to_name(err));
        goto exit;
    }
    
    err = nvs_set_u8(nvs_handle, "bt_volume", g_config.bluetooth.volume);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存bt_volume失败: %s", esp_err_to_name(err));
        goto exit;
    }
    
    // 提交更改
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS提交失败: %s", esp_err_to_name(err));
        goto exit;
    }
    
    ESP_LOGI(TAG, "配置已保存到NVS");
    
exit:
    nvs_close(nvs_handle);
    return err;
}

// 从NVS加载配置
esp_err_t load_config_from_nvs(void)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;
    
    // 打开NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "无法打开NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    // 加载蓝牙配置
    uint8_t bt_enabled = 0;
    err = nvs_get_u8(nvs_handle, "bt_enabled", &bt_enabled);
    if (err == ESP_OK) {
        g_config.bluetooth.enabled = (bt_enabled != 0);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "加载bt_enabled失败: %s", esp_err_to_name(err));
        goto exit;
    }
    
    char bt_name[32];
    size_t bt_name_len = sizeof(bt_name);
    err = nvs_get_str(nvs_handle, "bt_name", bt_name, &bt_name_len);
    if (err == ESP_OK) {
        strcpy(g_config.bluetooth.device_name, bt_name);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "加载bt_name失败: %s", esp_err_to_name(err));
        goto exit;
    }
    
    uint8_t bt_volume = 0;
    err = nvs_get_u8(nvs_handle, "bt_volume", &bt_volume);
    if (err == ESP_OK) {
        g_config.bluetooth.volume = bt_volume;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "加载bt_volume失败: %s", esp_err_to_name(err));
        goto exit;
    }
    
    ESP_LOGI(TAG, "配置已从NVS加载");
    
exit:
    nvs_close(nvs_handle);
    return err;
}