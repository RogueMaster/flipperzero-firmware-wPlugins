/**
 * @file pq_scan_export.c
 * @brief Scanner CSV 导出实现.直接 storage_file_write,不走 FlipperFormat
 *        (FlipperFormat 是 key-value 格式,CSV 不适合).
 */
#include "pq_scan_export.h"

#include <furi.h>
#include <stdio.h>
#include <storage/storage.h>

#define TAG "PqScanExport"

/* 写一行字符串到打开的 File*.成功 = bytes_written 等于 strlen.
 * size_t 强转 uint16_t — Flipper Storage write API 用 uint16_t. */
static bool write_line(File* f, const char* s) {
    size_t len = strlen(s);
    return storage_file_write(f, s, (uint16_t)len) == len;
}

bool pq_scan_export_csv(
    const uint8_t* hits,
    uint8_t hits_len,
    uint8_t peak_ch,
    uint16_t dwell_us,
    uint16_t sweep_count,
    char* out_path,
    size_t out_path_sz) {
    if(hits == NULL || hits_len != 126) {
        FURI_LOG_E(TAG, "invalid input: hits=%p len=%u", (void*)hits, hits_len);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* 确保目录存在(首次导出时创建). */
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/pingequa");
    storage_simply_mkdir(storage, PQ_SCAN_EXPORT_DIR);

    /* 文件名用 furi_get_tick() 作时间戳(boot 起的 ms 数,不是 wall-clock,
     * 但保证文件名唯一). */
    char path[96];
    uint32_t ts = furi_get_tick();
    snprintf(path, sizeof(path), "%s/scan_%010lu.csv", PQ_SCAN_EXPORT_DIR, (unsigned long)ts);

    File* file = storage_file_alloc(storage);
    bool ok = false;

    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "open failed: %s", path);
            break;
        }

        char buf[128];

        /* Header 注释行(#) — 兼容 Excel / Python pandas read_csv 的 comment 选项. */
        if(!write_line(file, "# PINGEQUA RF Lab Scanner Export\n")) break;

        snprintf(buf, sizeof(buf), "# Boot ms: %lu\n", (unsigned long)ts);
        if(!write_line(file, buf)) break;

        snprintf(buf, sizeof(buf), "# Sweep count: %u\n", sweep_count);
        if(!write_line(file, buf)) break;

        snprintf(buf, sizeof(buf), "# Dwell us: %u\n", dwell_us);
        if(!write_line(file, buf)) break;

        uint8_t peak_hits = (peak_ch < 126) ? hits[peak_ch] : 0;
        /* 注:`#` 注释行内不要带逗号 — Excel 默认把逗号视为列分隔符,会把单行
         * 注释拆成多列.用分号代替,Python pandas/Excel/Numbers 全部正常单行. */
        snprintf(
            buf, sizeof(buf), "# Peak channel: %u (%d MHz; %u hits)\n", peak_ch,
            2400 + peak_ch, peak_hits);
        if(!write_line(file, buf)) break;

        if(!write_line(file, "ch,freq_mhz,hits\n")) break;

        bool data_ok = true;
        for(int ch = 0; ch < 126 && data_ok; ch++) {
            snprintf(buf, sizeof(buf), "%d,%d,%u\n", ch, 2400 + ch, hits[ch]);
            if(!write_line(file, buf)) {
                data_ok = false;
                break;
            }
        }
        if(!data_ok) break;

        ok = true;
        FURI_LOG_I(TAG, "exported %s", path);
        if(out_path != NULL && out_path_sz > 0) {
            strncpy(out_path, path, out_path_sz - 1);
            out_path[out_path_sz - 1] = '\0';
        }
    } while(0);

    if(!ok) {
        FURI_LOG_E(TAG, "export failed");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
