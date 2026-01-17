#ifndef MIDI_PLAYER_H
#define MIDI_PLAYER_H

#include <stdint.h>
#include <stdbool.h>

// MIDI配置
#define MIDI_MAX_CHANNELS 16
#define MIDI_SAMPLE_RATE 44100
#define MIDI_VOLUME 127

// MIDI音符频率表（C0到B8）
extern const float midi_note_frequencies[128];

// 初始化MIDI播放器
esp_err_t midi_player_init(void);

// 加载并播放MIDI文件
esp_err_t midi_player_play_file(const char* file_path, bool loop);

// 停止播放
esp_err_t midi_player_stop(void);

// 检查是否正在播放
bool midi_player_is_playing(void);

// 获取当前播放的音频样本
uint8_t midi_player_get_current_sample(void);

#endif /* MIDI_PLAYER_H */
