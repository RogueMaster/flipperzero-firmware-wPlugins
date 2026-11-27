#pragma once
/*
 * fsd_tpms.h — tyre pressure read-out from 0x219 VCSEC_TPMSData, shared
 * header-only. Read-only: it decodes frames the car sends and builds none.
 *
 * Byte 0 says what a frame is. Two forms of the frame are known, told apart
 * by length:
 *
 *  5 bytes, the public Model 3 DBC: kinds 0..3, one sensor per frame.
 *     byte 1 pressure (0.025 bar)    byte 2 temperature (deg C + 40)
 *     byte 3 battery (0.01 V + 1.5)  byte 4 bits 0-2 location
 *     0xFF is "not available"; location 0 FL, 1 FR, 2 RL, 3 RR, 4 unknown.
 *     No capture of this form so far.
 *
 *  7 bytes, captured on a 2024 Model Y (#211, #237) and on the car behind
 *  the ukinora fork: kinds 0..5, one every 100 ms.
 *     kind 5   bytes 2..5 = the four pressures, 0.025 bar each, in the order
 *              FL, FR, RL, RR. On the fork's car they matched a gauge and
 *              moved between days, and letting air out of the driver's tyre
 *              moved byte 2 only. The order is from #237: a photo of the car's
 *              own tyre screen (2026-10-15) read FL 2.2, FR 2.7, RL 1.6,
 *              RR 2.3 bar while the dashboard showed bytes 2..5 as 2.15,
 *              2.68, 1.55, 2.25 bar: four different values, in that order.
 *              One car so far, a 2024 Model Y.
 *     kind 4   bytes 1..2 = 0x74 0x74 (2.90 bar) on both cars, unchanged over
 *              days: the car's target pair, presumably front / rear.
 *     kind 0-3 bytes 1..4 read 80 FF FF FF parked, and byte 1 stayed 0x80
 *              through a drive on the fork's car, so it is not a pressure in
 *              this form. Byte 5 bits 0-2 hold a different permutation of
 *              0..3 on each car, likely the sensor's corner. Not decoded
 *              until a capture shows a live reading in them.
 *
 * Any other length, and any kind a form does not have, is ignored: a number
 * read from the wrong byte looks just as plausible as a right one.
 *
 * The four slots hold one source at a time, so a slot never mixes two
 * orders. Per-sensor readings (5-byte form, with temperature and corner) win
 * while they are fresh; kind 5 fills the slots otherwise.
 *
 * Kept free of FSDState and platform code so the Flipper build can use it
 * too. The ESP32 build stores one FSDTpms in FSDState and feeds every 0x219
 * it receives, on any bus and in every mode.
 */

#include <stdbool.h>
#include <stdint.h>

#define FSD_TPMS_NA              0xFFu // not available (the DBC's SNA value)
#define FSD_TPMS_DLC_DBC         5u // per-sensor form
#define FSD_TPMS_DLC_EXT         7u // six-kind form
#define FSD_TPMS_KIND_SENSOR_MAX 3u // kinds 0..3: one sensor each
#define FSD_TPMS_KIND_TARGET     4u // 7-byte form: target pair
#define FSD_TPMS_KIND_ALL        5u // 7-byte form: all four pressures
#define FSD_TPMS_MBAR_PER_COUNT  25u // 0.025 bar
#define FSD_TPMS_TEMP_OFFSET_C   40
// No reading for this long and the values are shown as stale. The 7-byte form
// repeats every 600 ms; the 5-byte form's rate is unknown, hence the margin.
#define FSD_TPMS_STALE_MS        30000u

// pos[] values, the DBC's location table.
#define FSD_TPMS_POS_FL      0u
#define FSD_TPMS_POS_FR      1u
#define FSD_TPMS_POS_RL      2u
#define FSD_TPMS_POS_RR      3u
#define FSD_TPMS_POS_UNKNOWN 4u

