/**
 * @file pq_jammer_config.h
 * @brief Jammer 偏好持久化(规范 §8.2 数据目录).
 *
 * 文件路径:/ext/apps_data/pingequa/jammer.conf
 * 文件类型:PINGEQUA Jammer State (FlipperFormat 文本格式).
 *
 * v0.5.0 起记录用户上次进 jammer 的 mode + CW Custom 信道,下次进 jammer
 * 直接落到上次的状态(避免每次重选).加载失败回 default(JammerModeCwCustom @ 42).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 注:不前向声明 JammerMode — views/jammer_view.h 用 anonymous-enum + typedef
 * 写法,在头文件里前向声明会冲突.PqJammerState.mode 用 uint8_t 即可,实施
 * 在 .c 文件里 cast (JammerMode). */

#define PQ_JAMMER_CONFIG_VERSION 1
#define PQ_JAMMER_CONFIG_PATH    "/ext/apps_data/pingequa/jammer.conf"

typedef struct {
    uint8_t mode;       /* JammerMode 枚举值,持久化为整数 */
    uint8_t cw_channel; /* CW Custom 选定的 NRF24 ch (0..125) */
} PqJammerState;

/** 写入默认值 (JammerModeCwCustom = 0, ch 42). */
void pq_jammer_config_set_defaults(PqJammerState* st);

/**
 * 从 PQ_JAMMER_CONFIG_PATH 读取并解析.
 * 加载失败 (文件不存在 / 版本不匹配 / 字段缺失 / 范围越界) 自动回 defaults,
 * 调用方拿到的 st 一定是合法值.
 *
 * @return true = 文件解析成功;false = 用了 defaults
 */
bool pq_jammer_config_load(PqJammerState* st);

/**
 * 写回 PQ_JAMMER_CONFIG_PATH.目录不存在自动创建.
 * 典型调用:jammer_scene_on_exit.
 *
 * @return false = Storage 写失败 (磁盘满 / SD 卡未插)
 */
bool pq_jammer_config_save(const PqJammerState* st);
