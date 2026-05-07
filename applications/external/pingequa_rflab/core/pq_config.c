/**
 * @file pq_config.c
 * @brief 板卡配置实现.FlipperFormat 文件读写 + Storage API.
 *
 * 规范权威:
 *   - §5.2 数据结构与 API 列表
 *   - §8.1 文件格式
 *   - §8.2 数据目录
 *
 * 所有 IO 走官方 FlipperFormat / Storage,符合规范 §10 R1(优先官方 API).
 */
#include "pq_config.h"

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define TAG "PqConfig"

/* §8.1 字段名常量 — 全部大写空格风格,与 Sub-GHz / IR 等官方文件一致.
 * 品牌名 PINGEQUA 全大写(用户口径,覆盖规范 §8.1 示例的 "Pingequa" 写法). */
#define PQ_FILETYPE        "PINGEQUA Board Config"
#define KEY_BOARD_TYPE     "Board Type"
#define KEY_CC1101_CSN     "CC1101 CSN Pin"
#define KEY_NRF24_CSN      "NRF24 CSN Pin"
#define KEY_SHARED_GD0_CE  "Shared GD0/CE Pin"
#define KEY_LAST_DETECT    "Last Detect"

#define BOARD_TYPE_2IN1_STR "PINGEQUA_2IN1"
#define BOARD_TYPE_UNKNOWN_STR "UNKNOWN"

/* ---------------------------------------------------------------------------
 * 默认值
 * --------------------------------------------------------------------------*/

void pq_config_set_defaults(PqBoardConfig* cfg) {
    furi_check(cfg);
    cfg->version = PQ_CONFIG_VERSION;
    cfg->board_type = PQ_BOARD_PINGEQUA_2IN1;
    cfg->cc1101_csn_pin = 4;     /* PA4 */
    cfg->nrf24_csn_pin = 7;      /* PC3 */
    cfg->shared_gd0_ce_pin = 6;  /* PB2 */
    cfg->last_detect_ts = 0;
}

const char* pq_config_path(void) {
    return PQ_CONFIG_PATH;
}

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------*/

/* str → PqBoardType.未知字符串 → PQ_BOARD_NOT_FOUND. */
static PqBoardType board_type_from_str(const FuriString* s) {
    if(furi_string_cmp_str(s, BOARD_TYPE_2IN1_STR) == 0) {
        return PQ_BOARD_PINGEQUA_2IN1;
    }
    return PQ_BOARD_NOT_FOUND;
}

/* PqBoardType → c-str.写文件用. */
static const char* board_type_to_str(PqBoardType t) {
    switch(t) {
    case PQ_BOARD_PINGEQUA_2IN1:
        return BOARD_TYPE_2IN1_STR;
    case PQ_BOARD_NOT_FOUND:
    default:
        return BOARD_TYPE_UNKNOWN_STR;
    }
}

/* ---------------------------------------------------------------------------
 * 公开 API
 * --------------------------------------------------------------------------*/

bool pq_config_exists(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    FS_Error err = storage_common_stat(storage, PQ_CONFIG_PATH, &info);
    furi_record_close(RECORD_STORAGE);
    return err == FSE_OK;
}

bool pq_config_load(PqBoardConfig* cfg) {
    furi_check(cfg);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, PQ_CONFIG_PATH)) {
            FURI_LOG_I(TAG, "config not present at %s", PQ_CONFIG_PATH);
            break;
        }

        /* Header:filetype 字符串 + uint32 version. */
        uint32_t ver = 0;
        if(!flipper_format_read_header(ff, tmp, &ver)) {
            FURI_LOG_W(TAG, "read_header failed");
            break;
        }
        if(furi_string_cmp_str(tmp, PQ_FILETYPE) != 0) {
            FURI_LOG_W(TAG, "filetype mismatch: '%s'", furi_string_get_cstr(tmp));
            break;
        }
        if(ver != PQ_CONFIG_VERSION) {
            FURI_LOG_W(TAG, "version mismatch: got %lu want %d", ver, PQ_CONFIG_VERSION);
            break;
        }
        cfg->version = (uint16_t)ver;

        if(!flipper_format_read_string(ff, KEY_BOARD_TYPE, tmp)) break;
        cfg->board_type = board_type_from_str(tmp);

        uint32_t v = 0;
        if(!flipper_format_read_uint32(ff, KEY_CC1101_CSN, &v, 1)) break;
        cfg->cc1101_csn_pin = (uint8_t)v;

        if(!flipper_format_read_uint32(ff, KEY_NRF24_CSN, &v, 1)) break;
        cfg->nrf24_csn_pin = (uint8_t)v;

        if(!flipper_format_read_uint32(ff, KEY_SHARED_GD0_CE, &v, 1)) break;
        cfg->shared_gd0_ce_pin = (uint8_t)v;

        if(!flipper_format_read_uint32(ff, KEY_LAST_DETECT, &cfg->last_detect_ts, 1)) break;

        ok = true;
    } while(0);

    if(!ok) {
        FURI_LOG_W(TAG, "load failed; using defaults");
        pq_config_set_defaults(cfg);
    }

    furi_string_free(tmp);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool pq_config_save(const PqBoardConfig* cfg) {
    furi_check(cfg);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* 确保目录存在.storage_simply_mkdir 对已存在目录是 no-op. */
    storage_simply_mkdir(storage, PQ_CONFIG_DATA_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        /* file_open_always:存在则 truncate,不存在则创建. */
        if(!flipper_format_file_open_always(ff, PQ_CONFIG_PATH)) {
            FURI_LOG_E(TAG, "open_always failed: %s", PQ_CONFIG_PATH);
            break;
        }

        if(!flipper_format_write_header_cstr(ff, PQ_FILETYPE, PQ_CONFIG_VERSION)) break;

        if(!flipper_format_write_string_cstr(ff, KEY_BOARD_TYPE, board_type_to_str(cfg->board_type))) {
            break;
        }

        uint32_t v;
        v = cfg->cc1101_csn_pin;
        if(!flipper_format_write_uint32(ff, KEY_CC1101_CSN, &v, 1)) break;
        v = cfg->nrf24_csn_pin;
        if(!flipper_format_write_uint32(ff, KEY_NRF24_CSN, &v, 1)) break;
        v = cfg->shared_gd0_ce_pin;
        if(!flipper_format_write_uint32(ff, KEY_SHARED_GD0_CE, &v, 1)) break;
        if(!flipper_format_write_uint32(ff, KEY_LAST_DETECT, &cfg->last_detect_ts, 1)) break;

        ok = true;
    } while(0);

    if(!ok) FURI_LOG_E(TAG, "save failed");

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