// Which frames filled the slots.
#define FSD_TPMS_SRC_NONE   0u
#define FSD_TPMS_SRC_SENSOR 1u // 5-byte per-sensor frames, slot = sensor
#define FSD_TPMS_SRC_ALL    2u // 7-byte kind 5, slot = byte 2..5

// All-zero is the valid reset state (memset(0) in fsd_state_init): nothing
// seen. The first frame marks every value "not available" before using any.
typedef struct FSDTpms {
    uint8_t pressure[4]; // counts of 0.025 bar by slot; FSD_TPMS_NA = none
    uint8_t temp[4]; // deg C + 40 by slot; FSD_TPMS_NA = none
    uint8_t pos[4]; // FSD_TPMS_POS_* by slot
    uint8_t target[2]; // the car's target pair (kind 4); FSD_TPMS_NA = none
    uint8_t source; // FSD_TPMS_SRC_*
    uint8_t frame_dlc; // DLC of the last 0x219; 0 = none seen yet
    uint32_t last_ms; // ms clock of the last frame that carried a reading
} FSDTpms;

// What one frame gave. For tests and logs; the RX path ignores it.
typedef enum {
    FSD_TPMS_RX_IGNORED = 0, // nothing taken: unknown form or kind, or not preferred
    FSD_TPMS_RX_EMPTY, // a known frame, but the car has no value in it
    FSD_TPMS_RX_READING, // pressure(s) taken
    FSD_TPMS_RX_TARGET, // target pair taken
} FSDTpmsRx;

static inline bool fsd_tpms_seen(const FSDTpms* t) {
    return t->frame_dlc != 0u;
}

static inline void fsd_tpms_clear_slots(FSDTpms* t, uint8_t source) {
    for(unsigned i = 0; i < 4u; i++) {
        t->pressure[i] = FSD_TPMS_NA;
        t->temp[i] = FSD_TPMS_NA;
        t->pos[i] = FSD_TPMS_POS_UNKNOWN;
    }
    t->source = source;
}

static inline bool fsd_tpms_has_reading(const FSDTpms* t) {
    if(t->source == FSD_TPMS_SRC_NONE) return false;
    for(unsigned i = 0; i < 4u; i++)
        if(t->pressure[i] != FSD_TPMS_NA) return true;
    return false;
}

// ms since the last reading; 0 when there is none.
static inline uint32_t fsd_tpms_age_ms(const FSDTpms* t, uint32_t now_ms) {
    return fsd_tpms_has_reading(t) ? (uint32_t)(now_ms - t->last_ms) : 0u;
}

static inline bool fsd_tpms_stale(const FSDTpms* t, uint32_t now_ms) {
    return fsd_tpms_has_reading(t) && (uint32_t)(now_ms - t->last_ms) > FSD_TPMS_STALE_MS;
}

static inline unsigned fsd_tpms_mbar(uint8_t count) {
    return (unsigned)count * FSD_TPMS_MBAR_PER_COUNT;
}

static inline int fsd_tpms_temp_c(uint8_t raw) {
    return (int)raw - FSD_TPMS_TEMP_OFFSET_C;
}

// 5-byte form, kind 0..3 = slot.
static inline FSDTpmsRx
    fsd_tpms_sensor(FSDTpms* t, uint8_t slot, const uint8_t* d, uint32_t now_ms) {
    uint8_t p = d[1], temp = d[2], volt = d[3], loc = d[4] & 0x07u;
    // A sensor report brings temperature or battery along with the pressure.
    // A pressure byte with neither is a placeholder, like the 80 FF FF FF of
    // the 7-byte form.
    if(p == FSD_TPMS_NA || (temp == FSD_TPMS_NA && volt == FSD_TPMS_NA)) {
        // The car has nothing for this sensor now: stop showing the old value.
        if(t->source == FSD_TPMS_SRC_SENSOR) {
            t->pressure[slot] = FSD_TPMS_NA;
            t->temp[slot] = FSD_TPMS_NA;
        }
        return FSD_TPMS_RX_EMPTY;
    }
    if(t->source != FSD_TPMS_SRC_SENSOR) fsd_tpms_clear_slots(t, FSD_TPMS_SRC_SENSOR);
    t->pressure[slot] = p;
    t->temp[slot] = temp;
    t->pos[slot] = loc <= FSD_TPMS_POS_RR ? loc : FSD_TPMS_POS_UNKNOWN;
    t->last_ms = now_ms;
    return FSD_TPMS_RX_READING;
}

