#ifndef FM_TRANSMITTER_H
#define FM_TRANSMITTER_H

#include <stdint.h>
#include <stdbool.h>

// 配置
#define FM_FREQUENCY 85000000  // 85.0MHz
#define FM_PWM_PIN 27          // 使用GPIO27作为PWM输出

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

#endif /* FM_TRANSMITTER_H */
