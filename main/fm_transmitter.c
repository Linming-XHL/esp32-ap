#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "fm_transmitter.h"

// 配置
#define TAG "FM_TRANSMITTER"

// RMT配置
#define RMT_CHANNEL RMT_CHANNEL_0
#define RMT_RESOLUTION_HZ 100000000  // 100MHz分辨率
#define RMT_CLOCK_DIVIDER 1          // 时钟分频
#define RMT_MEM_BLOCK_NUM 1          // 内存块数量

// 音频配置
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CENTER 128
#define AUDIO_AMPLITUDE 127

// 频率调制配置
#define FM_DEVIATION 75000  // 75kHz频率偏移

// 状态
static bool is_enabled = false;
static rmt_config_t rmt_config = {
    .rmt_mode = RMT_MODE_TX,
    .channel = RMT_CHANNEL,
    .gpio_num = FM_PWM_PIN,
    .clk_div = RMT_CLOCK_DIVIDER,
    .mem_block_num = RMT_MEM_BLOCK_NUM,
    .tx_config = {
        .carrier_en = false,
        .loop_en = false,
        .idle_output_en = true,
        .idle_level = RMT_IDLE_LEVEL_LOW,
    },
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = RMT_RESOLUTION_HZ,
};

// 初始化FM发射器
esp_err_t fm_transmitter_init(void)
{
    ESP_LOGI(TAG, "初始化FM发射器");
    
    // 配置RMT
    if (rmt_config(&rmt_config) != ESP_OK) {
        ESP_LOGE(TAG, "RMT配置失败");
        return ESP_FAIL;
    }
    
    // 安装RMT驱动
    if (rmt_driver_install(RMT_CHANNEL, 0, 0) != ESP_OK) {
        ESP_LOGE(TAG, "RMT驱动安装失败");
        return ESP_FAIL;
    }
    
    is_enabled = false;
    ESP_LOGI(TAG, "FM发射器初始化完成");
    return ESP_OK;
}

// 设置FM频率
esp_err_t fm_transmitter_set_frequency(uint32_t frequency)
{
    ESP_LOGI(TAG, "设置FM频率: %lu Hz", frequency);
    // RMT不需要动态设置频率，直接在发送时计算
    return ESP_OK;
}

// 发送音频信号到FM发射器
esp_err_t fm_transmitter_send_sample(uint8_t audio_sample)
{
    if (!is_enabled) {
        return ESP_OK;
    }
    
    // 将音频样本转换为频率偏移
    float normalized_sample = (float)(audio_sample - AUDIO_CENTER) / AUDIO_AMPLITUDE;
    int32_t frequency_offset = (int32_t)(normalized_sample * FM_DEVIATION);
    
    // 计算当前频率
    uint32_t current_frequency = FM_FREQUENCY + frequency_offset;
    
    // 计算周期(ns)和半周期(ns)
    uint32_t period_ns = 1000000000 / current_frequency;
    uint32_t half_period_ns = period_ns / 2;
    
    // 准备RMT信号数据
    rmt_item32_t items[2] = {
        {
            .duration0 = half_period_ns,
            .level0 = 1,
            .duration1 = half_period_ns,
            .level1 = 0
        },
        {
            .duration0 = half_period_ns,
            .level0 = 1,
            .duration1 = half_period_ns,
            .level1 = 0
        }
    };
    
    // 发送RMT信号
    if (rmt_write_items(RMT_CHANNEL, items, 2, false) != ESP_OK) {
        ESP_LOGE(TAG, "发送RMT信号失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// 启用FM发射器
esp_err_t fm_transmitter_enable(void)
{
    ESP_LOGI(TAG, "启用FM发射器");
    is_enabled = true;
    return ESP_OK;
}

// 禁用FM发射器
esp_err_t fm_transmitter_disable(void)
{
    ESP_LOGI(TAG, "禁用FM发射器");
    is_enabled = false;
    return ESP_OK;
}

// 检查FM发射器是否已启用
bool fm_transmitter_is_enabled(void)
{
    return is_enabled;
}