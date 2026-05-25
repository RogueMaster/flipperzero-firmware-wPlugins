/**
 * @file pq_scan_export.c
 * @brief Scanner CSV 导出实现.直接 storage_file_write,不走 FlipperFormat
 *        (FlipperFormat 是 key-value 格式,CSV 不适合).
 */
#include "pq_scan_export.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <stdio.h>
#include <storage/storage.h>

#define TAG "PqScanExport"

/* 写一行字符串到打开的 File*.成功 = bytes_written 等于 strlen.
 * size_t 强转 uint16_t — Flipper Storage write API 用 uint16_t. */
static bool write_line(File* f, const char* s) {
    size_t len = strlen(s);
    return storage_file_write(f, s, (uint16_t)len) == len;
}

/* 文件名优先用 RTC 墙钟:scan_YYYY-MM-DD_HHMMSS_chNN.csv.日期在前 → qFlipper
 * 文件列表按名排序即按时间排序;文件名自带峰值通道,无需打开即知摘要.
 * RTC 未设(year<2024)时回退 scan_bootNNNNNNNNNN_chNN.csv,避免 1970 垃圾名.
 * 同秒重名时加 _1.._99 后缀保唯一(furi_hal_rtc_get_datetime 仅秒级精度). */
static void build_scan_filename(
    Storage* storage,
    const DateTime* dt,
    uint8_t peak_ch,
    char* path,
    size_t path_sz) {
    char base[96];
    if(dt->year < 2024) {
        snprintf(
            base,
            sizeof(base),
            "%s/scan_boot%010lu_ch%u",
            PQ_SCAN_EXPORT_DIR,
            (unsigned long)furi_get_tick(),
            peak_ch);
    } else {
        snprintf(
            base,
            sizeof(base),
            "%s/scan_%04u-%02u-%02u_%02u%02u%02u_ch%u",
            PQ_SCAN_EXPORT_DIR,
            dt->year,
            dt->month,
            dt->day,
            dt->hour,
            dt->minute,
            dt->second,
            peak_ch);
    }
    snprintf(path, path_sz, "%s.csv", base);
    for(int i = 1; i <= 99 && storage_file_exists(storage, path); i++) {
        snprintf(path, path_sz, "%s_%d.csv", base, i);
    }
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

    /* 墙钟用于文件名 + header. */
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char path[128];
    build_scan_filename(storage, &dt, peak_ch, path, sizeof(path));

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

        if(dt.year >= 2024) {
            snprintf(
                buf,
                sizeof(buf),
                "# Datetime: %04u-%02u-%02u %02u:%02u:%02u\n",
                dt.year,
                dt.month,
                dt.day,
                dt.hour,
                dt.minute,
                dt.second);
        } else {
            snprintf(buf, sizeof(buf), "# Datetime: (RTC not set)\n");
        }
        if(!write_line(file, buf)) break;

        snprintf(buf, sizeof(buf), "# Sweep count: %u\n", sweep_count);
        if(!write_line(file, buf)) break;

        snprintf(buf, sizeof(buf), "# Dwell us: %u\n", dwell_us);
        if(!write_line(file, buf)) break;

        uint8_t peak_hits = (peak_ch < 126) ? hits[peak_ch] : 0;
        /* 注:`#` 注释行内不要带逗号 — Excel 默认把逗号视为列分隔符,会把单行
         * 注释拆成多列.用分号代替,Python pandas/Excel/Numbers 全部正常单行. */
        snprintf(
            buf,
            sizeof(buf),
            "# Peak channel: %u (%d MHz; %u hits)\n",
            peak_ch,
            2400 + peak_ch,
            peak_hits);
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
