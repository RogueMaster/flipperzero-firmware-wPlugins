/**
 * @file pq_jammer_config.c
 * @brief Jammer 偏好持久化实现.沿用 pq_config.c 的 FlipperFormat + Storage 模板.
 *
 * 范围验证:JammerMode 0..6(对应 7 种模式);CW channel 0..125.越界回 defaults.
 */
#include "pq_jammer_config.h"

#include "../views/jammer_view.h" /* JammerModeCount 校验上界 */

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define TAG "PqJammerCfg"

#define PQ_JAMMER_FILETYPE  "PINGEQUA Jammer State"
#define KEY_MODE            "Mode"
#define KEY_CW_CHANNEL      "CW Channel"

#define NRF24_CHANNEL_MAX_LOCAL 125

/* JammerModeCwCustom 是默认 — 头文件 enum 第 0 项 = 0,所以 default mode=0 即可,
 * 不必 #include views/jammer_view.h 在头文件里(避免循环依赖). */
void pq_jammer_config_set_defaults(PqJammerState* st) {
    furi_check(st);
    st->mode = 0;        /* JammerModeCwCustom */
    st->cw_channel = 42; /* ~2442 MHz */
}

bool pq_jammer_config_load(PqJammerState* st) {
    furi_check(st);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, PQ_JAMMER_CONFIG_PATH)) {
            FURI_LOG_I(TAG, "config not present at %s", PQ_JAMMER_CONFIG_PATH);
            break;
        }

        uint32_t ver = 0;
        if(!flipper_format_read_header(ff, tmp, &ver)) {
            FURI_LOG_W(TAG, "read_header failed");
            break;
        }
        if(furi_string_cmp_str(tmp, PQ_JAMMER_FILETYPE) != 0) {
            FURI_LOG_W(TAG, "filetype mismatch: '%s'", furi_string_get_cstr(tmp));
            break;
        }
        if(ver != PQ_JAMMER_CONFIG_VERSION) {
            FURI_LOG_W(TAG, "version mismatch: got %lu want %d", ver, PQ_JAMMER_CONFIG_VERSION);
            break;
        }

        uint32_t v = 0;
        if(!flipper_format_read_uint32(ff, KEY_MODE, &v, 1)) break;
        if(v >= (uint32_t)JammerModeCount) {
            FURI_LOG_W(TAG, "mode out of range: %lu (max %d)", v, JammerModeCount - 1);
            break;
        }
        st->mode = (uint8_t)v;

        if(!flipper_format_read_uint32(ff, KEY_CW_CHANNEL, &v, 1)) break;
        if(v > NRF24_CHANNEL_MAX_LOCAL) {
            FURI_LOG_W(TAG, "cw_channel out of range: %lu", v);
            break;
        }
        st->cw_channel = (uint8_t)v;

        ok = true;
        FURI_LOG_I(TAG, "loaded: mode=%u ch=%u", st->mode, st->cw_channel);
    } while(0);

    if(!ok) {
        FURI_LOG_I(TAG, "load failed; using defaults");
        pq_jammer_config_set_defaults(st);
    }

    furi_string_free(tmp);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool pq_jammer_config_save(const PqJammerState* st) {
    furi_check(st);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data/pingequa"); /* 目录可能首次不存在 */

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, PQ_JAMMER_CONFIG_PATH)) {
            FURI_LOG_E(TAG, "open_always failed: %s", PQ_JAMMER_CONFIG_PATH);
            break;
        }

        if(!flipper_format_write_header_cstr(ff, PQ_JAMMER_FILETYPE, PQ_JAMMER_CONFIG_VERSION)) {
            break;
        }

        uint32_t v;
        v = st->mode;
        if(!flipper_format_write_uint32(ff, KEY_MODE, &v, 1)) break;
        v = st->cw_channel;
        if(!flipper_format_write_uint32(ff, KEY_CW_CHANNEL, &v, 1)) break;

        ok = true;
        FURI_LOG_I(TAG, "saved: mode=%u ch=%u", st->mode, st->cw_channel);
    } while(0);

    if(!ok) FURI_LOG_E(TAG, "save failed");

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