// Slots for kind 5: byte 2..5 = FL, FR, RL, RR, empty until a reading lands.
static inline void fsd_tpms_all_slots(FSDTpms* t) {
    fsd_tpms_clear_slots(t, FSD_TPMS_SRC_ALL);
    t->pos[0] = FSD_TPMS_POS_FL;
    t->pos[1] = FSD_TPMS_POS_FR;
    t->pos[2] = FSD_TPMS_POS_RL;
    t->pos[3] = FSD_TPMS_POS_RR;
}

// 7-byte form, kind 5.
static inline FSDTpmsRx fsd_tpms_all(FSDTpms* t, const uint8_t* d, uint32_t now_ms) {
    uint8_t v[4];
    bool any = false;
    for(unsigned i = 0; i < 4u; i++) {
        // 0 is how these frames say "nothing" (kind 4 pads with it), and a tyre
        // at 0 bar is flat anyway; FF is not available.
        v[i] = d[2 + i] == 0u ? FSD_TPMS_NA : d[2 + i];
        if(v[i] != FSD_TPMS_NA) any = true;
    }
    if(!any) {
        if(t->source == FSD_TPMS_SRC_ALL) fsd_tpms_all_slots(t);
        return FSD_TPMS_RX_EMPTY;
    }
    if(t->source == FSD_TPMS_SRC_SENSOR && fsd_tpms_has_reading(t) && !fsd_tpms_stale(t, now_ms))
        return FSD_TPMS_RX_IGNORED;
    fsd_tpms_all_slots(t);
    for(unsigned i = 0; i < 4u; i++)
        t->pressure[i] = v[i];
    t->last_ms = now_ms;
    return FSD_TPMS_RX_READING;
}

// 7-byte form, kind 4.
static inline FSDTpmsRx fsd_tpms_target(FSDTpms* t, const uint8_t* d) {
    for(unsigned i = 0; i < 2u; i++)
        t->target[i] = d[1 + i] == 0u ? FSD_TPMS_NA : d[1 + i];
    return (t->target[0] == FSD_TPMS_NA && t->target[1] == FSD_TPMS_NA) ? FSD_TPMS_RX_EMPTY :
                                                                          FSD_TPMS_RX_TARGET;
}

// Feed one received 0x219. Pure: changes only *t.
static inline FSDTpmsRx
    fsd_tpms_update(FSDTpms* t, const uint8_t* d, uint8_t dlc, uint32_t now_ms) {
    if(!t || !d || dlc == 0u) return FSD_TPMS_RX_IGNORED;
    if(t->frame_dlc == 0u) {
        fsd_tpms_clear_slots(t, FSD_TPMS_SRC_NONE);
        t->target[0] = t->target[1] = FSD_TPMS_NA;
    }
    t->frame_dlc = dlc;
    uint8_t kind = d[0];
    if(dlc == FSD_TPMS_DLC_DBC && kind <= FSD_TPMS_KIND_SENSOR_MAX)
        return fsd_tpms_sensor(t, kind, d, now_ms);
    if(dlc == FSD_TPMS_DLC_EXT && kind == FSD_TPMS_KIND_ALL) return fsd_tpms_all(t, d, now_ms);
    if(dlc == FSD_TPMS_DLC_EXT && kind == FSD_TPMS_KIND_TARGET) return fsd_tpms_target(t, d);
    return FSD_TPMS_RX_IGNORED;
}
