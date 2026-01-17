#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/dac_oneshot.h"
#include "bt_globals.h"

#ifdef CONFIG_BT_A2DP_SINK_ENABLED
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

#ifdef CONFIG_BT_A2DP_SINK_ENABLED

// A2DP回调函数
static void bt_a2d_sink_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "A2DP连接已建立");
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "A2DP连接已断开");
            }
            break;
        
        case ESP_A2D_AUDIO_STATE_EVT:
            if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
                ESP_LOGI(TAG, "音频播放已开始");
            } else if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STOPPED) {
                ESP_LOGI(TAG, "音频播放已停止");
            }
            break;
        
        case ESP_A2D_AUDIO_CFG_EVT:
            // 简化音频配置日志，避免访问不存在的结构体成员
            ESP_LOGI(TAG, "音频配置已更新");
            break;
        
        default:
            break;
    }
}

// A2DP音频数据回调
static void bt_a2d_sink_data_cb(const uint8_t *data, uint32_t len)
{
    static uint8_t dac_output[2048];
    int sample_count;
    int i;
    
    if (len == 0) {
        return;
    }
    
    // 解析PCM数据 (16位立体声)
    sample_count = len / 4;  // 每个立体声样本4字节
    
    // 转换为单声道并调整音量
    for (i = 0; i < sample_count; i++) {
        // 取左声道样本
        int16_t left_sample = ((int16_t)data[i*4 + 1] << 8) | data[i*4];
        // 取右声道样本
        int16_t right_sample = ((int16_t)data[i*4 + 3] << 8) | data[i*4 + 2];
        // 混合为单声道
        int16_t mono_sample = (left_sample + right_sample) / 2;
        // 调整音量
        int16_t vol_sample = (mono_sample * bt_volume) / 100;
        // 转换为DAC输出范围 (0-255)
        dac_output[i] = (uint8_t)((vol_sample + 32768) / 256);
    }
    
    // 通过DAC输出音频
    for (i = 0; i < sample_count; i++) {
        dac_oneshot_output_voltage(dac_oneshot, dac_output[i]);
        // 简单的延迟以匹配采样率
        esp_rom_delay_us(22);  // 约44.1kHz采样率
    }
}

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
    
    // 配置A2DP接收器
    esp_a2d_sink_init();
    esp_a2d_sink_register_callback(bt_a2d_sink_cb);
    esp_a2d_sink_register_data_callback(bt_a2d_sink_data_cb);
    
    // 配置蓝牙GAP
    esp_bt_gap_register_callback(NULL);
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    
    bt_enabled = true;
    ESP_LOGI(TAG, "蓝牙A2DP接收器初始化完成，设备名: %s", bt_device_name);
}

// 关闭蓝牙A2DP接收器
void bt_a2dp_sink_deinit(void)
{
    if (!bt_enabled) {
        return;
    }
    
    esp_a2d_sink_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    if (dac_oneshot) {
        dac_oneshot_del_channel(dac_oneshot);
        dac_oneshot = NULL;
    }
    
    bt_enabled = false;
    ESP_LOGI(TAG, "蓝牙A2DP接收器已关闭");
}

// 设置蓝牙设备名称
void bt_a2dp_sink_set_name(const char *name)
{
    if (name && strlen(name) > 0 && strlen(name) < 32) {
        strcpy(bt_device_name, name);
        if (bt_enabled) {
            esp_bt_gap_set_device_name(bt_device_name);
        }
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

#else // CONFIG_BT_A2DP_SINK_ENABLED

// 当蓝牙A2DP SINK功能未启用时，提供空实现
void bt_a2dp_sink_init(void)
{
    ESP_LOGW(TAG, "蓝牙A2DP SINK功能未启用");
}

void bt_a2dp_sink_deinit(void)
{
    ESP_LOGW(TAG, "蓝牙A2DP SINK功能未启用");
}

void bt_a2dp_sink_set_name(const char *name)
{
    ESP_LOGW(TAG, "蓝牙A2DP SINK功能未启用");
}

void bt_a2dp_sink_set_volume(uint8_t volume)
{
    ESP_LOGW(TAG, "蓝牙A2DP SINK功能未启用");
}

void bt_a2dp_sink_set_enabled(bool enabled)
{
    ESP_LOGW(TAG, "蓝牙A2DP SINK功能未启用");
}

bool bt_a2dp_sink_is_enabled(void)
{
    return false;
}

const char *bt_a2dp_sink_get_name(void)
{
    return "ESP32_Audio";
}

uint8_t bt_a2dp_sink_get_volume(void)
{
    return 70;
}

#endif // CONFIG_BT_A2DP_SINK_ENABLED