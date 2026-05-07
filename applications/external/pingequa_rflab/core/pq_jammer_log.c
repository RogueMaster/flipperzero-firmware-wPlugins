/**
 * @file pq_jammer_log.c
 * @brief Jammer 会话日志实现.直接 storage_file_write 写 CSV.
 */
#include "pq_jammer_log.h"

#include "../views/jammer_view.h" /* JammerMode 枚举与名字 */

#include <furi.h>
#include <stdio.h>
#include <storage/storage.h>

#define TAG "PqJammerLog"

static const char* mode_name_of(uint8_t idx) {
    switch(idx) {
    case 0: return "CW Custom";
    case 1: return "BLE Adv";
    case 2: return "BLE React";
    case 3: return "WiFi 1";
    case 4: return "WiFi 6";
    case 5: return "WiFi 11";
    case 6: return "ALL 2.4G";
    default: return "Unknown";
    }
}

static bool write_line(File* f, const char* s) {
    size_t len = strlen(s);
    return storage_file_write(f, s, (uint16_t)len) == len;
}

bool pq_jammer_log_session(
    uint8_t mode_index,
    uint8_t cw_channel,
    uint32_t start_ms,
    uint32_t chunks,
    uint32_t reactive_jams) {
    /* Mode 越界保护 — 不报错,记录原始值留给分析 */
    const char* mode_name = mode_name_of(mode_index);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/pingequa");
    storage_simply_mkdir(storage, PQ_JAMMER_LOG_DIR);

    char path[96];
    uint32_t end_ms = furi_get_tick();
    uint32_t duration_ms = end_ms - start_ms;
    snprintf(path, sizeof(path), "%s/session_%010lu.csv", PQ_JAMMER_LOG_DIR,
             (unsigned long)start_ms);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    char buf[128];

    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "open failed: %s", path);
            break;
        }

        /* 注释段 — 易读 */
        if(!write_line(file, "# PINGEQUA RF Lab Jammer Session\n")) break;
        snprintf(buf, sizeof(buf), "# Mode: %s\n", mode_name);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "# Mode index: %u\n", mode_index);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "# CW channel: %u (~%d MHz)\n", cw_channel,
                 2400 + cw_channel);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "# Start boot ms: %lu\n", (unsigned long)start_ms);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "# End boot ms: %lu\n", (unsigned long)end_ms);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "# Duration ms: %lu\n", (unsigned long)duration_ms);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "# Total chunks (last run): %lu\n",
                 (unsigned long)chunks);
        if(!write_line(file, buf)) break;
        if(mode_index == JammerModeReactiveBle) {
            snprintf(buf, sizeof(buf), "# Total reactive jams: %lu\n",
                     (unsigned long)reactive_jams);
            if(!write_line(file, buf)) break;
        }

        /* CSV key,value 段 — Python pandas 可直接 read_csv */
        if(!write_line(file, "key,value\n")) break;

        snprintf(buf, sizeof(buf), "mode_index,%u\n", mode_index);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "mode_name,%s\n", mode_name);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "cw_channel,%u\n", cw_channel);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "start_boot_ms,%lu\n", (unsigned long)start_ms);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "end_boot_ms,%lu\n", (unsigned long)end_ms);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "duration_ms,%lu\n", (unsigned long)duration_ms);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "chunks,%lu\n", (unsigned long)chunks);
        if(!write_line(file, buf)) break;
        snprintf(buf, sizeof(buf), "reactive_jams,%lu\n", (unsigned long)reactive_jams);
        if(!write_line(file, buf)) break;

        ok = true;
        FURI_LOG_I(TAG, "logged session: mode=%s dur=%lums chunks=%lu jams=%lu → %s",
                   mode_name, (unsigned long)duration_ms, (unsigned long)chunks,
                   (unsigned long)reactive_jams, path);
    } while(0);

    if(!ok) FURI_LOG_E(TAG, "log write failed");

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
