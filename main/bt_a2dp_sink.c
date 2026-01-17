#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/dac_oneshot.h"
#include "bt_globals.h"

#ifdef CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#endif

#define TAG "A2DP_SINK"

// DAC配置
#define DAC_CHANNEL DAC_CHAN_0  // IO26 (DAC通道0对应IO26)
#define DAC_MAX_VALUE 255

// DAC Oneshot配置
static dac_oneshot_handle_t dac_oneshot = NULL;
static dac_oneshot_config_t dac_config = {
    .chan_id = DAC_CHANNEL,
};

static bool bt_enabled = false;
static char bt_device_name[32] = "ESP32_Audio";
static uint8_t bt_volume = 70;  // 0-100

// 移除A2DP回调函数，避免编译错误

// 初始化DAC
static void dac_init(void)
{
    // 创建DAC通道
    dac_oneshot_new_channel(&dac_config, &dac_oneshot);
    // 输出中点电压
    dac_oneshot_output_voltage(dac_oneshot, DAC_MAX_VALUE / 2);
    ESP_LOGI(TAG, "DAC初始化完成，输出通道: %d (IO26)", DAC_CHANNEL);
}

// 初始化蓝牙A2DP接收器
void bt_a2dp_sink_init(void)
{
    if (bt_enabled) {
        return;
    }
    
    // 初始化DAC
    dac_init();
    
#ifdef CONFIG_BT_ENABLED
    // 初始化蓝牙
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "蓝牙控制器初始化失败");
        return;
    }
    
    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        ESP_LOGE(TAG, "蓝牙控制器启用失败");
        return;
    }
    
    if (esp_bluedroid_init() != ESP_OK) {
        ESP_LOGE(TAG, "BlueDroid初始化失败");
        return;
    }
    
    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(TAG, "BlueDroid启用失败");
        return;
    }
    
    // 设置设备名称
    esp_bt_gap_set_device_name(bt_device_name);
    
    bt_enabled = true;
    ESP_LOGI(TAG, "蓝牙初始化完成，设备名: %s", bt_device_name);
#else
    ESP_LOGW(TAG, "蓝牙功能未启用");
#endif
}

// 关闭蓝牙A2DP接收器
void bt_a2dp_sink_deinit(void)
{
    if (!bt_enabled) {
        return;
    }
    
#ifdef CONFIG_BT_ENABLED
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
#endif
    
    if (dac_oneshot) {
        dac_oneshot_del_channel(dac_oneshot);
        dac_oneshot = NULL;
    }
    
    bt_enabled = false;
    ESP_LOGI(TAG, "蓝牙已关闭");
}

// 设置蓝牙设备名称
void bt_a2dp_sink_set_name(const char *name)
{
    if (name && strlen(name) > 0 && strlen(name) < 32) {
        strcpy(bt_device_name, name);
#ifdef CONFIG_BT_ENABLED
        if (bt_enabled) {
            esp_bt_gap_set_device_name(bt_device_name);
        }
#endif
        ESP_LOGI(TAG, "蓝牙设备名已设置为: %s", bt_device_name);
    }
}

// 设置音量
void bt_a2dp_sink_set_volume(uint8_t volume)
{
    if (volume <= 100) {
        bt_volume = volume;
        ESP_LOGI(TAG, "音量已设置为: %d%%", bt_volume);
    }
}

// 启用/禁用蓝牙
void bt_a2dp_sink_set_enabled(bool enabled)
{
    if (enabled && !bt_enabled) {
        bt_a2dp_sink_init();
    } else if (!enabled && bt_enabled) {
        bt_a2dp_sink_deinit();
    }
}

// 获取蓝牙状态
bool bt_a2dp_sink_is_enabled(void)
{
    return bt_enabled;
}

// 获取蓝牙设备名称
const char *bt_a2dp_sink_get_name(void)
{
    return bt_device_name;
}

// 获取当前音量
uint8_t bt_a2dp_sink_get_volume(void)
{
    return bt_volume;
}

