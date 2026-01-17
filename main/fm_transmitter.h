#ifndef FM_TRANSMITTER_H
#define FM_TRANSMITTER_H

#include <stdint.h>
#include <stdbool.h>

// 配置
#define FM_FREQUENCY 85000000  // 85.0MHz
#define FM_FM_PIN 0           // 使用GPIO0作为FM输出（通过CLK_OUT1）
#define MAX_DEV_HZ 75000UL     // ±75 kHz标准广播
#define WAV_SR_HZ 8000         // 采样率（8 kHz）

// APLL配置结构
typedef struct {
    uint8_t o_div;         // 输出分频器
    uint8_t sdm2;          // 整数部分
    uint16_t base_frac16;  // 基准分数部分（16位）
    uint16_t dev_frac16;   // 偏差分数部分（16位）
    bool is_rev0;          // 是否为ESP32 rev0芯片
} fm_apll_cfg_t;

// 初始化FM发射器
esp_err_t fm_transmitter_init(void);

// 设置FM频率
esp_err_t fm_transmitter_set_frequency(uint32_t frequency);

// 发送音频信号到FM发射器
// audio_sample: 音频样本值（范围：0-255）
esp_err_t fm_transmitter_send_sample(uint8_t audio_sample);

// 启用FM发射器
esp_err_t fm_transmitter_enable(void);

// 禁用FM发射器
esp_err_t fm_transmitter_disable(void);

// 检查FM发射器是否已启用
bool fm_transmitter_is_enabled(void);

// 内部函数声明（仅用于实现）
static fm_apll_cfg_t fm_calc_apll(uint32_t fout_hz, uint32_t dev_hz);
static inline void fm_set_deviation(int16_t delta_frac16);

#endif /* FM_TRANSMITTER_H */
