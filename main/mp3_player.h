#ifndef MP3_PLAYER_H
#define MP3_PLAYER_H

#include <stdint.h>
#include <stdbool.h>

// 初始化MP3播放器
esp_err_t mp3_player_init(void);

// 播放MP3文件
esp_err_t mp3_player_play(const char* file_path);

// 停止播放
esp_err_t mp3_player_stop(void);

// 删除当前MP3文件
esp_err_t mp3_player_delete_file(const char* file_path);

// 检查是否正在播放
bool mp3_player_is_playing(void);

#endif /* MP3_PLAYER_H */
