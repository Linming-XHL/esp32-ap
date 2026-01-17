#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "midi_player.h"

// 配置
#define TAG "MIDI_PLAYER"
#define MIDI_TASK_STACK_SIZE 8192
#define MIDI_TASK_PRIORITY 5

// MIDI音符频率表（C0到B8）
const float midi_note_frequencies[128] = {
    8.18, 8.66, 9.18, 9.72, 10.30, 10.91, 11.56, 12.25, 12.98, 13.75, 14.57, 15.43,
    16.35, 17.32, 18.35, 19.45, 20.60, 21.83, 23.12, 24.50, 25.96, 27.50, 29.14, 30.87,
    32.70, 34.65, 36.71, 38.89, 41.20, 43.65, 46.25, 49.00, 51.91, 55.00, 58.27, 61.74,
    65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 116.54, 123.47,
    130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08, 246.94,
    261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88,
    523.25, 554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99, 830.61, 880.00, 932.33, 987.77,
    1046.50, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760.00, 1864.66, 1975.53,
    2093.00, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520.00, 3729.31, 3951.07,
    4186.01, 4434.92, 4698.63, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040.00, 7458.62, 7902.13
};

// MIDI事件类型
#define MIDI_EVENT_NOTE_ON 0x90
#define MIDI_EVENT_NOTE_OFF 0x80
#define MIDI_EVENT_CONTROL_CHANGE 0xB0

// 音符状态
typedef struct {
    bool active;
    float frequency;
    float phase;
    uint8_t velocity;
    uint32_t start_time;
} midi_note_t;

// 通道状态
typedef struct {
    midi_note_t notes[128];
    uint8_t volume;
    uint8_t pan;
} midi_channel_t;

// 播放器状态
static bool is_playing = false;
static bool loop_playback = false;
static char current_midi_file[256] = "";
static TaskHandle_t midi_task_handle = NULL;
static midi_channel_t channels[MIDI_MAX_CHANNELS];
static uint8_t current_sample = 128;

// 初始化MIDI播放器
esp_err_t midi_player_init(void)
{
    ESP_LOGI(TAG, "初始化MIDI播放器");
    
    // 初始化通道状态
    memset(channels, 0, sizeof(channels));
    for (int i = 0; i < MIDI_MAX_CHANNELS; i++) {
        channels[i].volume = MIDI_VOLUME;
        channels[i].pan = 64;
    }
    
    is_playing = false;
    loop_playback = false;
    current_sample = 128;
    
    ESP_LOGI(TAG, "MIDI播放器初始化完成");
    return ESP_OK;
}

// 生成正弦波样本
static float generate_sine_wave(float frequency, float* phase, float sample_rate)
{
    float sample = sin(*phase);
    *phase += 2 * M_PI * frequency / sample_rate;
    if (*phase >= 2 * M_PI) {
        *phase -= 2 * M_PI;
    }
    return sample;
}

// 混合所有激活的音符
static uint8_t mix_notes(void)
{
    float mix = 0.0f;
    int active_notes = 0;
    
    // 混合所有通道的所有激活音符
    for (int ch = 0; ch < MIDI_MAX_CHANNELS; ch++) {
        for (int note = 0; note < 128; note++) {
            if (channels[ch].notes[note].active) {
                float sample = generate_sine_wave(
                    channels[ch].notes[note].frequency,
                    &channels[ch].notes[note].phase,
                    MIDI_SAMPLE_RATE
                );
                
                // 应用音量
                float volume = (float)channels[ch].notes[note].velocity / 127.0f;
                volume *= (float)channels[ch].volume / 127.0f;
                
                mix += sample * volume;
                active_notes++;
            }
        }
    }
    
    // 如果没有激活的音符，返回中心值
    if (active_notes == 0) {
        return 128;
    }
    
    // 归一化并转换为uint8_t
    mix /= (float)active_notes;
    mix = fmaxf(-1.0f, fminf(1.0f, mix));
    
    return (uint8_t)((mix + 1.0f) * 127.5f);
}

// 解析MIDI消息
static void parse_midi_message(uint8_t* message, size_t length)
{
    if (length < 1) return;
    
    uint8_t status = message[0];
    uint8_t channel = status & 0x0F;
    uint8_t event = status & 0xF0;
    
    switch (event) {
        case MIDI_EVENT_NOTE_ON:
            if (length >= 3) {
                uint8_t note = message[1];
                uint8_t velocity = message[2];
                
                if (velocity > 0) {
                    // 音符开启
                    channels[channel].notes[note].active = true;
                    channels[channel].notes[note].frequency = midi_note_frequencies[note];
                    channels[channel].notes[note].phase = 0.0f;
                    channels[channel].notes[note].velocity = velocity;
                    channels[channel].notes[note].start_time = xTaskGetTickCount();
                } else {
                    // 速度为0表示音符关闭
                    channels[channel].notes[note].active = false;
                }
            }
            break;
            
        case MIDI_EVENT_NOTE_OFF:
            if (length >= 3) {
                uint8_t note = message[1];
                channels[channel].notes[note].active = false;
            }
            break;
            
        case MIDI_EVENT_CONTROL_CHANGE:
            if (length >= 3) {
                uint8_t control = message[1];
                uint8_t value = message[2];
                
                switch (control) {
                    case 0x07:  // 音量控制
                        channels[channel].volume = value;
                        break;
                    case 0x0A:  // 声像控制
                        channels[channel].pan = value;
                        break;
                }
            }
            break;
            
        default:
            // 忽略其他MIDI事件
            break;
    }
}

