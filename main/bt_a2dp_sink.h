#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化蓝牙A2DP接收器
 */
void bt_a2dp_sink_init(void);

/**
 * @brief 关闭蓝牙A2DP接收器
 */
void bt_a2dp_sink_deinit(void);

/**
 * @brief 设置蓝牙设备名称
 * @param name 设备名称
 */
void bt_a2dp_sink_set_name(const char *name);

/**
 * @brief 设置音量
 * @param volume 音量值 (0-100)
 */
void bt_a2dp_sink_set_volume(uint8_t volume);

/**
 * @brief 启用/禁用蓝牙
 * @param enabled true: 启用, false: 禁用
 */
void bt_a2dp_sink_set_enabled(bool enabled);

/**
 * @brief 获取蓝牙状态
 * @return true: 已启用, false: 已禁用
 */
bool bt_a2dp_sink_is_enabled(void);

/**
 * @brief 获取蓝牙设备名称
 * @return 设备名称
 */
const char *bt_a2dp_sink_get_name(void);

/**
 * @brief 获取当前音量
 * @return 音量值 (0-100)
 */
uint8_t bt_a2dp_sink_get_volume(void);