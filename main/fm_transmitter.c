#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "fm_transmitter.h"

// 配置
#define TAG "FM_TRANSMITTER"

// LEDC配置
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_4_BIT  // 4位占空比，以达到更高的频率

// 音频配置
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CENTER 128
#define AUDIO_AMPLITUDE 127

// 频率调制配置
#define FM_DEVIATION 75000  // 75kHz频率偏移

// 状态
static bool is_enabled = false;
static ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_DUTY_RES,
    .freq_hz = FM_FREQUENCY,
    .speed_mode = LEDC_MODE,
    .timer_num = LEDC_TIMER,
    .clk_cfg = LEDC_AUTO_CLK,
};

static ledc_channel_config_t ledc_channel = {
    .channel = LEDC_CHANNEL,
    .duty = 0,
    .gpio_num = FM_PWM_PIN,
    .speed_mode = LEDC_MODE,
    .hpoint = 0,
    .timer_sel = LEDC_TIMER,
};

// 初始化FM发射器
esp_err_t fm_transmitter_init(void)
{
    ESP_LOGI(TAG, "初始化FM发射器");
    
    // 配置LEDC定时器
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        ESP_LOGE(TAG, "LEDC定时器配置失败");
        return ESP_FAIL;
    }
    
    // 配置LEDC通道
    if (ledc_channel_config(&ledc_channel) != ESP_OK) {
        ESP_LOGE(TAG, "LEDC通道配置失败");
        return ESP_FAIL;
    }
    
    // 设置占空比为50%
    if (ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 8) != ESP_OK) {
        ESP_LOGE(TAG, "设置LEDC占空比失败");
        return ESP_FAIL;
    }
    
    if (ledc_update_duty(LEDC_MODE, LEDC_CHANNEL) != ESP_OK) {
        ESP_LOGE(TAG, "更新LEDC占空比失败");
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
    
    // 更新定时器频率
    ledc_timer.freq_hz = frequency;
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        ESP_LOGE(TAG, "更新LEDC定时器频率失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// 发送音频信号到FM发射器
esp_err_t fm_transmitter_send_sample(uint8_t audio_sample)
{
    if (!is_enabled) {
        return ESP_OK;
    }
    
    // 将音频样本转换为频率偏移
    // audio_sample范围: 0-255
    // 转换为频率偏移: -FM_DEVIATION 到 +FM_DEVIATION
    float normalized_sample = (float)(audio_sample - AUDIO_CENTER) / AUDIO_AMPLITUDE;
    int32_t frequency_offset = (int32_t)(normalized_sample * FM_DEVIATION);
    
    // 计算新的频率
    uint32_t new_frequency = FM_FREQUENCY + frequency_offset;
    
    // 更新LEDC频率
    // 注意：频繁更新频率可能会导致性能问题
    // 实际应用中应该使用更高效的调制方法
    ledc_timer.freq_hz = new_frequency;
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        ESP_LOGE(TAG, "更新LEDC频率失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// 启用FM发射器
esp_err_t fm_transmitter_enable(void)
{
    ESP_LOGI(TAG, "启用FM发射器");
    
    // 确保LEDC通道已配置
    if (ledc_channel_config(&ledc_channel) != ESP_OK) {
        ESP_LOGE(TAG, "配置LEDC通道失败");
        return ESP_FAIL;
    }
    
    is_enabled = true;
    return ESP_OK;
}

// 禁用FM发射器
esp_err_t fm_transmitter_disable(void)
{
    ESP_LOGI(TAG, "禁用FM发射器");
    
    // 关闭LEDC通道
    if (ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "停止LEDC通道失败");
        return ESP_FAIL;
    }
    
    is_enabled = false;
    return ESP_OK;
}

// 检查FM发射器是否已启用
bool fm_transmitter_is_enabled(void)
{
    return is_enabled;
}
