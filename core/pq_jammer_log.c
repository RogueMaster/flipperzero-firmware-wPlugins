/**
 * @file pq_jammer_log.c
 * @brief Jammer 会话日志实现.单 key,value CSV 段(无重复 # 段).
 *
 * v0.5.2 重构:剔除原 # 注释段 + key,value 段的 100% 数据重复(每个字段写两遍).
 * 同时砍掉对用户无意义的字段(mode_index 内部值 / start_boot_ms / end_boot_ms),
 * 加上真正有分析价值的字段(engine 类型 / target_freq_mhz / 按模式条件输出
 * 的 cw_channel/cw_freq_mhz/reactive_jams).
 *
 * 注:scene_duration_s 是用户停留 jammer scene 的时长(含暂停空闲),**不是**
 * 真实 TX 发射时长.真实 TX 时长需 worker 累计 active-态时间,v0.5.2 暂未做
 * (避免动 worker 影响稳定性).
 */
#include "pq_jammer_log.h"

#include "../views/jammer_view.h" /* JammerMode 枚举与名字 */

#include <furi.h>
#include <furi_hal_rtc.h>
#include <stdio.h>
#include <storage/storage.h>

#define TAG "PqJammerLog"

static const char* mode_name_of(uint8_t idx) {
    switch(idx) {
    case JammerModeCwCustom:    return "CW Custom";
    case JammerModeBleAdv:      return "BLE Adv";
    case JammerModeReactiveBle: return "BLE React";
    case JammerModeWifi1:       return "WiFi 1";
    case JammerModeWifi6:       return "WiFi 6";
    case JammerModeWifi11:      return "WiFi 11";
    case JammerModeAllBand:     return "ALL 2.4G";
    default:                    return "Unknown";
    }
}

/* 文件名专用模式短名 — 无空格,可直接进文件名. */
static const char* mode_shortname_of(uint8_t idx) {
    switch(idx) {
    case JammerModeCwCustom:    return "CWcustom";
    case JammerModeBleAdv:      return "BLEadv";
    case JammerModeReactiveBle: return "BLEreact";
    case JammerModeWifi1:       return "WiFi1";
    case JammerModeWifi6:       return "WiFi6";
    case JammerModeWifi11:      return "WiFi11";
    case JammerModeAllBand:     return "ALL2G4";
    default:                    return "Unknown";
    }
}

/* Engine 类型 —— 描述 worker 怎么干扰的.profile 表里目前只用到 CW + Reactive
 * 两种(jammer_scene.c:102-157 JamEngine);PayloadSpam 引擎已在 v0.4.x 弃用. */
static const char* mode_engine_of(uint8_t idx) {
    return (idx == JammerModeReactiveBle) ? "Reactive" : "CW";
}

/* 目标频率描述 —— 从 profile 表硬编码(jammer_scene.c:82-100 channel 集).
 * CwCustom 返回 NULL,调用方从 app->jammer_cw_channel 推导.
 * !! 改 profile 频率需同步更新此函数 !! */
static const char* mode_target_freq_of(uint8_t idx) {
    switch(idx) {
    case JammerModeBleAdv:      return "2402/2426/2480 (BLE adv)";
    case JammerModeReactiveBle: return "2402/2426/2480 (BLE adv)";
    case JammerModeWifi1:       return "2405/2410/2414/2419 (WiFi ch1 pilots)";
    case JammerModeWifi6:       return "2430/2435/2439/2444 (WiFi ch6 pilots)";
    case JammerModeWifi11:      return "2455/2460/2464/2469 (WiFi ch11 pilots)";
    case JammerModeAllBand:     return "2400-2525 sweep";
    default:                    return NULL; /* CwCustom — 调用方推导 */
    }
}

static bool write_line(File* f, const char* s) {
    size_t len = strlen(s);
    return storage_file_write(f, s, (uint16_t)len) == len;
}

/* 文件名:jam_YYYY-MM-DD_HHMMSS_<mode>_<dur>s.csv.日期在前 → qFlipper 列表按名
 * 排序即按时间排序;文件名自带模式短名 + scene 时长,无需打开即知摘要.时间戳取
 * RTC-now ≈ 会话结束时刻(无会话起始墙钟可用).RTC 未设(year<2024)回退
 * jam_boot<tick>_<mode>_<dur>s.csv.同秒重名加 _1.._99 后缀保唯一. */
