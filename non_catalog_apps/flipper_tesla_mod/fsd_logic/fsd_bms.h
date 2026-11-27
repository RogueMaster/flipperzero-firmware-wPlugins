#pragma once
/*
 * fsd_bms.h — read-only battery decoders, shared header-only by the Flipper and
 * ESP32 builds (the ESP32 build compiles no fsd_logic source files).
 *
 * 0x312 BMS_thermalStatus pack temperature: byte4 - 40 / byte5 - 40 deg C is not
 * from any DBC. Tesla's signal list puts BMS_minPackTemperature and
 * BMS_maxPackTemperature in this frame behind BMS_thermalStatusMultiplexer, at
 * positions not known here. On the #211 Model Y the decode read 206~8 and
 * 205~-32 deg C (shown wrapped to int8 as -50~8 and -51~-32) on an autumn day,
 * and 24~101 on the one 0x312 in its dump. So the layout is kept, but a reading
 * is stored only when it is plausible for one pack; the last plausible one is
 * kept with its age.
 *
 * 0x33A UI_range: the SOC the car shows on its own screen, bit 20, 7 bits,
 * plain percent (#251, from ukinora's fork). In no DBC; pinned by measurement.
 * It is not 0x292's BMS figure: the two move together with a gap of 0.8-2.9 %
 * that varies, so neither is derived from the other. Bit 27 is set on some
 * frames, so the mask must stay 7 bits; above 100 is refused (SNA or a moved
 * field). #211 Model Y: D2 34 04 7D FE FF 13 63 -> 80 % beside 0x292's 81.8 %.
 *
 * 0x292 BMS_socStatus: four 10-bit little-endian fields, 0.1 % each, in Tesla's
 * signal list (talas9/tesla_can_signals, Model 3 and Model Y; no opendbc DBC has
 * the frame): BMS_socMin 0|10, BMS_socUI 10|10, BMS_socMax 20|10, BMS_socAvg
 * 30|10. soc_percent is BMS_socUI, range 0-100, the BMS figure meant for
 * display. The other three run to 102.2 and read above it as the pack empties
 * (ukinora's fork, 31 captures: min <= avg <= max in every one, socUI lower by
 * up to 4.3 % at 45 %), so of the four socUI is the nearest to the car's screen
 * (0x33A), but still not that number. #211 Model Y: 3D CB 5C B4 D0 01 00 00 ->
 * min 82.9, UI 81.8, max 83.7, avg 83.4, with 0x33A at 80. The ESP32 has read
 * socUI since 109f3b2; the Flipper read socMin up to v2.17-beta.2.
 */

#include "fsd_state.h"
#include <stdbool.h>
#include <stdint.h>

#define FSD_BMS_THERMAL_MIN_DLC 6u
#define FSD_BMS_TEMP_MIN_BYTE   4u
#define FSD_BMS_TEMP_MAX_BYTE   5u
#define FSD_BMS_TEMP_OFFSET     40
// Plausibility bounds, heuristic (not from a DBC):
//  -40 deg C: the automotive cold-soak floor; colder is another signal.
//  +70 deg C: well past where a pack is allowed to run; hotter is another signal.
//  20 deg C spread: the coldest and warmest module of one pack sit within a few
//  degrees, leaving room for heating / fast-charge gradients; wider is not one pack.
#define FSD_BMS_TEMP_LOW_C      (-40)
#define FSD_BMS_TEMP_HIGH_C     70
#define FSD_BMS_TEMP_SPREAD_C   20
// A plausible reading older than this is stale (frames stopped, or every one
// since was rejected): no read-out, no cold/hot advisory.
#define FSD_BMS_TEMP_STALE_MS   30000u

static inline bool fsd_bms_temp_plausible(int min_c, int max_c) {
    return min_c >= FSD_BMS_TEMP_LOW_C && max_c <= FSD_BMS_TEMP_HIGH_C && min_c <= max_c &&
           (max_c - min_c) <= FSD_BMS_TEMP_SPREAD_C;
}