// MIDI播放任务
static void midi_play_task(void* arg)
{
    char* file_path = (char*)arg;
    FILE* fp = fopen(file_path, "rb");
    
    if (!fp) {
        ESP_LOGE(TAG, "无法打开MIDI文件: %s", file_path);
        is_playing = false;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "开始播放MIDI文件: %s", file_path);
    
    // 读取MIDI文件头
    uint8_t header[14];
    if (fread(header, 1, 14, fp) != 14) {
        ESP_LOGE(TAG, "无法读取MIDI文件头");
        fclose(fp);
        is_playing = false;
        vTaskDelete(NULL);
        return;
    }
    
    // 检查是否为有效的MIDI文件
    if (memcmp(header, "MThd", 4) != 0) {
        ESP_LOGE(TAG, "无效的MIDI文件格式");
        fclose(fp);
        is_playing = false;
        vTaskDelete(NULL);
        return;
    }
    
    // 简化实现：循环生成音频样本
    // 实际应用中需要解析完整的MIDI文件结构
    const int sample_delay = 1000 / MIDI_SAMPLE_RATE;
    
    while (is_playing) {
        // 混合所有激活的音符生成样本
        current_sample = mix_notes();
        
        // 模拟MIDI消息（测试用）
        // 实际应用中需要从文件中读取并解析MIDI消息
        static bool test_note_active = false;
        static uint32_t test_note_time = 0;
        uint32_t current_time = xTaskGetTickCount();
        
        if (current_time - test_note_time > 1000) {
            test_note_time = current_time;
            test_note_active = !test_note_active;
            
            if (test_note_active) {
                // 发送音符开启消息
                uint8_t note_on_msg[] = {MIDI_EVENT_NOTE_ON, 60, 100}; // C4音符，力度100
                parse_midi_message(note_on_msg, sizeof(note_on_msg));
            } else {
                // 发送音符关闭消息
                uint8_t note_off_msg[] = {MIDI_EVENT_NOTE_OFF, 60, 0}; // C4音符
                parse_midi_message(note_off_msg, sizeof(note_off_msg));
            }
        }
        
        // 延迟以达到采样率
        vTaskDelay(sample_delay / portTICK_PERIOD_MS);
    }
    
    fclose(fp);
    ESP_LOGI(TAG, "MIDI播放完成");
    
    // 停止所有音符
    for (int ch = 0; ch < MIDI_MAX_CHANNELS; ch++) {
        for (int note = 0; note < 128; note++) {
            channels[ch].notes[note].active = false;
        }
    }
    
    current_sample = 128;
    is_playing = false;
    vTaskDelete(NULL);
}

// 加载并播放MIDI文件
esp_err_t midi_player_play_file(const char* file_path, bool loop)
{
    if (!file_path || strlen(file_path) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (is_playing) {
        midi_player_stop();
    }
    
    // 保存当前播放的文件路径
    strncpy(current_midi_file, file_path, sizeof(current_midi_file) - 1);
    loop_playback = loop;
    
    // 创建播放任务
    if (xTaskCreate(midi_play_task, "midi_play_task", MIDI_TASK_STACK_SIZE, 
                    (void*)current_midi_file, MIDI_TASK_PRIORITY, &midi_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "创建MIDI播放任务失败");
        return ESP_FAIL;
    }
    
    is_playing = true;
    return ESP_OK;
}

// 停止播放
esp_err_t midi_player_stop(void)
{
    if (!is_playing) {
        return ESP_OK;
    }
    
    is_playing = false;
    
    // 等待任务结束
    if (midi_task_handle != NULL) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        midi_task_handle = NULL;
    }
    
    // 停止所有音符
    for (int ch = 0; ch < MIDI_MAX_CHANNELS; ch++) {
        for (int note = 0; note < 128; note++) {
            channels[ch].notes[note].active = false;
        }
    }
    
    current_sample = 128;
    return ESP_OK;
}

// 检查是否正在播放
bool midi_player_is_playing(void)
{
    return is_playing;
}

// 获取当前播放的音频样本
uint8_t midi_player_get_current_sample(void)
{
    return current_sample;
}