static void build_jammer_filename(
    Storage* storage,
    const DateTime* dt,
    uint8_t mode_index,
    uint32_t duration_ms,
    char* path,
    size_t path_sz) {
    const char* mode = mode_shortname_of(mode_index);
    unsigned long dur_s = (unsigned long)(duration_ms / 1000);
    char base[112];
    if(dt->year < 2024) {
        snprintf(
            base, sizeof(base), "%s/siggen_boot%010lu_%s_%lus", PQ_JAMMER_LOG_DIR,
            (unsigned long)furi_get_tick(), mode, dur_s);
    } else {
        snprintf(
            base, sizeof(base), "%s/siggen_%04u-%02u-%02u_%02u%02u%02u_%s_%lus",
            PQ_JAMMER_LOG_DIR, dt->year, dt->month, dt->day, dt->hour, dt->minute,
            dt->second, mode, dur_s);
    }
    snprintf(path, path_sz, "%s.csv", base);
    for(int i = 1; i <= 99 && storage_file_exists(storage, path); i++) {
        snprintf(path, path_sz, "%s_%d.csv", base, i);
    }
}

bool pq_jammer_log_session(
    uint8_t mode_index,
    uint8_t cw_channel,
    uint32_t start_ms,
    uint32_t reactive_jams) {
    const char* mode_name = mode_name_of(mode_index);
    const char* engine = mode_engine_of(mode_index);
    const char* target = mode_target_freq_of(mode_index);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/pingequa");
    storage_simply_mkdir(storage, PQ_JAMMER_LOG_DIR);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    uint32_t end_ms = furi_get_tick();
    uint32_t duration_ms = end_ms - start_ms;

    char path[128];
    build_jammer_filename(storage, &dt, mode_index, duration_ms, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    bool ok = false;
    char buf[160];

    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "open failed: %s", path);
            break;
        }

        /* 单 # 标题(便于人识别这是什么文件). */
        if(!write_line(file, "# PINGEQUA RF Lab Signal Generator Session\n")) break;

        /* 单一 key,value 段 — 无重复.pandas read_csv(comment='#') 直读. */
        if(!write_line(file, "key,value\n")) break;

        if(dt.year >= 2024) {
            snprintf(
                buf, sizeof(buf), "datetime,%04u-%02u-%02u %02u:%02u:%02u\n",
                dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        } else {
            snprintf(buf, sizeof(buf), "datetime,(RTC not set)\n");
        }
        if(!write_line(file, buf)) break;

        snprintf(buf, sizeof(buf), "mode,%s\n", mode_name);
        if(!write_line(file, buf)) break;

        snprintf(buf, sizeof(buf), "engine,%s\n", engine);
        if(!write_line(file, buf)) break;

        /* target_freq_mhz —— CwCustom 由 cw_channel 推导,其他模式查表. */
        if(target != NULL) {
            snprintf(buf, sizeof(buf), "target_freq_mhz,%s\n", target);
        } else {
            snprintf(
                buf, sizeof(buf), "target_freq_mhz,%u (ch%u)\n", 2400 + cw_channel,
                cw_channel);
        }
        if(!write_line(file, buf)) break;

        /* scene_duration_s —— 一位小数,诚实标注是 scene 停留时长(非 TX 时长). */
        uint32_t dur_int = duration_ms / 1000;
        uint32_t dur_frac = (duration_ms % 1000) / 100;
        snprintf(
            buf, sizeof(buf), "scene_duration_s,%lu.%lu\n", (unsigned long)dur_int,
            (unsigned long)dur_frac);
        if(!write_line(file, buf)) break;

        /* cw_channel + cw_freq_mhz —— 仅 CwCustom 模式有意义,其他模式不写
         * (避免重蹈 v0.5.0 把 cw_channel 对 WiFi 模式也写 "42" 的误导噪音). */
        if(mode_index == JammerModeCwCustom) {
            snprintf(buf, sizeof(buf), "cw_channel,%u\n", cw_channel);
            if(!write_line(file, buf)) break;
            snprintf(buf, sizeof(buf), "cw_freq_mhz,%u\n", 2400 + cw_channel);
            if(!write_line(file, buf)) break;
        }

        /* reactive_jams —— 仅 BLE React 模式有意义. */
        if(mode_index == JammerModeReactiveBle) {
            snprintf(
                buf, sizeof(buf), "reactive_events,%lu\n", (unsigned long)reactive_jams);
            if(!write_line(file, buf)) break;
        }

        ok = true;
        FURI_LOG_I(TAG, "logged session: mode=%s dur=%lums jams=%lu → %s",
                   mode_name, (unsigned long)duration_ms,
                   (unsigned long)reactive_jams, path);
    } while(0);

    if(!ok) FURI_LOG_E(TAG, "log write failed");

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