// Parse one 0x312. Marks the BMS seen; stores min/max only when plausible,
// otherwise counts the frame in batt_temp_rejected and keeps the last good one.
static inline void
    fsd_bms_thermal_update(FSDState* s, const uint8_t* d, uint8_t dlc, uint32_t now_ms) {
    if(dlc < FSD_BMS_THERMAL_MIN_DLC) return;
    int tmin = (int)d[FSD_BMS_TEMP_MIN_BYTE] - FSD_BMS_TEMP_OFFSET;
    int tmax = (int)d[FSD_BMS_TEMP_MAX_BYTE] - FSD_BMS_TEMP_OFFSET;
    s->bms_seen = true;
    if(!fsd_bms_temp_plausible(tmin, tmax)) {
        s->batt_temp_rejected++;
        return;
    }
    s->batt_temp_min_c = (int8_t)tmin;
    s->batt_temp_max_c = (int8_t)tmax;
    s->batt_temp_ok = true;
    s->batt_temp_ms = now_ms;
}

// True while a plausible reading is at most FSD_BMS_TEMP_STALE_MS old.
static inline bool fsd_bms_temp_fresh(const FSDState* s, uint32_t now_ms) {
    return s->batt_temp_ok && (uint32_t)(now_ms - s->batt_temp_ms) <= FSD_BMS_TEMP_STALE_MS;
}

#define FSD_BMS_SOC_MIN_BIT    0u
#define FSD_BMS_SOC_UI_BIT     10u
#define FSD_BMS_SOC_MAX_BIT    20u
#define FSD_BMS_SOC_AVG_BIT    30u
#define FSD_BMS_SOC_MIN_DLC    3u // socUI ends in byte 2
#define FSD_BMS_SOC_UI_MAX_X10 1000u // 100.0 %

// Raw 10-bit field (0.1 %) of one 0x292 at FSD_BMS_SOC_*_BIT. None of the four
// spans more than two bytes; the caller checks the frame holds them.
static inline uint16_t fsd_bms_soc_field(const uint8_t* d, unsigned bit) {
    uint16_t w = (uint16_t)(d[bit / 8u] | ((uint16_t)d[bit / 8u + 1u] << 8));
    return (uint16_t)((w >> (bit % 8u)) & 0x3FFu);
}

// BMS_socUI in 0.1 % from one 0x292. False (and *x10 untouched) for a short
// frame or a value above 100 %.
static inline bool fsd_bms_decode_soc(const uint8_t* d, uint8_t dlc, uint16_t* x10) {
    if(dlc < FSD_BMS_SOC_MIN_DLC) return false;
    uint16_t raw = fsd_bms_soc_field(d, FSD_BMS_SOC_UI_BIT);
    if(raw > FSD_BMS_SOC_UI_MAX_X10) return false;
    *x10 = raw;
    return true;
}

// Parse one 0x292 into soc_percent and mark the BMS seen. A refused frame
// changes nothing.
static inline void fsd_bms_soc_update(FSDState* s, const uint8_t* d, uint8_t dlc) {
    uint16_t x10;
    if(!fsd_bms_decode_soc(d, dlc, &x10)) return;
    s->soc_percent = x10 * 0.1f;
    s->bms_seen = true;
}

#define FSD_BMS_UI_SOC_MIN_DLC 4u
#define FSD_BMS_UI_SOC_MAX     100u

// Decode the car-displayed SOC from one 0x33A. False (and *pct untouched) for a
// short frame or a value above 100.
static inline bool fsd_bms_decode_ui_soc(const uint8_t* d, uint8_t dlc, uint8_t* pct) {
    if(dlc < FSD_BMS_UI_SOC_MIN_DLC) return false;
    uint8_t raw = (uint8_t)(((d[2] >> 4) | ((uint16_t)d[3] << 4)) & 0x7Fu);
    if(raw > FSD_BMS_UI_SOC_MAX) return false;
    *pct = raw;
    return true;
}

// Parse one 0x33A into ui_soc / ui_soc_seen. Read-only; soc_percent (0x292) is
// left alone.
static inline void fsd_bms_ui_soc_update(FSDState* s, const uint8_t* d, uint8_t dlc) {
    uint8_t pct;
    if(!fsd_bms_decode_ui_soc(d, dlc, &pct)) return;
    s->ui_soc = pct;
    s->ui_soc_seen = true;
}
