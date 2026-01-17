#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
#include "driver/dac_oneshot.h"
#include "mp3_player.h"

// 配置
#define TAG "MP3_PLAYER"
#define DAC_CHANNEL DAC_CHAN_0  // 使用DAC通道0（对应IO26）
#define DAC_MAX_VALUE 255
#define MP3_TASK_STACK_SIZE 4096
#define MP3_TASK_PRIORITY 5

// DAC配置
static dac_oneshot_handle_t dac_oneshot = NULL;
static dac_oneshot_config_t dac_config = {
    .chan_id = DAC_CHANNEL,
};

// 播放器状态
static bool is_playing = false;
static TaskHandle_t mp3_task_handle = NULL;
static char current_mp3_file[256] = "";

// 初始化DAC
static esp_err_t dac_init(void)
{
    // 创建DAC通道
    if (dac_oneshot_new_channel(&dac_config, &dac_oneshot) != ESP_OK) {
        ESP_LOGE(TAG, "DAC初始化失败");
        return ESP_FAIL;
    }
    // 输出中点电压
    dac_oneshot_output_voltage(dac_oneshot, DAC_MAX_VALUE / 2);
    ESP_LOGI(TAG, "DAC初始化完成");
    return ESP_OK;
}

// MP3播放任务
static void mp3_play_task(void* arg)
{
    char* file_path = (char*)arg;
    FILE* fp = fopen(file_path, "rb");
    
    if (!fp) {
        ESP_LOGE(TAG, "无法打开MP3文件: %s", file_path);
        is_playing = false;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "开始播放MP3文件: %s", file_path);
    
    // 这里实现一个简单的音频输出测试，产生正弦波
    // 实际项目中需要使用ESP-IDF的音频组件（如audio_pipeline、esp_audio等）实现MP3解码
    
    // 产生1kHz的正弦波
    const int sample_rate = 44100; // 采样率
    const float frequency = 1000.0f; // 频率
    const int duration = 10000; // 持续时间（毫秒）
    const int sample_count = (sample_rate * duration) / 1000;
    
    float amplitude = DAC_MAX_VALUE / 2.0f;
    float offset = DAC_MAX_VALUE / 2.0f;
    
    for (int i = 0; i < sample_count; i++) {
        // 计算正弦波值
        float t = (float)i / sample_rate;
        float value = offset + amplitude * sin(2 * M_PI * frequency * t);
        
        // 输出到DAC
        dac_oneshot_output_voltage(dac_oneshot, (uint8_t)value);
        
        // 延迟以达到采样率
        vTaskDelay(1000 / sample_rate);
    }
    
    fclose(fp);
    ESP_LOGI(TAG, "MP3播放完成");
    
    // 播放完成后删除文件
    if (remove(file_path) == 0) {
        ESP_LOGI(TAG, "MP3文件已删除: %s", file_path);
        memset(current_mp3_file, 0, sizeof(current_mp3_file));
    } else {
        ESP_LOGE(TAG, "删除MP3文件失败: %s", file_path);
    }
    
    // 恢复中点电压（静音）
    dac_oneshot_output_voltage(dac_oneshot, DAC_MAX_VALUE / 2);
    
    is_playing = false;
    vTaskDelete(NULL);
}

// 初始化MP3播放器
esp_err_t mp3_player_init(void)
{
    ESP_LOGI(TAG, "初始化MP3播放器");
    
    // 初始化DAC
    if (dac_init() != ESP_OK) {
        return ESP_FAIL;
    }
    
    is_playing = false;
    return ESP_OK;
}

// 播放MP3文件
esp_err_t mp3_player_play(const char* file_path)
{
    if (!file_path || strlen(file_path) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (is_playing) {
        ESP_LOGW(TAG, "播放器正在播放中");
        return ESP_FAIL;
    }
    
    // 保存当前播放的文件路径
    strncpy(current_mp3_file, file_path, sizeof(current_mp3_file) - 1);
    
    // 创建播放任务
    if (xTaskCreate(mp3_play_task, "mp3_play_task", MP3_TASK_STACK_SIZE, 
                    (void*)current_mp3_file, MP3_TASK_PRIORITY, &mp3_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "创建MP3播放任务失败");
        return ESP_FAIL;
    }
    
    is_playing = true;
    return ESP_OK;
}

// 停止播放
esp_err_t mp3_player_stop(void)
{
    if (!is_playing) {
        return ESP_OK;
    }
    
    // 停止播放任务
    if (mp3_task_handle != NULL) {
        vTaskDelete(mp3_task_handle);
        mp3_task_handle = NULL;
    }
    
    // 输出中点电压（静音）
    dac_oneshot_output_voltage(dac_oneshot, DAC_MAX_VALUE / 2);
    
    is_playing = false;
    
    // 删除当前文件
    if (strlen(current_mp3_file) > 0) {
        if (remove(current_mp3_file) == 0) {
            ESP_LOGI(TAG, "MP3文件已删除: %s", current_mp3_file);
            memset(current_mp3_file, 0, sizeof(current_mp3_file));
        } else {
            ESP_LOGE(TAG, "删除MP3文件失败: %s", current_mp3_file);
        }
    }
    
    return ESP_OK;
}

// 删除当前MP3文件
esp_err_t mp3_player_delete_file(const char* file_path)
{
    if (!file_path || strlen(file_path) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 如果正在播放该文件，先停止播放
    if (is_playing && strcmp(current_mp3_file, file_path) == 0) {
        mp3_player_stop();
        return ESP_OK;
    }
    
    // 删除文件
    if (remove(file_path) == 0) {
        ESP_LOGI(TAG, "MP3文件已删除: %s", file_path);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "删除MP3文件失败: %s", file_path);
        return ESP_FAIL;
    }
}

// 检查是否正在播放
bool mp3_player_is_playing(void)
{
    return is_playing;
}
