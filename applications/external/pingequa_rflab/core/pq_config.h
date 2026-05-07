/**
 * @file pq_config.h
 * @brief 板卡配置(数据 + 持久化).规范 §5.2 + §8.1.
 *
 * 数据存于 /ext/apps_data/pingequa/board.conf,FlipperFormat 文本格式.
 * 加载失败一律 fallback 到默认值(规范 §5.2 强制约束).
 *
 * 字段含义参见规范 §8.1:
 *   Filetype: "PINGEQUA Board Config"   (品牌全大写,覆盖规范示例)
 *   Version:  1
 *   Board Type: "PINGEQUA_2IN1"
 *   CC1101 CSN Pin: 4   (= STM32 PA4)
 *   NRF24 CSN Pin:  7   (= STM32 PC3)
 *   Shared GD0/CE Pin: 6 (= STM32 PB2)
 *   Last Detect: <unix timestamp,板卡探测成功时间>
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** 配置文件 schema 版本.字段增减时 +1,加载时严格匹配. */
#define PQ_CONFIG_VERSION 1

/** Storage 路径(规范 §8.2 数据目录). */
#define PQ_CONFIG_DATA_DIR "/ext/apps_data/pingequa"
#define PQ_CONFIG_PATH     PQ_CONFIG_DATA_DIR "/board.conf"

/**
 * 板卡型号枚举.规范 §5.1 + §9 命名约定(枚举值用 UPPER_SNAKE).
 * 将来加新板时只在此扩展;NOT_FOUND = 0 保证零初始化语义安全.
 */
typedef enum {
    PQ_BOARD_NOT_FOUND = 0,
    PQ_BOARD_PINGEQUA_2IN1,
} PqBoardType;

/**
 * 带 tag 的结构体声明(允许其它头文件做 `typedef struct PqBoardConfig
 * PqBoardConfig;` 前向声明).pq_chip_arbiter.h 就是这么用的.
 */
struct PqBoardConfig {
    uint16_t version;
    PqBoardType board_type;
    uint8_t cc1101_csn_pin;    /* Flipper GPIO 头物理脚号(4 = PA4) */
    uint8_t nrf24_csn_pin;     /* (7 = PC3) */
    uint8_t shared_gd0_ce_pin; /* (6 = PB2) */
    uint32_t last_detect_ts;
};
typedef struct PqBoardConfig PqBoardConfig;

/**
 * 写入 cfg 默认值(板型 = PINGEQUA 2-in-1,引脚硬连规范默认).
 * load 失败时也会被自动调用.
 */
void pq_config_set_defaults(PqBoardConfig* cfg);

/**
 * 从 PQ_CONFIG_PATH 读取并解析.
 *
 * @return  true  = 文件存在且 schema/字段合法,cfg 已填充
 *          false = 任一环节失败(文件不存在 / 版本不匹配 / 字段缺失);
 *                  cfg 仍被填充为默认值,可直接使用
 *
 * 规范 §5.2 强制约束:加载失败 fallback 默认值,绝不让上层拿到未初始化数据.
 */
bool pq_config_load(PqBoardConfig* cfg);

/**
 * 写回 PQ_CONFIG_PATH(目录不存在自动创建).
 * 典型调用点:board_detect 成功后写入 last_detect_ts.
 *
 * @return false = Storage 写失败(磁盘满 / SD 卡未插 等)
 */
bool pq_config_save(const PqBoardConfig* cfg);

/** 文件是否存在(用于"首次启动 vs 已有缓存"分支判断). */
bool pq_config_exists(void);

/** 返回 PQ_CONFIG_PATH(供 UI 显示 / 日志输出). */
const char* pq_config_path(void);
