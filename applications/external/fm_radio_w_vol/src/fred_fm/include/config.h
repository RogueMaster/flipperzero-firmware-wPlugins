/**
 * @file config.h
 * @brief Build flags, version strings, and RDS/capture sizing constants.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ENABLE_RDS                        1
#define ENABLE_ADC_CAPTURE                1
#define TAG                               "fred_fm"
#define FRED_FM_UI_VERSION                "0.11"
#define SETTINGS_FILE                     APP_DATA_PATH("settings.fff")
#define SETTINGS_FILETYPE                 "FReD FM Settings"
#define SETTINGS_VERSION                  (6U)
#define PRESETS_FILE                      APP_DATA_PATH("presets.fff")
#define PRESETS_FILETYPE                  "FReD FM Presets"
#define PRESETS_VERSION                   (1U)
#define PRESETS_MAX                       (32U)
#define PRESET_OFFSET_CENTIHZ_BIAS_CENTER (600U)
#define PRESET_OFFSET_BIAS_MIN            (0U)
#define PRESET_OFFSET_CENTIHZ_BIAS_MAX    (1200U)
#define RDS_MANUAL_OFFSET_MIN_CENTIHZ     (-600)
#define RDS_MANUAL_OFFSET_MAX_CENTIHZ     (600)
#define RDS_CONSTELLATION_HISTORY_LEN     (192U)
#define TEA_I2C_RDS_FAILURE_LIMIT         (8U)
#define RDS_CONSTELLATION_CENTER_X        (64)
#define RDS_CONSTELLATION_CENTER_Y        (36)
#define RDS_CONSTELLATION_PLOT_LEFT       (2)
#define RDS_CONSTELLATION_PLOT_RIGHT      (125)
#define RDS_CONSTELLATION_PLOT_TOP        (10)
#define RDS_CONSTELLATION_PLOT_BOTTOM     (62)
#define TEA_IF_COUNT_TARGET               0x37U
#define SEEK_MIN_INTERVAL_MS              2500
#define SEEK_SETTLE_DELAY_MS              250
#define SEEK_READY_TIMEOUT_MS             1800
#define SEEK_READY_POLL_MS                50
#ifdef ENABLE_RDS
#define RDS_ADC_FIXED_MIDPOINT         2048U
#define RDS_DSP_WORKER_STACK_SIZE      4096U
#define RDS_DSP_WORKER_FLAG_TICK       0x01U
#define RDS_DSP_WORKER_FLAG_STOP       0x02U
#define RDS_CAPTURE_FILE_TEMPLATE      APP_DATA_PATH("rds_capture_u16le_%04lu.raw")
#define RDS_CAPTURE_META_FILE_TEMPLATE APP_DATA_PATH("rds_capture_meta_%04lu.txt")
#define RDS_CAPTURE_PATH_MAX           128U
#define RDS_CAPTURE_MAX_INDEX          9999U
#define RDS_CAPTURE_SAMPLE_BYTES       sizeof(uint16_t)
#define RDS_CAPTURE_SAMPLE_BITS        16U
#define RDS_CAPTURE_SAMPLE_FORMAT      "u16le"
#define RDS_CAPTURE_TARGET_BLOCKS      512U
#define RDS_CAPTURE_TARGET_SAMPLES     (RDS_CAPTURE_TARGET_BLOCKS * RDS_ACQ_BLOCK_SAMPLES)
#define RDS_CAPTURE_TARGET_BYTES       (RDS_CAPTURE_TARGET_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES)
#define RDS_CAPTURE_RING_MIN_BLOCKS    8U
#define RDS_CAPTURE_RING_MAX_BLOCKS    128U
#define RDS_CAPTURE_HEAP_RESERVE_BYTES (16U * 1024U)
#define RDS_CAPTURE_WRITER_STACK_SIZE  2048U
#define RDS_CAPTURE_WRITER_PRIORITY    FuriThreadPriorityNormal
#define RDS_CAPTURE_WRITER_FLAG_WORK   (1U << 0)
#define RDS_CAPTURE_WRITER_FLAG_STOP   (1U << 1)
#endif
