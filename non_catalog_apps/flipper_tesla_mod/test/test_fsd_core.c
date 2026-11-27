/*
 * test_fsd_core.c — host unit tests for the Tesla FSD protocol core.
 *
 * Compiles fsd_logic/fsd_handler.c on the host (no Flipper SDK, no SPI) and
 * checks the bit-packing, mux dispatch, HW3/HW4 branching, checksum, and
 * signal-parsing behavior with INDEPENDENT oracles (the expected checksum is
 * recomputed here, not snapshotted from the implementation).
 *
 * Purpose: lock the current behavior before the protocol core is converged
 * into a single shared file. A frame the car silently rejects (wrong
 * checksum / wrong bit) is the exact failure mode that shipped as the v2.14
 * HW3 regression — these tests turn that into a red CI build.
 *
 * Build + run:  make -C test check
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fsd_can_ops.h"
#include "fsd_blackbox_filter.h"
#include "fsd_bms.h"
#include "fsd_blackbox_summary.h"
#include "fsd_capability.h"
#include "fsd_capture.h"
#include "fsd_checksum.h"
#include "fsd_events.h"
#include "fsd_handler.h"
#include "fsd_power.h"
#include "fsd_profile.h"
#include "fsd_profile_db.h"
#include "fsd_steer.h"
#include "fsd_tpms.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if(cond) {                                        \
            g_pass++;                                     \
        } else {                                          \
            g_fail++;                                     \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while(0)

static void zero(CANFRAME* f) {
    memset(f, 0, sizeof(*f));
}

// ── bit-packing primitive ───────────────────────────────────────────────────
static void test_set_bit(void) {
    CANFRAME f;
    zero(&f);
    fsd_set_bit(&f, 46, true); // 46/8=5, 46%8=6 -> 0x40
    CHECK(f.buffer[5] == 0x40, "bit46 -> buffer[5]=0x%02X exp 0x40", f.buffer[5]);
    fsd_set_bit(&f, 46, false);
    CHECK(f.buffer[5] == 0x00, "bit46 clear -> buffer[5]=0x%02X exp 0x00", f.buffer[5]);
    fsd_set_bit(&f, 60, true); // 60/8=7, 60%8=4 -> 0x10
    CHECK(f.buffer[7] == 0x10, "bit60 -> buffer[7]=0x%02X exp 0x10", f.buffer[7]);

    CANFRAME g;
    zero(&g);
    fsd_set_bit(&g, 64, true); // out of range
    fsd_set_bit(&g, -1, true);
    int all_zero = 1;
    for(int i = 0; i < 8; i++)
        if(g.buffer[i]) all_zero = 0;
    CHECK(all_zero, "out-of-range bit must be a no-op");
}

// ── mux id ────────────────────────────────────────────────────────────────────
static void test_read_mux(void) {
    CANFRAME f;
    zero(&f);
    f.buffer[0] = 0x05;
    CHECK(fsd_read_mux_id(&f) == 5, "mux 5 got %u", fsd_read_mux_id(&f));
    f.buffer[0] = 0x0F; // upper bits must be masked off
    CHECK(fsd_read_mux_id(&f) == 7, "mux mask 0x07 got %u", fsd_read_mux_id(&f));
}

// ── UI FSD-selected flag ──────────────────────────────────────────────────────
static void test_is_selected(void) {
    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[4] = 0x40; // byte4 bit6
    CHECK(fsd_is_selected_in_ui(&f, false) == true, "bit6 set -> selected");
    f.buffer[4] = 0x00;
    CHECK(fsd_is_selected_in_ui(&f, false) == false, "bit6 clear -> not selected");
    CHECK(fsd_is_selected_in_ui(&f, true) == true, "force_fsd overrides UI flag");
    f.data_lenght = 4;
    CHECK(fsd_is_selected_in_ui(&f, false) == false, "dlc<5 guard -> not selected");
}

// ── HW version detection from 0x398 ───────────────────────────────────────────
static void test_detect_hw(void) {
    CANFRAME f;
    zero(&f);
    f.canId = CAN_ID_GTW_CAR_CONFIG;
    f.data_lenght = 8;
    f.buffer[0] = 0x80; // bits7:6 = 0b10 = 2
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_HW3, "das_hw=2 -> HW3");
    f.buffer[0] = 0xC0; // 0b11 = 3
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_HW4, "das_hw=3 -> HW4");
    // Real Legacy car: das_hw=0/1 but GTW_carConfig is populated (VIN/options),
    // so a non-zero config byte distinguishes it from the all-zero stub below.
    f.buffer[3] = 0x55; // some real config payload
    f.buffer[0] = 0x00; // 0
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_Legacy, "das_hw=0 + payload -> Legacy");
    f.buffer[0] = 0x40; // 1
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_Legacy, "das_hw=1 + payload -> Legacy");
    // All-zero 0x398 stub (HW4 Juniper/Giga gateway copy on Bus 6): must NOT be
    // read as das_hw=0 -> Legacy; fall through to live markers instead.
    zero(&f);
    f.canId = CAN_ID_GTW_CAR_CONFIG;
    f.data_lenght = 8;
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_Unknown, "all-zero 0x398 stub -> Unknown");
    // Guard boundary: a populated frame with das_hw=0 (byte0 low bits set, e.g.
    // a real config/mux value) is NOT all-zero, so it still classifies Legacy —
    // the guard must only fire on a fully empty payload, never over-fire.
    f.buffer[0] = 0x05; // byte0 nonzero, das_hw bits (7:6) = 0
    CHECK(
        fsd_detect_hw_version(&f) == TeslaHW_Legacy,
        "das_hw=0 populated -> Legacy (guard no over-fire)");
    f.canId = 0x123; // wrong id
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_Unknown, "wrong id -> Unknown");
}

// ── follow distance -> speed profile ──────────────────────────────────────────
static void test_follow_distance(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CANFRAME f;
    zero(&f);
    f.data_lenght = 6;

    s.hw_version = TeslaHW_HW3;
    f.buffer[5] = (uint8_t)(1u << 5);
    fsd_handle_follow_distance(&s, &f);
    CHECK(s.speed_profile == 2, "HW3 fd1 -> profile2 got %d", s.speed_profile);
    CHECK(
        s.follow_distance_seen && s.follow_distance == 1,
        "fd read-out 1 got %u",
        s.follow_distance);
    f.buffer[5] = (uint8_t)(3u << 5);
    fsd_handle_follow_distance(&s, &f);
    CHECK(s.speed_profile == 0, "HW3 fd3 -> profile0 got %d", s.speed_profile);

    s.hw_version = TeslaHW_HW4;
    f.buffer[5] = (uint8_t)(5u << 5);
    fsd_handle_follow_distance(&s, &f);
    CHECK(s.speed_profile == 4, "HW4 fd5 -> profile4 got %d", s.speed_profile);
    f.buffer[5] = (uint8_t)(1u << 5);
    fsd_handle_follow_distance(&s, &f);
    CHECK(s.speed_profile == 3, "HW4 fd1 -> profile3 got %d", s.speed_profile);

    // speed_profile_locked keeps the profile but the read-out still follows the stalk.
    s.speed_profile_locked = true;
    f.buffer[5] = (uint8_t)(4u << 5);
    fsd_handle_follow_distance(&s, &f);
    CHECK(s.speed_profile == 3 && s.follow_distance == 4, "locked: profile 3 kept, fd read 4");
}

// ── 0x3FD autopilot frame, HW4 ────────────────────────────────────────────────
static void test_autopilot_hw4(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW4;
    s.force_fsd = true;
    s.speed_profile = 4;

    // mux0 -> FSD activation bits 46 + 60
    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0;
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW4 mux0 reports modified");
    CHECK((f.buffer[5] & 0x40) != 0, "HW4 mux0 bit46 set");
    CHECK((f.buffer[7] & 0x10) != 0, "HW4 mux0 bit60 set");
    CHECK(s.fsd_enabled, "HW4 mux0 sets fsd_enabled");

    // mux1 -> nag bit19 cleared; bit47 left alone (summon_unlock off)
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1;
    f.buffer[2] = 0x08; // pre-set bit19 so we prove it is cleared
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW4 mux1 reports modified");
    CHECK((f.buffer[2] & 0x08) == 0, "HW4 mux1 bit19 cleared");
    CHECK((f.buffer[5] & 0x80) == 0, "HW4 mux1 bit47 not set without summon_unlock");

    // mux2 -> speed profile written to byte7 bits 6:4 (frame bits 60-62)
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 2;
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW4 mux2 reports modified");
    CHECK(
        ((f.buffer[7] >> 4) & 0x07) == 4,
        "HW4 mux2 speed_profile=4 got %u",
        (f.buffer[7] >> 4) & 0x07);
}

// ── 0x3FD mux2 HW4 speed profile layout (#59) ────────────────────────────────
// Real 0x3FD mux2 payload from HW4 cars (hw4_unbanned_* dumps and the Palladium
// Plaid captures in #18): byte7 is 0x90 on every frame. Bit 63 is set on every
// mux of 0x3FD (mux0 0x80/0x88, mux1 0x80, mux2 0x90) — it is the per-mux valid
// flag, not part of the profile. The profile is bits 60-62. Writing it at <<5
// cleared bit 63 for profiles 0-3 and voided the whole mux2 frame, including
// the speed offset carried in byte1.
static void test_hw4_mux2_profile_layout(void) {
    static const uint8_t k_hw4_mux2[8] = {0x02, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90};
    FSDState s;
    CANFRAME f;

    for(int p = 0; p <= 4; p++) {
        memset(&s, 0, sizeof(s));
        s.hw_version = TeslaHW_HW4;
        s.speed_profile = p;
        zero(&f);
        f.data_lenght = 8;
        memcpy(f.buffer, k_hw4_mux2, 8);
        CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "#59 p%d: HW4 mux2 modified", p);
        CHECK(
            ((f.buffer[7] >> 4) & 0x07) == p,
            "#59 p%d: profile in bits 60-62, got %u",
            p,
            (f.buffer[7] >> 4) & 0x07);
        CHECK(
            (f.buffer[7] & 0x80) != 0,
            "#59 p%d: bit63 (mux2 valid) kept, byte7=0x%02X",
            p,
            f.buffer[7]);
        CHECK(
            (f.buffer[7] & 0x0F) == 0x00,
            "#59 p%d: byte7 low nibble untouched, byte7=0x%02X",
            p,
            f.buffer[7]);
        CHECK(memcmp(f.buffer, k_hw4_mux2, 7) == 0, "#59 p%d: bytes 0-6 untouched (no offset)", p);
    }

    // Offset override rides in the same frame: byte1 bits 5:0, top bits and byte0 kept.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW4;
    s.speed_profile = 1;
    s.hw4_offset = 10;
    zero(&f);
    f.data_lenght = 8;
    memcpy(f.buffer, k_hw4_mux2, 8);
    f.buffer[1] |= 0xC0; // prove bits 7:6 of byte1 survive
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "#59 offset: HW4 mux2 modified");
    CHECK((f.buffer[1] & 0x3F) == 10, "#59 offset: byte1[5:0]=10 got %u", f.buffer[1] & 0x3F);
    CHECK((f.buffer[1] & 0xC0) == 0xC0, "#59 offset: byte1[7:6] kept");
    CHECK(f.buffer[0] == 0x02, "#59 offset: byte0 untouched got 0x%02X", f.buffer[0]);
    CHECK(
        f.buffer[7] == 0x90,
        "#59 offset: byte7 = 0x90 (valid + profile 1) got 0x%02X",
        f.buffer[7]);
}

// ── 0x3FD autopilot frame, HW3 (#209) ─────────────────────────────────────────
// Default is pass-through: mux0 gets bit46 only, the car's own FSD profile (mux0
// bits 49-50) and the whole mux2 frame (FSD max-speed offset, bits 6-13) are left
// as received and mux2 is not re-sent. hw3_speed_override restores the legacy
// write byte-for-byte: follow-distance profile into bits 49-50 and the Autopilot
// offset (mux0 bits 25-30, (raw-30)*5, clamped 0-100) into mux2 bits 6-13.
//
// Car frames: mux0 AP offset raw 35 (byte3 0x46 -> 25), profile 1 (byte6 0x83,
// other byte6 bits set to prove they survive), byte7 0x80 valid flag; mux2 FSD
// offset raw 46 (byte0 bits 6-7 = 2, byte1 low6 = 11), byte1 top bits set.
static const uint8_t k_hw3_mux0[8] = {0x00, 0x11, 0x22, 0x46, 0x00, 0x00, 0x83, 0x80};
static const uint8_t k_hw3_mux2[8] = {0x82, 0xCB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};

static void hw3_frame(CANFRAME* f, const uint8_t b[8]) {
    zero(f);
    f->canId = CAN_ID_AP_CONTROL;
    f->data_lenght = 8;
    memcpy(f->buffer, b, 8);
}

static void test_autopilot_hw3(void) {
    FSDState s;
    CANFRAME f;

    // ── default: pass-through ──
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    s.force_fsd = true;
    s.speed_profile = 2;

    hw3_frame(&f, k_hw3_mux0);
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW3 mux0 reports modified (bit46)");
    CHECK((f.buffer[5] & 0x40) != 0, "HW3 mux0 bit46 set");
    CHECK(
        f.buffer[6] == 0x83,
        "HW3 default: byte6 (profile bits 49-50) untouched, got 0x%02X",
        f.buffer[6]);
    static const uint8_t k_mux0_bit46[8] = {0x00, 0x11, 0x22, 0x46, 0x00, 0x40, 0x83, 0x80};
    CHECK(memcmp(f.buffer, k_mux0_bit46, 8) == 0, "HW3 default: mux0 = car frame + bit46 only");
    CHECK(
        s.speed_offset == 25, "HW3 AP offset still read: speed_offset=%d exp 25", s.speed_offset);
    CHECK(
        s.hw3_profile_seen && s.hw3_car_profile == 1 && s.hw3_sent_profile == 1,
        "HW3 default read-out: car profile 1 -> sent 1 (got %u -> %u)",
        s.hw3_car_profile,
        s.hw3_sent_profile);

    hw3_frame(&f, k_hw3_mux2);
    CHECK(!fsd_handle_autopilot_frame(&s, &f, 0), "HW3 default: mux2 NOT modified");
    CHECK(memcmp(f.buffer, k_hw3_mux2, 8) == 0, "HW3 default: mux2 payload untouched");
    CHECK(
        s.hw3_offset_seen && s.hw3_car_offset == 46 && s.hw3_sent_offset == 46,
        "HW3 default read-out: car offset 46 -> sent 46 (got %u -> %u)",
        s.hw3_car_offset,
        s.hw3_sent_offset);

    // Old test inputs (zero frame, profile 2): bit46 only, no profile written.
    zero(&f);
    f.data_lenght = 8;
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW3 default zero mux0 modified");
    CHECK(f.buffer[5] == 0x40 && f.buffer[6] == 0x00, "HW3 default zero mux0: bit46 only");

    // ── hw3_speed_override: legacy bytes ──
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    s.force_fsd = true;
    s.speed_profile = 2;
    s.hw3_speed_override = true;

    // Old test inputs: zero mux0, profile 2 -> byte5 0x40, byte6 0x04.
    zero(&f);
    f.data_lenght = 8;
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW3 override zero mux0 modified");
    static const uint8_t k_zero_legacy[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x04, 0x00};
    CHECK(memcmp(f.buffer, k_zero_legacy, 8) == 0, "HW3 override zero mux0 = legacy bytes");

    hw3_frame(&f, k_hw3_mux0);
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW3 override mux0 modified");
    static const uint8_t k_mux0_legacy[8] = {0x00, 0x11, 0x22, 0x46, 0x00, 0x40, 0x85, 0x80};
    CHECK(
        memcmp(f.buffer, k_mux0_legacy, 8) == 0,
        "HW3 override mux0 = legacy bytes (byte6 0x%02X exp 0x85)",
        f.buffer[6]);
    CHECK(
        s.hw3_car_profile == 1 && s.hw3_sent_profile == 2,
        "HW3 override read-out: profile 1 -> 2 (got %u -> %u)",
        s.hw3_car_profile,
        s.hw3_sent_profile);

    hw3_frame(&f, k_hw3_mux2);
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW3 override mux2 modified");
    // speed_offset 25: byte0 bits 6-7 = 25&3 = 1, byte1 low6 = 25>>2 = 6.
    static const uint8_t k_mux2_legacy[8] = {0x42, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
    CHECK(
        memcmp(f.buffer, k_mux2_legacy, 8) == 0,
        "HW3 override mux2 = legacy bytes (got %02X %02X)",
        f.buffer[0],
        f.buffer[1]);
    CHECK(
        s.hw3_car_offset == 46 && s.hw3_sent_offset == 25,
        "HW3 override read-out: offset 46 -> 25 (got %u -> %u)",
        s.hw3_car_offset,
        s.hw3_sent_offset);

    // Follow distance -> profile -> mux0 bits 49-50, override only.
    CANFRAME fd;
    zero(&fd);
    fd.data_lenght = 6;
    fd.buffer[5] = (uint8_t)(3u << 5); // fd3 -> profile 0
    fsd_handle_follow_distance(&s, &fd);
    hw3_frame(&f, k_hw3_mux0);
    fsd_handle_autopilot_frame(&s, &f, 0);
    CHECK(((f.buffer[6] >> 1) & 0x03) == 0, "HW3 override: fd3 -> profile 0 written");
    s.hw3_speed_override = false;
    hw3_frame(&f, k_hw3_mux0);
    fsd_handle_autopilot_frame(&s, &f, 0);
    CHECK(((f.buffer[6] >> 1) & 0x03) == 1, "HW3 default: fd3 ignored, car profile 1 kept");

    // Held by a gate (AP-First, AP not engaged): nothing written, read-out still tracks.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    s.force_fsd = true;
    s.ap_first = true;
    hw3_frame(&f, k_hw3_mux2);
    CHECK(!fsd_handle_autopilot_frame(&s, &f, 0), "HW3 ap_first hold: not modified");
    CHECK(
        s.hw3_offset_seen && s.hw3_car_offset == 46 && s.hw3_sent_offset == 46,
        "HW3 ap_first hold: read-out still updated");
}

// ── #209 regression guard: hw3_speed_override never touches HW4 ──────────────
static void test_hw4_ignores_hw3_override(void) {
    static const uint8_t k_hw4_mux0[8] = {0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x83, 0x80};
    static const uint8_t k_hw4_mux2[8] = {0x02, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90};
    static const uint8_t k_hw4_mux0_out[8] = {0x00, 0x00, 0x00, 0x46, 0x00, 0x40, 0x83, 0x90};
    static const uint8_t k_hw4_mux2_out[8] = {0x02, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0};
    for(int ov = 0; ov <= 1; ov++) {
        FSDState s;
        memset(&s, 0, sizeof(s));
        s.hw_version = TeslaHW_HW4;
        s.force_fsd = true;
        s.speed_profile = 4;
        s.hw3_speed_override = (ov == 1);
        CANFRAME f;
        hw3_frame(&f, k_hw4_mux0);
        CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW4 ov%d mux0 modified", ov);
        CHECK(memcmp(f.buffer, k_hw4_mux0_out, 8) == 0, "HW4 ov%d mux0 = bit46+bit60 only", ov);
        hw3_frame(&f, k_hw4_mux2);
        CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW4 ov%d mux2 modified", ov);
        CHECK(
            memcmp(f.buffer, k_hw4_mux2_out, 8) == 0,
            "HW4 ov%d mux2 = profile 4 in bits 60-62",
            ov);
        CHECK(!s.hw3_profile_seen && !s.hw3_offset_seen, "HW4 ov%d: HW3 read-out untouched", ov);
    }
}

// ── Summon EU Unlock (0x3FD mux1 bit47) — opt-in, HW3 + HW4 ───────────────────
// ev-open-can-tools summon-eu-unlock: clear bit19, set bit47 on mux1. This test
// locks that summon_unlock gates bit47 on both HW3 and HW4, and that mux1 with
// the toggle OFF still clears bit19 and never touches bit47.
static void test_summon_unlock(void) {
    CANFRAME f;

    // ── HW3: bit47 is gated by summon_unlock ──
    FSDState h3;
    memset(&h3, 0, sizeof(h3));
    h3.hw_version = TeslaHW_HW3;

    // summon OFF: mux1 clears bit19, does NOT set bit47 (prior HW3 behavior).
    h3.summon_unlock = false;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1; // mux1
    f.buffer[2] = 0x08; // pre-set bit19 to prove it is cleared
    CHECK(fsd_handle_autopilot_frame(&h3, &f, 0), "HW3 mux1 summon-off reports modified");
    CHECK((f.buffer[2] & 0x08) == 0, "HW3 mux1 summon-off bit19 cleared");
    CHECK((f.buffer[5] & 0x80) == 0, "HW3 mux1 summon-off bit47 NOT set");
    CHECK(h3.nag_suppressed, "HW3 mux1 summon-off still sets nag_suppressed");

    // summon ON: mux1 now also sets bit47.
    h3.summon_unlock = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1;
    CHECK(fsd_handle_autopilot_frame(&h3, &f, 0), "HW3 mux1 summon-on reports modified");
    CHECK((f.buffer[5] & 0x80) != 0, "HW3 mux1 summon-on bit47 set");

    // ── HW4: bit47 is gated by summon_unlock too ──
    FSDState h4;
    memset(&h4, 0, sizeof(h4));
    h4.hw_version = TeslaHW_HW4;

    h4.summon_unlock = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1;
    CHECK(fsd_handle_autopilot_frame(&h4, &f, 0), "HW4 mux1 summon-on reports modified");
    CHECK((f.buffer[5] & 0x80) != 0, "HW4 mux1 summon-on bit47 set");

    // summon OFF: mux1 still clears bit19 but leaves bit47 alone.
    h4.summon_unlock = false;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1;
    f.buffer[2] = 0x08;
    CHECK(fsd_handle_autopilot_frame(&h4, &f, 0), "HW4 mux1 summon-off reports modified");
    CHECK((f.buffer[2] & 0x08) == 0, "HW4 mux1 summon-off bit19 cleared");
    CHECK((f.buffer[5] & 0x80) == 0, "HW4 mux1 summon-off bit47 NOT set");
    CHECK(h4.nag_suppressed, "HW4 mux1 summon-off still sets nag_suppressed");
}

// ── Continue on Green (0x3FD mux0 bit39) — opt-in, HW3 + HW4 ──────────────────
// ev-open-can-tools TSLLC plugin sets UI_fsdContinueOnGreenWithCIPV (bit39) on
// mux0. It is gated by continue_on_green + fsd_enabled and applies before the
// HW3/HW4 split, so bit39 must appear for both hardware versions when ON, and
// never when the toggle is OFF. bit39 -> byte 4 bit 7 -> mask 0x80.
static void test_continue_on_green(void) {
    CANFRAME f;

    // ── HW3 ──
    FSDState h3;
    memset(&h3, 0, sizeof(h3));
    h3.hw_version = TeslaHW_HW3;
    h3.force_fsd = true; // makes mux0 select FSD -> fsd_enabled = true

    // ON: bit39 set on mux0.
    h3.continue_on_green = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0; // mux0
    CHECK(fsd_handle_autopilot_frame(&h3, &f, 0), "HW3 mux0 cog-on reports modified");
    CHECK(h3.fsd_enabled, "HW3 mux0 sets fsd_enabled");
    CHECK((f.buffer[4] & 0x80) != 0, "HW3 mux0 cog-on bit39 set");

    // OFF: bit39 NOT set.
    h3.continue_on_green = false;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0;
    fsd_handle_autopilot_frame(&h3, &f, 0);
    CHECK((f.buffer[4] & 0x80) == 0, "HW3 mux0 cog-off bit39 NOT set");

    // ── HW4 ──
    FSDState h4;
    memset(&h4, 0, sizeof(h4));
    h4.hw_version = TeslaHW_HW4;
    h4.force_fsd = true;

    // ON: bit39 set on mux0.
    h4.continue_on_green = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0;
    CHECK(fsd_handle_autopilot_frame(&h4, &f, 0), "HW4 mux0 cog-on reports modified");
    CHECK(h4.fsd_enabled, "HW4 mux0 sets fsd_enabled");
    CHECK((f.buffer[4] & 0x80) != 0, "HW4 mux0 cog-on bit39 set");

    // OFF: bit39 NOT set.
    h4.continue_on_green = false;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0;
    fsd_handle_autopilot_frame(&h4, &f, 0);
    CHECK((f.buffer[4] & 0x80) == 0, "HW4 mux0 cog-off bit39 NOT set");
}

// ── TLSSC bit38 (0x3FD mux0) — opt-in, HW3 + HW4 ─────────────────────────────
// Explicit TLSSC enable on mux0. Gated by assist_tlssc_bit38 + fsd_enabled,
// applies before the HW split so behavior must match on HW3/HW4.
// bit38 -> byte4 bit6 -> mask 0x40.
static void test_tlssc_bit38(void) {
    CANFRAME f;

    // ── HW3 ──
    FSDState h3;
    memset(&h3, 0, sizeof(h3));
    h3.hw_version = TeslaHW_HW3;
    h3.force_fsd = true; // makes mux0 select FSD -> fsd_enabled = true

    // ON: bit38 set on mux0.
    h3.assist_tlssc_bit38 = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0; // mux0
    CHECK(fsd_handle_autopilot_frame(&h3, &f, 0), "HW3 mux0 tlssc38-on reports modified");
    CHECK(h3.fsd_enabled, "HW3 mux0 sets fsd_enabled");
    CHECK((f.buffer[4] & 0x40) != 0, "HW3 mux0 tlssc38-on bit38 set");

    // OFF: bit38 NOT set.
    h3.assist_tlssc_bit38 = false;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0;
    fsd_handle_autopilot_frame(&h3, &f, 0);
    CHECK((f.buffer[4] & 0x40) == 0, "HW3 mux0 tlssc38-off bit38 NOT set");

    // ── HW4 ──
    FSDState h4;
    memset(&h4, 0, sizeof(h4));
    h4.hw_version = TeslaHW_HW4;
    h4.force_fsd = true;

    // ON: bit38 set on mux0.
    h4.assist_tlssc_bit38 = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0;
    CHECK(fsd_handle_autopilot_frame(&h4, &f, 0), "HW4 mux0 tlssc38-on reports modified");
    CHECK(h4.fsd_enabled, "HW4 mux0 sets fsd_enabled");
    CHECK((f.buffer[4] & 0x40) != 0, "HW4 mux0 tlssc38-on bit38 set");

    // OFF: bit38 NOT set.
    h4.assist_tlssc_bit38 = false;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0;
    fsd_handle_autopilot_frame(&h4, &f, 0);
    CHECK((f.buffer[4] & 0x40) == 0, "HW4 mux0 tlssc38-off bit38 NOT set");
}

// ── 0x399 ISA speed chime: bit + Tesla additive checksum ──────────────────────
static void test_isa_checksum(void) {
    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0x11;
    f.buffer[1] = 0x22;
    f.buffer[2] = 0x33;
    f.buffer[3] = 0x44;
    f.buffer[4] = 0x55;
    f.buffer[5] = 0x66;
    f.buffer[6] = 0x77;

    CHECK(fsd_handle_isa_speed_chime(&f), "isa reports modified");
    CHECK((f.buffer[1] & 0x20) != 0, "isa sets bit5 of byte1");

    // Independent oracle: sum(byte0..6 AFTER the bit set) + id_lo + id_hi.
    // CAN_ID_ISA_SPEED = 0x399 -> lo 0x99, hi 0x03.
    uint8_t sum = 0;
    for(int i = 0; i < 7; i++)
        sum += f.buffer[i];
    sum = (uint8_t)(sum + 0x99 + 0x03);
    CHECK(f.buffer[7] == sum, "isa checksum got 0x%02X exp 0x%02X", f.buffer[7], sum);
}

// ── 0x257 DI_speed parse ──────────────────────────────────────────────────────
static void test_di_speed(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[1] = 0x10;
    f.buffer[2] = 0x27;
    f.buffer[3] = 0x42;
    fsd_handle_di_speed(&s, &f);
    // raw = (0x27<<4)|(0x10>>4) = 0x270|1 = 625 ; 625*0.08 - 40 = 10.0
    CHECK(
        fabs(s.vehicle_speed_kph - 10.0f) < 0.01f,
        "speed got %.3f exp 10.0",
        (double)s.vehicle_speed_kph);
    CHECK(s.ui_speed == 0x42, "ui_speed got 0x%02X exp 0x42", s.ui_speed);
    CHECK(s.speed_seen, "speed_seen set after parse");
}

// ── 0x331 TLSSC restore ───────────────────────────────────────────────────────
static void test_tlssc_restore(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.tlssc_restore = true;
    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;

    f.buffer[0] = 0x00;
    CHECK(fsd_handle_tlssc_restore(&s, &f), "tlssc modifies fresh byte0");
    CHECK(f.buffer[0] == 0x1B, "tlssc byte0 -> 0x1B got 0x%02X", f.buffer[0]);
    // already-restored frame: no change, returns false
    CHECK(fsd_handle_tlssc_restore(&s, &f) == false, "tlssc no-op when already 0x1B");
    // upper 2 bits preserved
    f.buffer[0] = 0xC5;
    CHECK(fsd_handle_tlssc_restore(&s, &f), "tlssc modifies 0xC5");
    CHECK(f.buffer[0] == 0xDB, "tlssc preserves top bits -> 0xDB got 0x%02X", f.buffer[0]);

    s.tlssc_restore = false;
    f.buffer[0] = 0x00;
    CHECK(fsd_handle_tlssc_restore(&s, &f) == false, "tlssc disabled -> no-op");
}

// ── 0x313 track-mode inject + additive checksum ───────────────────────────────
static void test_track_mode_crc(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.track_mode_inject = true; // master opt-in
    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0xF0;
    f.buffer[1] = 0x11;
    f.buffer[2] = 0x22;

    CHECK(fsd_handle_track_mode_inject(&s, &f), "track-mode reports modified");
    CHECK((f.buffer[0] & 0x03) == 0x01, "track-mode sets request ON bit");
    // Independent oracle: byte7 = (id_lo + id_hi + sum(byte0..6)) & 0xFF, 0x313.
    uint16_t sum = (0x313 & 0xFF) + ((0x313 >> 8) & 0xFF);
    for(int i = 0; i < 7; i++)
        sum += f.buffer[i];
    CHECK(
        f.buffer[7] == (uint8_t)(sum & 0xFF),
        "track-mode checksum got 0x%02X exp 0x%02X",
        f.buffer[7],
        (uint8_t)(sum & 0xFF));

    // Master-toggle gate: inject off -> no-op
    s.track_mode_inject = false;
    CANFRAME g;
    zero(&g);
    g.data_lenght = 8;
    CHECK(fsd_handle_track_mode_inject(&s, &g) == false, "track-mode gated when inject off");
}

// ── 0x313 adjustable balance / stability / cooling ────────────────────────────
static void test_track_mode_adjust(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.track_mode_inject = true;
    s.track_rotation_pct = 40; // -> raw 80
    s.track_stability_pct = 80; // -> raw 160
    s.track_post_cooling = true; // byte3 bit0 set
    s.track_cmp_overclock = false; // byte3 bit1 clear

    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[3] = 0xF0; // upper nibble must survive the bit0/bit1 write

    CHECK(fsd_handle_track_mode_inject(&s, &f), "track-adjust reports modified");
    CHECK((f.buffer[0] & 0x03) == 0x01, "track-adjust request ON");
    CHECK(f.buffer[1] == 80, "track-adjust rotation raw got %u exp 80", f.buffer[1]);
    CHECK(f.buffer[2] == 160, "track-adjust stability raw got %u exp 160", f.buffer[2]);
    CHECK((f.buffer[3] & 0x01) == 0x01, "track-adjust post-cooling bit set");
    CHECK((f.buffer[3] & 0x02) == 0x00, "track-adjust overclock bit clear");
    CHECK((f.buffer[3] & 0xF0) == 0xF0, "track-adjust preserves byte3 upper bits");
    CHECK(
        f.buffer[7] == tesla_additive_checksum(0x313, f.buffer, 7),
        "track-adjust checksum got 0x%02X",
        f.buffer[7]);

    // inject off -> false, frame unchanged
    FSDState s2;
    memset(&s2, 0, sizeof(s2));
    s2.track_mode_inject = false;
    CANFRAME g;
    zero(&g);
    g.data_lenght = 8;
    g.buffer[0] = 0xAA;
    g.buffer[1] = 0xBB;
    CHECK(fsd_handle_track_mode_inject(&s2, &g) == false, "track-adjust off -> no-op");
    CHECK(g.buffer[0] == 0xAA && g.buffer[1] == 0xBB, "track-adjust off leaves frame unchanged");
}

// ── 0x249 SCCM_leftStalk builders + CRC ───────────────────────────────────────
static uint8_t sccm_expected_crc(const CANFRAME* f) {
    return (uint8_t)(((0x249 & 0xFF) + ((0x249 >> 8) & 0xFF) + f->buffer[1] + f->buffer[2]) &
                     0xFF);
}

static void test_sccm_crc(void) {
    CANFRAME f;

    fsd_build_highbeam_flash(&f, 3, true);
    CHECK(f.canId == CAN_ID_SCCM_LSTALK, "highbeam id 0x249");
    CHECK(f.data_lenght == 3, "highbeam dlc 3");
    CHECK((f.buffer[1] & 0x0F) == 3, "highbeam counter=3");
    CHECK((f.buffer[1] & 0x30) == 0x10, "highbeam PULL bit (status=1)");
    CHECK(
        f.buffer[0] == sccm_expected_crc(&f),
        "highbeam CRC got 0x%02X exp 0x%02X",
        f.buffer[0],
        sccm_expected_crc(&f));

    fsd_build_turn_signal(&f, 5, 3); // 3 = DOWN_1 (left)
    CHECK((f.buffer[1] & 0x0F) == 5, "turn counter=5");
    CHECK((f.buffer[2] & 0x07) == 3, "turn direction=3");
    CHECK(
        f.buffer[0] == sccm_expected_crc(&f),
        "turn CRC got 0x%02X exp 0x%02X",
        f.buffer[0],
        sccm_expected_crc(&f));

    fsd_build_wiper_wash(&f, 2);
    CHECK((f.buffer[1] & 0x0F) == 2, "wiper counter=2");
    CHECK((f.buffer[1] & 0xC0) == 0x40, "wiper 1ST_DETENT bit");
    CHECK(
        f.buffer[0] == sccm_expected_crc(&f),
        "wiper CRC got 0x%02X exp 0x%02X",
        f.buffer[0],
        sccm_expected_crc(&f));
}

// ── 0x370 nag killer: counter+1, hands-on spoof, self-consistent checksum ─────
static void test_nag_killer(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.nag_killer = true;
    s.das_hands_on_state = 0xFF; // no DAS frame seen -> conservative echo
    s.nag_demand_active = false;

    CANFRAME in;
    zero(&in);
    in.data_lenght = 8;
    in.buffer[4] = 0x00; // handsOnLevel = 0 (nag imminent)
    in.buffer[6] = 0x05; // counter = 5

    CANFRAME out;
    zero(&out);
    CHECK(fsd_handle_nag_killer(&s, &in, &out, 1000u), "nag echo emitted on level 0");
    CHECK(out.canId == CAN_ID_EPAS_STATUS, "nag echo id 0x370");
    CHECK(out.data_lenght == 8, "nag echo dlc 8");
    CHECK(((out.buffer[4] >> 6) & 0x03) == 1, "nag spoofs handsOnLevel=1");
    CHECK((out.buffer[6] & 0x0F) == 6, "nag counter+1 -> 6 got %u", out.buffer[6] & 0x0F);
    // Checksum self-consistency: holds regardless of the (PRNG-driven) torque bytes.
    uint16_t sum = 0;
    for(int i = 0; i < 7; i++)
        sum += out.buffer[i];
    sum += (CAN_ID_EPAS_STATUS & 0xFF) + (CAN_ID_EPAS_STATUS >> 8);
    CHECK(
        out.buffer[7] == (uint8_t)(sum & 0xFF),
        "nag checksum self-consistent got 0x%02X exp 0x%02X",
        out.buffer[7],
        (uint8_t)(sum & 0xFF));

    // skip paths
    CANFRAME out2;
    zero(&out2);
    in.buffer[4] = 0x40; // handsOnLevel = 1 (hands detected)
    CHECK(fsd_handle_nag_killer(&s, &in, &out2, 1000u) == false, "nag skips when hands detected");
    in.buffer[4] = 0x00;
    s.das_hands_on_state = 0; // DAS satisfied
    CHECK(fsd_handle_nag_killer(&s, &in, &out2, 1000u) == false, "nag skips when DAS satisfied");
    s.das_hands_on_state = 0xFF;
    s.nag_killer = false;
    CHECK(fsd_handle_nag_killer(&s, &in, &out2, 1000u) == false, "nag skips when disabled");

    // --- DAS escalation edge re-arms the grip pulse even when EPAS handsOnLevel
    //     is frozen at 0 (HW4 Juniper trims, #100). das stepping up must fire a
    //     fresh strong pulse on each rising edge, not just once. ---
    FSDState e;
    memset(&e, 0, sizeof(e));
    e.nag_killer = true;
    e.das_prev_hands_on_state = 0xFF;
    CANFRAME ein, eout;
    zero(&ein);
    ein.data_lenght = 8;
    ein.buffer[4] = 0x00; // handsOnLevel frozen at 0, as on the affected trims

    // das 2 -> 3 rising edge still re-arms the grip-pulse path despite frozen
    // handsOnLevel (#100). The pulse torque is now clamped to the ±1.8 Nm cap
    // (#122) — it no longer exceeds the walk, so we verify the echo + the cap
    // rather than the old >2290 excursion (which the cap removes by design).
    e.das_hands_on_state = 2;
    zero(&eout);
    fsd_handle_nag_killer(&e, &ein, &eout, 1000u);
    e.das_hands_on_state = 3;
    zero(&eout);
    CHECK(fsd_handle_nag_killer(&e, &ein, &eout, 1000u), "nag echo on das 2->3 edge");
    int ntorq2 = ((eout.buffer[2] & 0x0F) << 8) | eout.buffer[3];
    CHECK(
        ntorq2 >= NAG_TORQUE_RAW_MIN && ntorq2 <= NAG_TORQUE_RAW_MAX,
        "das 2->3 echo within ±1.8 Nm cap (#122), torq=%d",
        ntorq2);
}

// ── 0x370 nag killer EPAS-faithful (Mode-C) mode (v2.17, #100): demand-state
//    machine — AP-state gate, state-2 mild walk after 2 s delay, state-3 strong
//    ramp after 1 s pause, handsOnLevel derived from torque magnitude. ─────────
#define RAWABS(r) ((r) >= 2048 ? (r) - 2048 : 2048 - (r))
static void test_nag_killer_faithful(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.nag_killer = true;
    s.nag_epas_faithful = true;
    s.das_ap_state = 3; // AP active
    s.steering_angle_deg = 0.0f; // dir = +1 -> positive torque band

    CANFRAME in, out;
    zero(&in);
    in.data_lenght = 8;
    in.buffer[6] = 0x05;
    uint32_t t = 100000u;

    // AP-state gate: no inject when AP not active.
    s.das_ap_state = 0;
    s.das_hands_on_state = 2;
    zero(&out);
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, t) == false, "faithful: no inject when AP inactive");
    s.das_ap_state = 3;

    // Demand satisfied (state 0): no inject (also resets state memory).
    s.das_hands_on_state = 0;
    zero(&out);
    CHECK(fsd_handle_nag_killer(&s, &in, &out, t) == false, "faithful: no inject in state 0");

    // State 2: 2 s idle delay holds injection off.
    s.das_hands_on_state = 2;
    zero(&out);
    CHECK(fsd_handle_nag_killer(&s, &in, &out, t) == false, "faithful: state2 enters delay");
    zero(&out);
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, t + 1500u) == false,
        "faithful: state2 still in 2s delay");

    // After 2 s: mild torque in +0.5..+2.0 Nm band, handsOnLevel LEFT AT 0, counter+1, checksum.
    int any = 0, minraw = 99999, maxraw = -1, hands_set = 0, cnt_ok = 1, csum_ok = 1;
    for(uint32_t dt = 2100; dt < 6000; dt += 50) {
        in.buffer[6] = (in.buffer[6] & 0xF0) | ((5 + dt / 50) & 0x0F);
        zero(&out);
        if(!fsd_handle_nag_killer(&s, &in, &out, t + dt)) continue;
        any = 1;
        int raw = ((out.buffer[2] & 0x0F) << 8) | out.buffer[3];
        if(raw < minraw) minraw = raw;
        if(raw > maxraw) maxraw = raw;
        // handsOnLevel must stay at the real value (input byte4=0) — real EPAS
        // never sets it; deriving it is a 14.x preflight tell (#122).
        if(((out.buffer[4] >> 6) & 0x03) != 0) hands_set++;
        if((out.buffer[6] & 0x0F) != (((5 + dt / 50) + 1) & 0x0F)) cnt_ok = 0;
        uint16_t cs = (CAN_ID_EPAS_STATUS & 0xFF) + (CAN_ID_EPAS_STATUS >> 8);
        for(int b = 0; b < 7; b++)
            cs += out.buffer[b];
        if(out.buffer[7] != (uint8_t)(cs & 0xFF)) csum_ok = 0;
    }
    CHECK(any, "faithful state2: injects after 2 s delay");
    CHECK(
        minraw >= 2098 && maxraw <= 2248,
        "faithful state2: mild band +0.5..+2.0Nm (%d..%d)",
        minraw,
        maxraw);
    CHECK(
        hands_set == 0,
        "faithful: handsOnLevel left at 0 (real EPAS never sets it) — %d violations",
        hands_set);
    CHECK(cnt_ok, "faithful: counter+1 every injected frame");
    CHECK(csum_ok, "faithful: checksum valid every injected frame");

    // State 3 (strong): 1 s pause, then ramp/hold toward 2.1 Nm; handsOnLevel still untouched.
    s.das_hands_on_state = 3;
    uint32_t ts = t + 7000u;
    zero(&out);
    CHECK(fsd_handle_nag_killer(&s, &in, &out, ts) == false, "faithful state3: 1 s pause holds");
    int got_strong = 0, hands_set3 = 0;
    for(uint32_t dt = 1100; dt < 3500; dt += 50) {
        zero(&out);
        if(!fsd_handle_nag_killer(&s, &in, &out, ts + dt)) continue;
        int raw = ((out.buffer[2] & 0x0F) << 8) | out.buffer[3];
        if(RAWABS(raw) >= 180) got_strong = 1; // reaches near 2.1 Nm
        if(((out.buffer[4] >> 6) & 0x03) != 0) hands_set3++;
    }
    CHECK(got_strong, "faithful state3: ramps to strong ~2.1 Nm");
    CHECK(
        hands_set3 == 0,
        "faithful state3: handsOnLevel still left at 0 (%d violations)",
        hands_set3);
}
#undef RAWABS

// ── 0x312 pack temperature plausibility gate (fsd_bms.h) ─────────────────────
// byte4/byte5 - 40 is not from any DBC and the frame is multiplexed on newer
// firmware. Rejected: the #211 dump's one 0x312 (24~101 C) and the two readings
// the tester's Performance card showed (206~8 and 205~-32 C, displayed wrapped
// to int8 as -50~8 and -51~-32).
static void bms312(CANFRAME* f, uint8_t b4, uint8_t b5) {
    zero(f);
    f->canId = CAN_ID_BMS_THERMAL;
    f->data_lenght = 8;
    f->buffer[4] = b4;
    f->buffer[5] = b5;
}
static void test_bms_thermal(void) {
    FSDState s;
    CANFRAME f;
    memset(&s, 0, sizeof(s));
    bms312(&f, 0x3A, 0x3D); // 18~21 C
    fsd_handle_bms_thermal(&s, &f, 1000u);
    CHECK(s.bms_seen && s.batt_temp_ok, "18~21 C: plausible, stored");
    CHECK(
        s.batt_temp_min_c == 18 && s.batt_temp_max_c == 21,
        "18~21 C: got %d~%d",
        s.batt_temp_min_c,
        s.batt_temp_max_c);
    CHECK(fsd_bms_temp_fresh(&s, 1000u + FSD_BMS_TEMP_STALE_MS), "fresh at the stale edge");
    CHECK(
        !fsd_bms_temp_fresh(&s, 1001u + FSD_BMS_TEMP_STALE_MS),
        "stale after FSD_BMS_TEMP_STALE_MS");

    const uint8_t dump[8] = {0xC9, 0x21, 0x48, 0x17, 0x40, 0x8D, 0x00, 0x00}; // #211 dump
    zero(&f);
    f.canId = CAN_ID_BMS_THERMAL;
    f.data_lenght = 8;
    memcpy(f.buffer, dump, 8);
    fsd_handle_bms_thermal(&s, &f, 2000u);
    CHECK(s.batt_temp_rejected == 1, "#211 dump 24~101 C rejected");
    CHECK(
        s.batt_temp_min_c == 18 && s.batt_temp_max_c == 21 && s.batt_temp_ms == 1000u,
        "rejected frame keeps the last plausible reading and its age");
    bms312(&f, 0xF6, 0x30);
    fsd_handle_bms_thermal(&s, &f, 2100u); // 206~8
    bms312(&f, 0xF5, 0x08);
    fsd_handle_bms_thermal(&s, &f, 2200u); // 205~-32
    CHECK(s.batt_temp_rejected == 3, "tester's -50~8 / -51~-32 readings rejected");

    // Bounds: -40 / +70 inclusive, min <= max, spread <= 20.
    CHECK(fsd_bms_temp_plausible(-40, -25), "-40~-25 plausible");
    CHECK(!fsd_bms_temp_plausible(-41, -30), "below -40 rejected");
    CHECK(fsd_bms_temp_plausible(55, 70), "55~70 plausible");
    CHECK(!fsd_bms_temp_plausible(55, 71), "above 70 rejected");
    CHECK(!fsd_bms_temp_plausible(21, 18), "min > max rejected");
    CHECK(fsd_bms_temp_plausible(10, 30), "spread 20 plausible");
    CHECK(!fsd_bms_temp_plausible(10, 31), "spread 21 rejected");

    // Never plausible: no reading at all; short frame ignored.
    memset(&s, 0, sizeof(s));
    bms312(&f, 0xF6, 0x30);
    fsd_handle_bms_thermal(&s, &f, 1000u);
    CHECK(
        s.bms_seen && !s.batt_temp_ok && !fsd_bms_temp_fresh(&s, 1000u),
        "only implausible frames: no reading");
    memset(&s, 0, sizeof(s));
    bms312(&f, 0x3A, 0x3D);
    f.data_lenght = 5;
    fsd_handle_bms_thermal(&s, &f, 1000u);
    CHECK(!s.bms_seen && !s.batt_temp_ok, "DLC 5 ignored");
}

// ── 0x33A car-displayed SOC (fsd_bms.h, #251) ────────────────────────────────
// Five frames from ukinora's fork (their car, screen photographed at 22 %) and
// one from the #211 Model Y. wide = what an 8-bit mask would report: bit 27 is
// set on some frames.
static const struct {
    uint8_t d[8];
    uint8_t pct;
    uint8_t wide;
} UISOC[] = {
    {{0xF8, 0x70, 0x43, 0xA5, 0xFE, 0xFF, 0xCF, 0x64}, 84, 84},
    {{0xCC, 0x70, 0x53, 0x2C, 0xFE, 0xFF, 0xCF, 0x64}, 69, 197},
    {{0xAD, 0x70, 0xB3, 0xDB, 0xFD, 0xFF, 0xCF, 0x64}, 59, 187},
    {{0x9D, 0x70, 0x53, 0xAB, 0xFD, 0xFF, 0xCF, 0x64}, 53, 181},
    {{0x41, 0x70, 0x63, 0xB1, 0xFC, 0xFF, 0xCF, 0x64}, 22, 22},
    {{0xD2, 0x34, 0x04, 0x7D, 0xFE, 0xFF, 0x13, 0x63}, 80, 208}, // #211, 0x292 read 81.8 %
};
static void test_ui_soc(void) {
    for(size_t i = 0; i < sizeof(UISOC) / sizeof(UISOC[0]); i++) {
        uint8_t pct = 0xFF;
        CHECK(
            fsd_bms_decode_ui_soc(UISOC[i].d, 8, &pct) && pct == UISOC[i].pct,
            "0x33A frame %u: %u %% exp %u (8-bit mask would say %u)",
            (unsigned)i,
            (unsigned)pct,
            (unsigned)UISOC[i].pct,
            (unsigned)UISOC[i].wide);
        FSDState s;
        memset(&s, 0, sizeof(s));
        fsd_bms_ui_soc_update(&s, UISOC[i].d, 8);
        CHECK(
            s.ui_soc_seen && s.ui_soc == UISOC[i].pct && s.soc_percent == 0.0f && !s.bms_seen,
            "0x33A frame %u: ui_soc only, BMS fields untouched",
            (unsigned)i);
    }
    uint8_t d[8] = {0, 0, 0x40, 0x06, 0, 0, 0, 0}; // 100
    uint8_t pct = 0xFF;
    CHECK(fsd_bms_decode_ui_soc(d, 8, &pct) && pct == 100, "100 %% decodes");
    d[2] = 0x50; // 101
    pct = 33;
    CHECK(!fsd_bms_decode_ui_soc(d, 8, &pct) && pct == 33, "101 refused, caller value kept");
    d[2] = 0xF0;
    d[3] = 0x07; // 127 (all ones)
    CHECK(!fsd_bms_decode_ui_soc(d, 8, &pct), "127 refused");
    CHECK(!fsd_bms_decode_ui_soc(UISOC[0].d, 3, &pct), "DLC 3 refused");
    CHECK(fsd_bms_decode_ui_soc(UISOC[0].d, 4, &pct) && pct == 84, "DLC 4 enough");
}

// ── 0x292 BMS_socStatus (fsd_bms.h) ──────────────────────────────────────────
// The #211 Model Y frame (tesla_can_20261010_162400.txt, 0x33A read 80 % in the
// same dump). The Flipper used to read bits 0-9 (socMin, 82.9 %).
static void test_bms_soc(void) {
    const uint8_t d211[8] = {0x3D, 0xCB, 0x5C, 0xB4, 0xD0, 0x01, 0x00, 0x00};
    CHECK(
        fsd_bms_soc_field(d211, FSD_BMS_SOC_MIN_BIT) == 829 &&
            fsd_bms_soc_field(d211, FSD_BMS_SOC_UI_BIT) == 818 &&
            fsd_bms_soc_field(d211, FSD_BMS_SOC_MAX_BIT) == 837 &&
            fsd_bms_soc_field(d211, FSD_BMS_SOC_AVG_BIT) == 834,
        "#211 0x292: min 82.9 / UI 81.8 / max 83.7 / avg 83.4");

    // Each field alone at full scale reads back in its own place only.
    static const unsigned bits[4] = {
        FSD_BMS_SOC_MIN_BIT, FSD_BMS_SOC_UI_BIT, FSD_BMS_SOC_MAX_BIT, FSD_BMS_SOC_AVG_BIT};
    bool apart = true;
    for(int i = 0; i < 4; i++) {
        uint8_t d[8] = {0};
        uint64_t w = (uint64_t)0x3FFu << bits[i];
        for(int k = 0; k < 8; k++)
            d[k] = (uint8_t)(w >> (8 * k));
        for(int j = 0; j < 4; j++)
            apart &= fsd_bms_soc_field(d, bits[j]) == (i == j ? 0x3FFu : 0u);
    }
    CHECK(apart, "the four 10-bit fields do not overlap");

    FSDState s;
    CANFRAME f;
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.canId = CAN_ID_BMS_SOC;
    f.data_lenght = 8;
    memcpy(f.buffer, d211, 8);
    fsd_handle_bms_soc(&s, &f);
    CHECK(
        s.bms_seen && s.soc_percent > 81.75f && s.soc_percent < 81.85f,
        "Flipper handler: socUI 81.8 %% (got %.2f)",
        (double)s.soc_percent);

    uint8_t d[8] = {0, 0xA0, 0x0F, 0, 0, 0, 0, 0}; // socUI 1000
    uint16_t x10 = 0;
    CHECK(fsd_bms_decode_soc(d, 3, &x10) && x10 == 1000, "100.0 %% decodes, DLC 3 enough");
    d[1] = 0xA4; // 1001
    x10 = 7;
    CHECK(!fsd_bms_decode_soc(d, 8, &x10) && x10 == 7, "100.1 %% refused, caller value kept");
    d[1] = 0xFC;
    d[2] = 0x0F; // 1023 (all ones)
    CHECK(!fsd_bms_decode_soc(d, 8, &x10), "1023 refused");
    CHECK(!fsd_bms_decode_soc(d211, 2, &x10), "DLC 2 refused");
    memcpy(f.buffer, d, 8);
    fsd_handle_bms_soc(&s, &f);
    CHECK(s.soc_percent > 81.75f && s.soc_percent < 81.85f, "refused frame keeps the last SOC");
    memset(&s, 0, sizeof(s));
    fsd_handle_bms_soc(&s, &f);
    CHECK(!s.bms_seen && s.soc_percent == 0.0f, "only refused frames: BMS not seen");
}

// ── 0x129 SCCM_steeringAngleSensor (fsd_steer.h) ─────────────────────────────
// Real frames from the #176 (HW3 Model 3) and #211 (HW4 Model Y) records. The
// pre-fix int16 read of bytes 0-1 gave the angle in the trailing comment.
static const struct {
    uint8_t d[8];
    bool valid;
    float deg;
    const char* where;
} STEER129[] = {
    {{0xDA, 0x25, 0xF4, 0x5F, 0x00, 0x20, 0xFF, 0x3F}, true, -1.2f, "#176 evt_557853"}, // 969.0
    {{0x63, 0x2B, 0x99, 0x61, 0x0C, 0x20, 0xFF, 0x3F}, true, 40.9f, "#176 evt_974427"}, // 1110.7
    {{0x01, 0x26, 0xDF, 0x5F, 0xFE, 0x1F, 0xFF, 0x3F}, true, -3.3f, "#176 evt_2116601"}, // 972.9
    {{0xFD, 0x2F, 0x28, 0x60, 0x00, 0x20, 0xFF, 0x3F}, true, 4.0f, "#211 ev000001"}, // 1228.5
    {{0x0E, 0x27, 0xD8, 0x60, 0xFF, 0x1F, 0xFF, 0x3F}, true, 21.6f, "#211 ev000002"}, // 999.8
    {{0xC9, 0x21, 0xC5, 0x5F, 0xFF, 0x1F, 0xFF, 0x3F}, true, -5.9f, "#211 ev000003"}, // 864.9
    // Parked dump, SCCM_steeringAngleValidity INVALID (raw angle -357.5).
    {{0xC4, 0x27, 0x09, 0x12, 0xFF, 0x1F, 0xFF, 0x3F}, false, 0.0f, "#211 parked"}, // 1018.0
};

static void steer_frame(CANFRAME* f, const uint8_t d[8]) {
    zero(f);
    f->canId = CAN_ID_STEER_ANGLE;
    f->data_lenght = 8;
    memcpy(f->buffer, d, 8);
}

static int modec_raw(const CANFRAME* out) {
    return ((out->buffer[2] & 0x0F) << 8) | out->buffer[3];
}

static void test_steering_angle(void) {
    FSDState s;
    CANFRAME f;
    for(size_t i = 0; i < sizeof(STEER129) / sizeof(STEER129[0]); i++) {
        memset(&s, 0, sizeof(s));
        steer_frame(&f, STEER129[i].d);
        fsd_handle_steering_angle(&s, &f, 1000u);
        CHECK(s.steering_angle_seen, "%s: 0x129 seen", STEER129[i].where);
        CHECK(
            s.steering_angle_valid == STEER129[i].valid,
            "%s: valid %d exp %d",
            STEER129[i].where,
            s.steering_angle_valid,
            STEER129[i].valid);
        CHECK(fsd_steer_known(&s, 1000u) == STEER129[i].valid, "%s: known", STEER129[i].where);
        if(STEER129[i].valid)
            CHECK(
                fabs(s.steering_angle_deg - STEER129[i].deg) < 0.05f,
                "%s: %.2f deg exp %.1f",
                STEER129[i].where,
                (double)s.steering_angle_deg,
                (double)STEER129[i].deg);
    }

    // Freshness: known for FSD_STEER_FRESH_MS, then unknown.
    memset(&s, 0, sizeof(s));
    steer_frame(&f, STEER129[1].d);
    fsd_handle_steering_angle(&s, &f, 1000u);
    CHECK(fsd_steer_known(&s, 1000u + FSD_STEER_FRESH_MS), "angle known at the freshness edge");
    CHECK(!fsd_steer_known(&s, 1001u + FSD_STEER_FRESH_MS), "angle unknown once stale");

    // A non-VALID frame after a valid one: unknown; the stored value is kept.
    steer_frame(&f, STEER129[6].d);
    fsd_handle_steering_angle(&s, &f, 1010u);
    CHECK(!fsd_steer_known(&s, 1010u) && s.steering_angle_seen, "INVALID after valid -> unknown");
    CHECK(
        fabs(s.steering_angle_deg - 40.9f) < 0.05f, "INVALID leaves the last valid angle stored");

    // Validity INIT / SNA, sensor status ERROR, short DLC.
    uint8_t d[8];
    memcpy(d, STEER129[0].d, 8);
    d[3] = (uint8_t)((d[3] & 0x3Fu) | (2u << 6)); // validity INIT
    memset(&s, 0, sizeof(s));
    steer_frame(&f, d);
    fsd_handle_steering_angle(&s, &f, 1000u);
    CHECK(!s.steering_angle_valid, "validity INIT -> unknown");
    d[3] = (uint8_t)((d[3] & 0x3Fu) | (3u << 6)); // validity SNA
    memset(&s, 0, sizeof(s));
    steer_frame(&f, d);
    fsd_handle_steering_angle(&s, &f, 1000u);
    CHECK(!s.steering_angle_valid, "validity SNA -> unknown");
    memcpy(d, STEER129[0].d, 8);
    d[1] = (uint8_t)((d[1] & 0x3Fu) | (2u << 6)); // sensor status ERROR
    memset(&s, 0, sizeof(s));
    steer_frame(&f, d);
    fsd_handle_steering_angle(&s, &f, 1000u);
    CHECK(!s.steering_angle_valid, "sensor status ERROR -> unknown");
    memset(&s, 0, sizeof(s));
    steer_frame(&f, STEER129[0].d);
    f.data_lenght = 3;
    fsd_handle_steering_angle(&s, &f, 1000u);
    CHECK(!s.steering_angle_seen, "DLC 3 ignored");

    // EPAS-faithful torque direction (#100): opposite the wheel, held within
    // ±FSD_STEER_NAG_HOLD_DEG and while the angle is unknown. Before the fix the
    // 819..1228 deg reading made it -1 (-1.8 Nm in the #176 records) on every frame.
    memset(&s, 0, sizeof(s));
    s.nag_killer = true;
    s.nag_epas_faithful = true;
    s.das_ap_state = 3;
    CANFRAME in, out;
    zero(&in);
    in.data_lenght = 8;
    uint32_t t = 200000u;
    s.das_hands_on_state = 0;
    fsd_handle_nag_killer(&s, &in, &out, t); // reset demand memory
    s.das_hands_on_state = 3;
    fsd_handle_nag_killer(&s, &in, &out, t); // strong demand, 1 s pause
    zero(&out);
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, t + 1600u) && modec_raw(&out) > 2048,
        "faithful dir: no angle yet -> + (raw %d)",
        modec_raw(&out));
    steer_frame(&f, STEER129[1].d); // +40.9 deg
    fsd_handle_steering_angle(&s, &f, t + 1700u);
    zero(&out);
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, t + 1700u) && modec_raw(&out) < 2048,
        "faithful dir: +40.9 deg -> - (raw %d)",
        modec_raw(&out));
    fsd_steer_set(&s, -2.0f, t + 1800u); // inside the hold band
    zero(&out);
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, t + 1800u) && modec_raw(&out) < 2048,
        "faithful dir: -2.0 deg holds - (raw %d)",
        modec_raw(&out));
    steer_frame(&f, STEER129[5].d); // -5.9 deg
    fsd_handle_steering_angle(&s, &f, t + 1900u);
    zero(&out);
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, t + 1900u) && modec_raw(&out) > 2048,
        "faithful dir: -5.9 deg -> + (raw %d)",
        modec_raw(&out));
    zero(&out); // angle stale by now
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, t + 1900u + FSD_STEER_FRESH_MS + 1u) &&
            modec_raw(&out) > 2048,
        "faithful dir: stale angle holds + (raw %d)",
        modec_raw(&out));
}

// ── configurable signal mapping + freshness (#122) ───────────────────────────
static void test_signal_config(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    // Auto mode (cfg_das_id == 0): freshness always true, extractor is a no-op.
    CHECK(fsd_das_ctx_fresh(&s, 5000) == true, "ctx fresh: auto mode always fresh");

    // Configure ssw0209's variant: 0x39B, AP-state in byte0 hi-nibble (his byte1[7:4]=0),
    // hands-on in byte5 bits[5:2]. Frame 0x39B# 52 0E DF A0 B0 44 9? ...
    s.cfg_das_id = 0x39B;
    s.cfg_apstate_byte = 0;
    s.cfg_apstate_shift = 4;
    s.cfg_apstate_mask = 0x0F;
    s.cfg_handson_byte = 5;
    s.cfg_handson_shift = 2;
    s.cfg_handson_mask = 0x0F;
    CANFRAME f;
    zero(&f);
    f.canId = 0x39B;
    f.data_lenght = 8;
    f.buffer[0] = 0x52; // hi nibble 5 -> AP active
    f.buffer[5] = (2u << 2); // hands-on state 2
    fsd_apply_signal_config(&s, &f, 10000u);
    CHECK(s.das_ap_state == 5, "cfg: AP-state read from byte0 hi-nibble = %u", s.das_ap_state);
    CHECK(
        s.das_hands_on_state == 2,
        "cfg: hands-on read from byte5[5:2] = %u",
        s.das_hands_on_state);
    CHECK(s.das_ctx_seen_ms == 10000u, "cfg: freshness stamped");
    CHECK(fsd_das_ctx_fresh(&s, 10500u) == true, "ctx fresh: within 1s");
    CHECK(fsd_das_ctx_fresh(&s, 11500u) == false, "ctx stale: >1s -> not fresh");

    // Non-matching id leaves state untouched.
    CANFRAME g;
    zero(&g);
    g.canId = 0x123;
    g.data_lenght = 8;
    g.buffer[0] = 0xFF;
    fsd_apply_signal_config(&s, &g, 12000u);
    CHECK(s.das_ap_state == 5, "cfg: non-matching id ignored");

    // Steering id 0x129 is read with its own layout (fsd_steer.h) whatever hi/lo
    // say: hi=1 lo=0 read the CRC + counter, 969.0 deg for this -1.2 frame.
    s.cfg_steer_id = 0x129;
    s.cfg_steer_hi = 1;
    s.cfg_steer_lo = 0;
    CANFRAME h;
    steer_frame(&h, STEER129[0].d);
    fsd_apply_signal_config(&s, &h, 12000u);
    CHECK(
        s.steering_angle_deg > -1.25f && s.steering_angle_deg < -1.15f,
        "cfg: 0x129 steering = %.1f deg (expect -1.2)",
        (double)s.steering_angle_deg);
    CHECK(
        fsd_steer_known(&s, 12000u) && s.steer_ctx_seen_ms == 12000u,
        "cfg: 0x129 steering angle known (fresh)");
    steer_frame(&h, STEER129[6].d); // parked, validity INVALID
    fsd_apply_signal_config(&s, &h, 12100u);
    CHECK(s.steering_angle_seen && !fsd_steer_known(&s, 12100u), "cfg: 0x129 INVALID -> unknown");

    // Any other id: signed 16-bit at hi/lo, 0.1 deg per bit; hi == lo maps nothing.
    s.cfg_steer_id = 0x123;
    s.steering_angle_valid = false;
    zero(&h);
    h.canId = 0x123;
    h.data_lenght = 4;
    h.buffer[0] = 0x2C;
    h.buffer[1] = 0x01; // 0x012C = 300 -> 30.0 deg
    fsd_apply_signal_config(&s, &h, 12200u);
    CHECK(
        s.steering_angle_deg > 29.95f && s.steering_angle_deg < 30.05f &&
            fsd_steer_known(&s, 12200u),
        "cfg: custom id steering = %.1f deg (expect 30)",
        (double)s.steering_angle_deg);
    s.cfg_steer_lo = 1;
    s.steering_angle_valid = false;
    fsd_apply_signal_config(&s, &h, 12300u);
    CHECK(
        !s.steering_angle_valid && s.steer_ctx_seen_ms == 12200u,
        "cfg: custom id, hi == lo -> not mapped");

    // Nag killer no-ops when the configured DAS source goes stale.
    FSDState n;
    memset(&n, 0, sizeof(n));
    n.nag_killer = true;
    n.cfg_das_id = 0x39B;
    n.das_ctx_seen_ms = 1000u;
    n.das_hands_on_state = 0xFF;
    CANFRAME in, out;
    zero(&in);
    in.data_lenght = 8;
    in.buffer[4] = 0x00;
    CHECK(fsd_handle_nag_killer(&n, &in, &out, 1500u) != false, "cfg fresh -> nag echoes");
    CHECK(fsd_handle_nag_killer(&n, &in, &out, 3000u) == false, "cfg stale -> nag no-ops");

    // Mask 0 means "not mapped": the field must be IGNORED (keep the prior value),
    // not forced to 0. A tester who set the DAS id but left the hands-on mask 0
    // zeroed hands-on so the nag killer never fired (#100).
    FSDState m;
    memset(&m, 0, sizeof(m));
    m.das_ap_state = 5;
    m.das_hands_on_state = 3;
    m.cfg_das_id = 0x39B;
    m.cfg_apstate_byte = 0;
    m.cfg_apstate_shift = 0;
    m.cfg_apstate_mask = 0x00; // unmapped
    m.cfg_handson_byte = 5;
    m.cfg_handson_shift = 2;
    m.cfg_handson_mask = 0x00; // unmapped
    CANFRAME mf;
    zero(&mf);
    mf.canId = 0x39B;
    mf.data_lenght = 8;
    mf.buffer[0] = 0x02; // would decode to ap_state 2 if the mask weren't 0
    mf.buffer[5] = (1u << 2); // would decode to hands-on 1 if the mask weren't 0
    fsd_apply_signal_config(&m, &mf, 5000u);
    CHECK(m.das_ap_state == 5, "mask-0 apstate ignored (kept 5, got %u)", m.das_ap_state);
    CHECK(
        m.das_hands_on_state == 3,
        "mask-0 hands-on ignored (kept 3, got %u)",
        m.das_hands_on_state);
    CHECK(m.das_ctx_seen_ms == 5000u, "mask-0 still stamps freshness (id matched)");
    // A real mask still maps normally.
    m.cfg_apstate_mask = 0x0F;
    fsd_apply_signal_config(&m, &mf, 5100u);
    CHECK(m.das_ap_state == 2, "mapped mask reads byte0 (got %u)", m.das_ap_state);
}

// ── Abort Guard (steer-jerk, #108) ───────────────────────────────────────────
static void test_abort_guard(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));

    // Off by default: gate always allows, update never latches.
    s.das_ap_state = DAS_APSTATE_ABORTING;
    fsd_abort_guard_update(&s);
    CHECK(s.abort_guard_latched == false, "abort_guard off: no latch");
    CHECK(fsd_abort_guard_allows(&s) == true, "abort_guard off: always allows");

    // On + AP active (not an abort state): no latch, still allows.
    s.abort_guard = true;
    s.das_ap_state = 6; // ACTIVE
    fsd_abort_guard_update(&s);
    CHECK(fsd_abort_guard_allows(&s) == true, "active state: injection allowed");

    // Car enters ABORTING -> latch -> suppress.
    s.das_ap_state = DAS_APSTATE_ABORTING;
    fsd_abort_guard_update(&s);
    CHECK(s.abort_guard_latched == true, "abort seen: latched");
    CHECK(fsd_abort_guard_allows(&s) == false, "abort latched: injection suppressed");

    // Still suppressed once the state moves to ABORTED, and even back to active —
    // the latch holds for the rest of the engagement.
    s.das_ap_state = DAS_APSTATE_ABORTED;
    fsd_abort_guard_update(&s);
    CHECK(fsd_abort_guard_allows(&s) == false, "aborted: still suppressed");
    s.das_ap_state = 6;
    fsd_abort_guard_update(&s);
    CHECK(fsd_abort_guard_allows(&s) == false, "re-active mid-cycle: stays suppressed");

    // Dropping to AVAILABLE(2) is a disengage (< DAS_APSTATE_ENGAGED) -> re-arms (#108).
    s.das_ap_state = 2;
    fsd_abort_guard_update(&s);
    CHECK(s.abort_guard_latched == false, "AVAILABLE(2) disengage: latch cleared");
    CHECK(fsd_abort_guard_allows(&s) == true, "AVAILABLE(2): re-armed, allows");

    // Re-latch, then confirm a full disengage (das_ap_state < 2) also clears.
    s.das_ap_state = DAS_APSTATE_ABORTING;
    fsd_abort_guard_update(&s);
    CHECK(s.abort_guard_latched == true, "re-latched for clean-disengage check");
    s.das_ap_state = 1;
    fsd_abort_guard_update(&s);
    CHECK(s.abort_guard_latched == false, "clean disengage: latch cleared");
    CHECK(fsd_abort_guard_allows(&s) == true, "re-armed: allows again");

    // The legacy autopilot handler honours the gate: latched -> no modification.
    FSDState h;
    memset(&h, 0, sizeof(h));
    h.abort_guard = true;
    h.abort_guard_latched = true;
    h.hw_version = TeslaHW_Legacy;
    h.force_fsd = true;
    CANFRAME f;
    zero(&f);
    f.canId = 0x3EE;
    f.data_lenght = 8;
    CHECK(
        fsd_handle_legacy_autopilot(&h, &f, 5000u) == false,
        "legacy autopilot suppressed while abort latched");
}

// ── fsd-events shared event-core (#123) ──────────────────────────────────────
static void test_fsd_events(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));

    // Cold start (das_ap_state 0): first poll just seeds the baseline, no event.
    CHECK(fsd_events_poll(&s, 0u) == EVT_NONE, "cold poll: no event");

    // Engage 0->6 (UNAVAIL->active) is not a reported transition.
    s.das_ap_state = 6;
    CHECK(fsd_events_poll(&s, 100u) == EVT_NONE, "engage to active: no event");
    CHECK(fsd_events_poll(&s, 200u) == EVT_NONE, "steady active: no event");

    // 6 -> 8 (ABORTING): EVT_ABORT, carrying from/to + timestamp.
    s.das_ap_state = DAS_APSTATE_ABORTING;
    CHECK(fsd_events_poll(&s, 1000u) == EVT_ABORT, "6->8: abort fires");
    CHECK(s.evt_last_from == 6 && s.evt_last_to == 8, "abort carries 6->8");
    CHECK(s.evt_last_ms == 1000u, "abort carries timestamp");

    // 8 -> 9 (ABORTING->ABORTED): still inside the abort, no re-fire.
    s.das_ap_state = DAS_APSTATE_ABORTED;
    CHECK(fsd_events_poll(&s, 1100u) == EVT_NONE, "8->9: no re-fire within abort");

    // 9 -> 1 (abort resolves to disengaged): disengage uses its own cooldown
    // slot, so it fires even though the abort cooldown is still active.
    s.das_ap_state = 1;
    CHECK(fsd_events_poll(&s, 1200u) == EVT_DISENGAGE, "9->1: disengage fires");
    CHECK(s.evt_last_from == 9 && s.evt_last_to == 1, "disengage carries 9->1");

    // 6 -> 9 path also fires a single abort.
    memset(&s, 0, sizeof(s));
    s.das_ap_state = 6;
    fsd_events_poll(&s, 0u);
    s.das_ap_state = DAS_APSTATE_ABORTED;
    CHECK(fsd_events_poll(&s, 10u) == EVT_ABORT, "6->9: abort fires");

    // Clean disengage 6 -> 1 (no abort first).
    memset(&s, 0, sizeof(s));
    s.das_ap_state = 6;
    fsd_events_poll(&s, 0u);
    s.das_ap_state = 1;
    CHECK(fsd_events_poll(&s, 10u) == EVT_DISENGAGE, "6->1: clean disengage fires");
    CHECK(s.evt_last_from == 6 && s.evt_last_to == 1, "disengage carries 6->1");

    // Cooldown: a flapping abort does not re-emit within FSD_EVENT_COOLDOWN_MS,
    // then re-arms once the window passes.
    memset(&s, 0, sizeof(s));
    s.das_ap_state = 6;
    fsd_events_poll(&s, 0u);
    s.das_ap_state = 8;
    CHECK(fsd_events_poll(&s, 1000u) == EVT_ABORT, "abort #1 fires");
    s.das_ap_state = 6;
    fsd_events_poll(&s, 1500u); // leave abort: no event
    s.das_ap_state = 8;
    CHECK(fsd_events_poll(&s, 2000u) == EVT_NONE, "abort #2 within cooldown suppressed");
    s.das_ap_state = 6;
    fsd_events_poll(&s, 1000u + FSD_EVENT_COOLDOWN_MS + 1u);
    s.das_ap_state = 8;
    CHECK(
        fsd_events_poll(&s, 1000u + FSD_EVENT_COOLDOWN_MS + 2u) == EVT_ABORT,
        "abort re-arms after cooldown");

    // Injected MANUAL respects its own cooldown and re-arms after the window.
    memset(&s, 0, sizeof(s));
    CHECK(fsd_events_inject(&s, EVT_MANUAL, 0u) == EVT_MANUAL, "manual #1 fires");
    CHECK(
        fsd_events_inject(&s, EVT_MANUAL, 5000u) == EVT_NONE, "manual within cooldown suppressed");
    CHECK(
        fsd_events_inject(&s, EVT_MANUAL, FSD_EVENT_COOLDOWN_MS + 1u) == EVT_MANUAL,
        "manual re-arms after cooldown");

    // Injected BUSOFF has an independent cooldown slot from MANUAL.
    CHECK(
        fsd_events_inject(&s, EVT_BUSOFF, FSD_EVENT_COOLDOWN_MS + 2u) == EVT_BUSOFF,
        "busoff fires independent of manual cooldown");
    CHECK(
        fsd_events_inject(&s, EVT_BUSOFF, FSD_EVENT_COOLDOWN_MS + 100u) == EVT_NONE,
        "busoff within its own cooldown suppressed");

    // inject only accepts caller-sourced types — detection-only / none are rejected.
    CHECK(fsd_events_inject(&s, EVT_ABORT, 999999u) == EVT_NONE, "inject rejects EVT_ABORT");
    CHECK(
        fsd_events_inject(&s, EVT_DISENGAGE, 999999u) == EVT_NONE, "inject rejects EVT_DISENGAGE");
    CHECK(fsd_events_inject(&s, EVT_NONE, 999999u) == EVT_NONE, "inject rejects EVT_NONE");
}

// ── tap capability verdicts (#125) ───────────────────────────────────────────
static void test_capability(void) {
    // A "full Party-like" tap: 0x370 + HW4 DAS (0x39B) + AP control + steer.
    // Everything works.
    FSDCapSeen full = {0};
    full.epas = true;
    full.das_hw4 = true;
    full.ap_control = true;
    full.steer = true;
    full.steer_valid = true;
    FSDCapReport r = fsd_capability_eval(full, TeslaHW_HW4);
    CHECK(r.nag_killer == CAP_OK, "full tap: nag killer OK (%d)", r.nag_killer);
    CHECK(r.ap_first == CAP_OK, "full tap: AP-First OK");
    CHECK(r.fsd_activation == CAP_OK, "full tap: FSD activation OK");
    CHECK(r.soft_engage == CAP_OK, "full tap: soft engage OK");
    CHECK(r.bus_hint == CAP_HINT_PARTY, "full tap: hint Party-like");
    CHECK(!r.hw_unconfirmed, "full tap: HW known");

    // ── THE 0x399 TRAP ──
    // HW4 with 0x399 ONLY (the ISA speed chime, NOT DAS state) + 0x370.
    // DAS state must read as MISSING -> nag killer degrades to dual-CAN, and
    // AP-First is impossible.
    FSDCapSeen hw4_isa = {0};
    hw4_isa.epas = true;
    hw4_isa.das_hw3 = true; // 0x399 seen, but it's the chime
    r = fsd_capability_eval(hw4_isa, TeslaHW_HW4);
    CHECK(!r.has_das, "0x399 on HW4 is the chime, not DAS state");
    CHECK(r.nag_killer == CAP_DUAL_CAN, "HW4 0x399-only: nag killer dual-CAN (%d)", r.nag_killer);
    CHECK(r.ap_first == CAP_MISSING, "HW4 0x399-only: AP-First missing");

    // HW3 with 0x399 (here it IS the DAS state) + 0x370 -> everything gates.
    FSDCapSeen hw3_das = {0};
    hw3_das.epas = true;
    hw3_das.das_hw3 = true;
    r = fsd_capability_eval(hw3_das, TeslaHW_HW3);
    CHECK(r.has_das, "0x399 on HW3 is DAS state");
    CHECK(r.nag_killer == CAP_OK, "HW3 0x399: nag killer OK");
    CHECK(r.ap_first == CAP_OK, "HW3 0x399: AP-First OK");

    // Legacy with 0x399 also reads as DAS state (only HW4 is the trap).
    FSDCapSeen legacy_das = {0};
    legacy_das.epas = true;
    legacy_das.das_hw3 = true;
    legacy_das.ap_legacy = true;
    r = fsd_capability_eval(legacy_das, TeslaHW_Legacy);
    CHECK(r.has_das, "0x399 on Legacy is DAS state");
    CHECK(r.nag_killer == CAP_OK, "Legacy 0x399: nag killer OK");
    CHECK(r.fsd_activation == CAP_OK, "Legacy 0x3EE: FSD activation OK");

    // ── dual-CAN: 0x370 present but NO DAS state anywhere ──
    FSDCapSeen epas_only = {0};
    epas_only.epas = true;
    r = fsd_capability_eval(epas_only, TeslaHW_HW4);
    CHECK(r.nag_killer == CAP_DUAL_CAN, "0x370 without DAS: dual-CAN recommended");
    CHECK(r.ap_first == CAP_MISSING, "0x370 without DAS: AP-First missing");

    // ── wrong bus for the nag killer: no 0x370 to echo ──
    FSDCapSeen no_epas = {0};
    no_epas.das_hw4 = true; // DAS state here, but no 0x370
    r = fsd_capability_eval(no_epas, TeslaHW_HW4);
    CHECK(r.nag_killer == CAP_MISSING, "no 0x370: nag killer missing (wrong bus)");
    CHECK(r.ap_first == CAP_OK, "DAS present: AP-First still OK");

    // ── FSD activation: either 0x3FD or 0x3EE satisfies it ──
    FSDCapSeen legacy_ap = {0};
    legacy_ap.ap_legacy = true;
    CHECK(
        fsd_capability_eval(legacy_ap, TeslaHW_Legacy).fsd_activation == CAP_OK,
        "0x3EE alone: FSD activation OK");
    FSDCapSeen empty = {0};
    CHECK(
        fsd_capability_eval(empty, TeslaHW_HW3).fsd_activation == CAP_MISSING,
        "no AP frame: FSD activation missing");

    // ── soft engage degrades to AP-First-only without 0x129 ──
    CHECK(
        fsd_capability_eval(hw3_das, TeslaHW_HW3).soft_engage == CAP_MISSING,
        "no 0x129: soft engage degrades (missing)");

    // ── HW unknown inference ──
    // 0x39B seen -> infer HW4 (so a lone 0x399 would be treated as chime).
    FSDCapSeen unk_hw4 = {0};
    unk_hw4.das_hw4 = true;
    unk_hw4.das_hw3 = true;
    unk_hw4.epas = true;
    r = fsd_capability_eval(unk_hw4, TeslaHW_Unknown);
    CHECK(r.hw_unconfirmed, "unknown HW: flagged unconfirmed");
    CHECK(r.hw_effective == TeslaHW_HW4, "0x39B present -> inferred HW4");
    CHECK(r.has_das, "inferred HW4 still has DAS via 0x39B");

    // Unknown HW, only 0x399 + 0x370: assume HW3/Legacy -> 0x399 IS DAS state,
    // so the nag killer is reported workable but unconfirmed.
    FSDCapSeen unk_399 = {0};
    unk_399.das_hw3 = true;
    unk_399.epas = true;
    r = fsd_capability_eval(unk_399, TeslaHW_Unknown);
    CHECK(r.hw_unconfirmed, "unknown HW w/ 0x399: unconfirmed");
    CHECK(r.hw_effective == TeslaHW_HW3, "0x399-only unknown -> assume HW3");
    CHECK(r.has_das, "0x399-only unknown -> treated as DAS state");
    CHECK(r.nag_killer == CAP_OK, "0x399-only unknown: nag killer OK (unconfirmed)");

    // Unknown HW with 0x3EE -> infer Legacy.
    FSDCapSeen unk_legacy = {0};
    unk_legacy.ap_legacy = true;
    unk_legacy.das_hw3 = true;
    unk_legacy.epas = true;
    r = fsd_capability_eval(unk_legacy, TeslaHW_Unknown);
    CHECK(r.hw_effective == TeslaHW_Legacy, "0x3EE present -> inferred Legacy");
    CHECK(r.has_das, "inferred Legacy: 0x399 is DAS state");

    // ── Vehicle CAN hint from 0x3C2 (a hint, never a verdict) ──
    FSDCapSeen vehicle_pair = {0};
    vehicle_pair.vcleft_switch = true;
    vehicle_pair.body_ui = true;
    r = fsd_capability_eval(vehicle_pair, TeslaHW_HW3);
    CHECK(r.has_vcleft_switch, "0x3C2 seen");
    CHECK(
        r.vehicle_hint == CAP_VEH_HINT_DIRECT,
        "0x3C2 seen: hint Vehicle CAN pair (%d)",
        r.vehicle_hint);

    // Forwarded subset: body frames arrive, 0x3C2 does not.
    FSDCapSeen forwarded = {0};
    forwarded.epas = true;
    forwarded.das_hw3 = true;
    forwarded.ap_control = true;
    forwarded.body_ui = true;
    forwarded.body_window = true;
    r = fsd_capability_eval(forwarded, TeslaHW_HW3);
    CHECK(!r.has_vcleft_switch, "forwarded subset: no 0x3C2");
    CHECK(
        r.vehicle_hint == CAP_VEH_HINT_FORWARDED,
        "body frames but no 0x3C2: hint forwarded subset (%d)",
        r.vehicle_hint);
    CHECK(r.nag_killer == CAP_OK, "forwarded subset still fine for nag killer");

    // 0x3C2 moves the hint and nothing else.
    forwarded.vcleft_switch = true;
    FSDCapReport with_switch = fsd_capability_eval(forwarded, TeslaHW_HW3);
    CHECK(
        with_switch.vehicle_hint == CAP_VEH_HINT_DIRECT &&
            with_switch.nag_killer == r.nag_killer && with_switch.ap_first == r.ap_first &&
            with_switch.fsd_activation == r.fsd_activation &&
            with_switch.soft_engage == r.soft_engage &&
            with_switch.body_control == r.body_control && with_switch.bus_hint == r.bus_hint,
        "0x3C2 changes only the vehicle hint");

    // Neither 0x3C2 nor any body frame: nothing to go on.
    r = fsd_capability_eval(hw3_das, TeslaHW_HW3);
    CHECK(
        r.vehicle_hint == CAP_VEH_HINT_NONE,
        "no body/0x3C2 frames: no vehicle hint (%d)",
        r.vehicle_hint);

    // ── bus hint: steering, no 0x370 -> Chassis/Vehicle-like ──
    FSDCapSeen chassis = {0};
    chassis.steer = true;
    chassis.steer_valid = true;
    r = fsd_capability_eval(chassis, TeslaHW_HW4);
    CHECK(r.bus_hint == CAP_HINT_CHASSIS, "steer w/o 0x370: hint Chassis-like");
    CHECK(r.soft_engage == CAP_OK, "0x129 with a valid angle: soft engage OK");

    // 0x129 arriving with no valid angle in the window (every frame INVALID,
    // like the #211 parked record): neither OK nor missing. It still says
    // which bus this is.
    FSDCapSeen steer_invalid = {0};
    steer_invalid.steer = true;
    r = fsd_capability_eval(steer_invalid, TeslaHW_HW4);
    CHECK(
        r.soft_engage == CAP_NO_VALID,
        "0x129, no valid angle: soft engage no-valid (%d)",
        r.soft_engage);
    CHECK(r.has_steer && !r.has_steer_valid, "0x129, no valid angle: present, not valid");
    CHECK(r.bus_hint == CAP_HINT_CHASSIS, "0x129, no valid angle: hint still Chassis-like");
    // A valid count without the frame itself counting as present is not OK.
    FSDCapSeen valid_only = {0};
    valid_only.steer_valid = true;
    CHECK(
        fsd_capability_eval(valid_only, TeslaHW_HW4).soft_engage == CAP_MISSING,
        "valid flag without 0x129 present: soft engage missing");

    // Empty tap -> everything missing, no hint.
    r = fsd_capability_eval(empty, TeslaHW_HW4);
    CHECK(
        r.nag_killer == CAP_MISSING && r.ap_first == CAP_MISSING &&
            r.fsd_activation == CAP_MISSING && r.soft_engage == CAP_MISSING,
        "empty tap: all missing");
    CHECK(r.bus_hint == CAP_HINT_NONE, "empty tap: no hint");

    // ── Vehicle/body-bus reachability (#128) ──
    // Reachability is RX presence only: ANY of the four body frames -> reachable.
    // No body frame on any of the prior AP/EPAS taps -> not reachable.
    CHECK(r.body_control == CAP_MISSING && !r.has_body, "empty tap: body bus not reachable");
    CHECK(
        fsd_capability_eval(full, TeslaHW_HW4).body_control == CAP_MISSING,
        "AP/Party tap w/o body frames: body bus not reachable");

    // Each body frame alone flips reachability and sets exactly its own flag.
    FSDCapSeen body_ui = {0};
    body_ui.body_ui = true;
    r = fsd_capability_eval(body_ui, TeslaHW_HW4);
    CHECK(r.body_control == CAP_OK && r.has_body, "0x273 alone: body bus reachable");
    CHECK(
        r.body_ui && !r.body_window && !r.body_lights && !r.body_door,
        "0x273 alone: only body_ui flagged");

    FSDCapSeen body_win = {0};
    body_win.body_window = true;
    r = fsd_capability_eval(body_win, TeslaHW_HW4);
    CHECK(
        r.body_control == CAP_OK && r.body_window && !r.body_ui, "0x119 alone: windows reachable");

    FSDCapSeen body_lt = {0};
    body_lt.body_lights = true;
    CHECK(
        fsd_capability_eval(body_lt, TeslaHW_HW4).body_control == CAP_OK,
        "0x3E9 alone: lights reachable");

    FSDCapSeen body_dr = {0};
    body_dr.body_door = true;
    r = fsd_capability_eval(body_dr, TeslaHW_HW4);
    CHECK(
        r.body_control == CAP_OK && r.body_door && !r.body_ui,
        "0x102 alone: mirror read-back reachable");

    // Full Vehicle tap: all four seen -> reachable, all four flags set. Body
    // reachability is independent of AP/EPAS presence.
    FSDCapSeen body_all = {0};
    body_all.body_ui = true;
    body_all.body_door = true;
    body_all.body_window = true;
    body_all.body_lights = true;
    r = fsd_capability_eval(body_all, TeslaHW_HW4);
    CHECK(r.body_control == CAP_OK && r.has_body, "all four body frames: reachable");
    CHECK(
        r.body_ui && r.body_door && r.body_window && r.body_lights,
        "all four body frames: every flag set");
    CHECK(
        r.nag_killer == CAP_MISSING,
        "body-only tap: AP features still missing (independent verdicts)");
}

// ── built-in variant profiles + auto-suggest matcher (#126) ────────────────────
// Helper: build a DAS frame with an 8-byte payload.
static FSDProfileFrame
    pf(uint8_t b0,
       uint8_t b1,
       uint8_t b2,
       uint8_t b3,
       uint8_t b4,
       uint8_t b5,
       uint8_t b6,
       uint8_t b7) {
    FSDProfileFrame f = {{b0, b1, b2, b3, b4, b5, b6, b7}, 8};
    return f;
}

static void test_profile_db(void) {
    // Both seed rows now read AP-state from byte0 low nibble (0x399 HW3/Legacy and
    // 0x39B HW4). The now-identical "Highland (byte0 lo)" row was dropped (#177).
    int i_hw3 = -1, i_hw4 = -1;
    for(int i = 0; i < FSD_PROFILE_DB_COUNT; i++) {
        const FSDProfile* p = &FSD_PROFILE_DB[i];
        if(p->das_id == 0x399 && p->apstate.byte == 0 && p->apstate.shift == 0) i_hw3 = i;
        if(p->das_id == 0x39B && p->apstate.byte == 0 && p->apstate.shift == 0) i_hw4 = i;
    }
    CHECK(FSD_PROFILE_DB_COUNT == 2, "two seed profiles after dropping Highland row");
    CHECK(i_hw3 >= 0 && i_hw4 >= 0, "both byte0 seed profiles present");
    CHECK(
        FSD_PROFILE_DB[i_hw4].apstate.byte == 0 && FSD_PROFILE_DB[i_hw4].apstate.shift == 0 &&
            FSD_PROFILE_DB[i_hw4].apstate.mask == 0x0F,
        "std HW4 is byte0/shift0/mask0x0F");
    // Exactly one candidate per DAS id now.
    int n399 = 0, n39b = 0;
    for(int i = 0; i < FSD_PROFILE_DB_COUNT; i++) {
        if(FSD_PROFILE_DB[i].das_id == 0x399) n399++;
        if(FSD_PROFILE_DB[i].das_id == 0x39B) n39b++;
    }
    CHECK(n399 == 1 && n39b == 1, "one candidate per DAS id (0x399:%d 0x39B:%d)", n399, n39b);
    // needs_override is false for all rows -> the matcher never suggests.
    for(int i = 0; i < FSD_PROFILE_DB_COUNT; i++)
        CHECK(!FSD_PROFILE_DB[i].needs_override, "%s not needs_override", FSD_PROFILE_DB[i].name);
    // handson is byte5/shift2/mask0xF throughout.
    for(int i = 0; i < FSD_PROFILE_DB_COUNT; i++) {
        CHECK(
            FSD_PROFILE_DB[i].handson.byte == 5 && FSD_PROFILE_DB[i].handson.shift == 2 &&
                FSD_PROFILE_DB[i].handson.mask == 0x0F,
            "handson byte5/sh2/0xF: %s",
            FSD_PROFILE_DB[i].name);
    }

    // ── Standard HW3 (0x399): AP-state in byte0 low nibble sweeps 2->3->4 -> unique
    // match, auto-handled -> no suggestion. ──
    FSDProfileFrame hw3[] = {
        pf(0x02, 0, 0, 0, 0, 0x00, 0, 0),
        pf(0x03, 0, 0, 0, 0, 0x04, 0, 0),
        pf(0x04, 0, 0, 0, 0, 0x08, 0, 0),
        pf(0x03, 0, 0, 0, 0, 0x04, 0, 0),
    };
    FSDMatchResult r = fsd_profile_match(0x399, hw3, 4);
    CHECK(r.status == FSD_MATCH_ONE && r.index == i_hw3, "std HW3: unique match");
    CHECK(!fsd_profile_should_suggest(r), "std HW3: no suggestion (auto-handled)");

    // ── Standard HW4 (0x39B): real byte0-low sweep 2->3->6 (byte1 = 0x0A noise) ->
    // unique byte0 match, no suggestion. ──
    FSDProfileFrame hw4[] = {
        pf(0x02, 0x0A, 0, 0, 0, 0x00, 0, 0),
        pf(0x03, 0x0A, 0, 0, 0, 0x04, 0, 0),
        pf(0x06, 0x0A, 0, 0, 0, 0x08, 0, 0),
        pf(0x03, 0x0A, 0, 0, 0, 0x04, 0, 0),
    };
    r = fsd_profile_match(0x39B, hw4, 4);
    CHECK(r.status == FSD_MATCH_ONE && r.index == i_hw4, "std HW4 byte0: unique match");
    CHECK(FSD_PROFILE_DB[r.index].apstate.shift == 0, "byte0-lo match is shift0");
    CHECK(!fsd_profile_should_suggest(r), "std HW4: no suggestion (parser is fine)");

    // ── The OLD byte1-hi layout is no longer matched: byte0 constant 0 fails the
    // "not stuck" test, so a byte1-only sweep -> NONE. ──
    FSDProfileFrame b1only[] = {
        pf(0x00, 0x10, 0, 0, 0, 0x00, 0, 0),
        pf(0x00, 0x20, 0, 0, 0, 0x04, 0, 0),
        pf(0x00, 0x30, 0, 0, 0, 0x08, 0, 0),
        pf(0x00, 0x20, 0, 0, 0, 0x04, 0, 0),
    };
    r = fsd_profile_match(0x39B, b1only, 4);
    CHECK(r.status == FSD_MATCH_NONE, "byte1-only sweep (byte0 const) -> no match");

    // ── No match: parked car, byte0 nibble constant. ──
    FSDProfileFrame parked[] = {
        pf(0x01, 0x11, 0, 0, 0, 0x00, 0, 0),
        pf(0x01, 0x11, 0, 0, 0, 0x00, 0, 0),
        pf(0x01, 0x11, 0, 0, 0, 0x00, 0, 0),
        pf(0x01, 0x11, 0, 0, 0, 0x00, 0, 0),
    };
    r = fsd_profile_match(0x39B, parked, 4);
    CHECK(r.status == FSD_MATCH_NONE, "constant/never-active -> no match");
    CHECK(!fsd_profile_should_suggest(r), "no match -> no suggestion");

    // ── No match: unknown DAS id has no candidates. ──
    r = fsd_profile_match(0x123, hw4, 4);
    CHECK(r.status == FSD_MATCH_NONE, "unknown das_id -> no candidates -> no match");

    // ── Too few frames: below FSD_PROFILE_MIN_FRAMES -> no decision. ──
    r = fsd_profile_match(0x39B, hw4, 2);
    CHECK(r.status == FSD_MATCH_NONE, "too few frames -> no match");

    // ── Out-of-range guard: byte0 lo = 0xA (10) disqualifies the only candidate. ──
    FSDProfileFrame oor[] = {
        pf(0x0A, 0x0A, 0, 0, 0, 0x00, 0, 0),
        pf(0x0A, 0x0A, 0, 0, 0, 0x04, 0, 0),
        pf(0x0A, 0x0A, 0, 0, 0, 0x08, 0, 0),
        pf(0x0A, 0x0A, 0, 0, 0, 0x04, 0, 0),
    };
    r = fsd_profile_match(0x39B, oor, 4);
    CHECK(r.status == FSD_MATCH_NONE, "out-of-range nibble rejected -> no match");
}

// ── black-box .json summary formatter (#124) ─────────────────────────────────
static void test_blackbox_summary(void) {
    uint32_t tl_ts[3] = {0u, 4200u, 4670u};
    uint8_t tl_state[3] = {2u, 6u, 9u};
    FSDBlackboxSummary s;
    memset(&s, 0, sizeof(s));
    s.trigger = "ABORT";
    s.from_state = 6;
    s.to_state = 9;
    s.trigger_rel_ms = 4670u;
    s.window_pre_ms = 10000u;
    s.window_post_ms = 5000u;
    s.frame_count = 12345u;
    s.hw_version = 3; // HW4
    s.hw4_das_status_seen = true;
    s.dual_can = true;
    s.bus0_frames = 8000u;
    s.bus1_frames = 4345u;
    s.nag = true;
    s.abort_guard = true;
    s.tl_ts = tl_ts;
    s.tl_state = tl_state;
    s.tl_count = 3;

    char out[512];
    int n = fsd_blackbox_format_json(out, sizeof(out), &s);
    CHECK(n > 0 && n == (int)strlen(out), "summary returns written length");
    CHECK(strstr(out, "\"trigger\":\"ABORT\"") != NULL, "trigger present");
    CHECK(strstr(out, "\"transition\":\"6->9\"") != NULL, "transition 6->9");
    CHECK(strstr(out, "ABORT 6->9 @ t=4.670s") != NULL, "human detail with rel time");
    CHECK(strstr(out, "\"hw\":\"HW4\"") != NULL, "hw name HW4");
    CHECK(strstr(out, "\"dual_can\":true") != NULL, "dual_can flag");
    CHECK(strstr(out, "\"can0\":8000") != NULL, "can0 frame count");
    CHECK(strstr(out, "\"nag\":true") != NULL, "nag toggle");
    CHECK(strstr(out, "\"abort_guard\":true") != NULL, "abort_guard toggle");
    CHECK(strstr(out, "\"signal_map\":false") != NULL, "signal_map off");
    CHECK(strstr(out, "{\"t\":4670,\"s\":9}") != NULL, "timeline tail entry");
    CHECK(strstr(out, "\"frames\":12345") != NULL, "frame count");

    // Manual mark: from==to, empty timeline still yields valid JSON.
    memset(&s, 0, sizeof(s));
    s.trigger = "MANUAL";
    s.from_state = s.to_state = 2;
    n = fsd_blackbox_format_json(out, sizeof(out), &s);
    CHECK(n > 0, "manual summary non-empty");
    CHECK(strstr(out, "\"transition\":\"2->2\"") != NULL, "manual transition 2->2");
    CHECK(
        strstr(out, "\"detail\":\"MANUAL AP state 2 @ t=0.000s\"") != NULL,
        "manual detail names the AP state once");
    CHECK(strstr(out, "MANUAL 2->2") == NULL, "manual detail has no fake transition");
    CHECK(strstr(out, "\"ap_timeline\":[]") != NULL, "empty timeline");
    CHECK(out[strlen(out) - 1] == '}', "well-terminated JSON");

    // Truncation safety: a tiny buffer must stay NUL-terminated and in-bounds.
    char tiny[16];
    n = fsd_blackbox_format_json(tiny, sizeof(tiny), &s);
    CHECK(n < (int)sizeof(tiny), "tiny buffer not overrun");
    CHECK(tiny[n] == '\0', "tiny buffer NUL-terminated");
}

// ── nag burst/pause + ±1.8 Nm torque cap (#122) ──────────────────────────────
static void test_nag_burst_cap(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.nag_killer = true;
    s.das_hands_on_state = 0xFF; // conservative echo
    CANFRAME in, out;
    zero(&in);
    in.data_lenght = 8;
    in.buffer[4] = 0x00; // handsOnLevel 0 -> not skipped
    in.buffer[6] = 0x05;

    // Torque cap: legacy grip pulses normally hit ~3 Nm (raw ~2350); must clamp to ±1.8 (1870..2230).
    int cap_ok = 1;
    for(int i = 0; i < 400; i++) {
        in.buffer[6] = (in.buffer[6] & 0xF0) | ((5 + i) & 0x0F);
        zero(&out);
        if(!fsd_handle_nag_killer(&s, &in, &out, 1000u + i)) continue;
        int raw = ((out.buffer[2] & 0x0F) << 8) | out.buffer[3];
        if(raw > NAG_TORQUE_RAW_MAX || raw < NAG_TORQUE_RAW_MIN) cap_ok = 0;
    }
    CHECK(cap_ok, "nag torque clamped to +/-1.8 Nm (raw 1870..2230)");

    // Burst/pause: cycle = 1000+1500 = 2500 ms; echo only in the first 1000 ms.
    s.nag_burst = true;
    zero(&out);
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, 1200u) == false,
        "burst: no echo in pause (t%%2500=1200)");
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, 2499u) == false,
        "burst: no echo in pause (t%%2500=2499)");
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, 2500u) != false, "burst: echo in burst (t%%2500=0)");
    CHECK(
        fsd_handle_nag_killer(&s, &in, &out, 3400u) != false,
        "burst: echo in burst (t%%2500=900)");
}

// ── shared stateless ops (china_mode path the Flipper wrapper can't reach) ────
static void test_can_ops(void) {
    uint8_t data[8] = {0};
    data[4] = 0x00; // UI flag clear
    CHECK(tesla_is_fsd_selected(data, 8, false, false) == false, "ops: not selected");
    CHECK(tesla_is_fsd_selected(data, 8, true, false) == true, "ops: force_fsd bypass");
    CHECK(tesla_is_fsd_selected(data, 8, false, true) == true, "ops: china_mode bypass");
    CHECK(tesla_is_fsd_selected(data, 4, false, false) == false, "ops: dlc<5 guard");
    data[4] = 0x40;
    CHECK(tesla_is_fsd_selected(data, 8, false, false) == true, "ops: UI bit6 selected");
    // set_bit / read_mux raw-pointer forms
    uint8_t d2[8] = {0};
    tesla_set_bit(d2, 47, true); // byte5 bit7
    CHECK(d2[5] == 0x80, "ops: set_bit 47 -> 0x80 got 0x%02X", d2[5]);
    d2[0] = 0x0E;
    CHECK(tesla_read_mux(d2) == 6, "ops: read_mux 0x0E&0x07 = 6 got %u", tesla_read_mux(d2));
}

// ── shared additive checksum kernel ───────────────────────────────────────────
static void test_additive_checksum(void) {
    uint8_t d[7] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    // 0x399 -> 0x99 + 0x03 + sum(d) ; sum(d) = 0x1B8 -> +0x9C = 0x254 -> 0x54
    uint16_t s = 0x99 + 0x03;
    for(int i = 0; i < 7; i++)
        s += d[i];
    CHECK(
        tesla_additive_checksum(0x399, d, 7) == (uint8_t)(s & 0xFF),
        "kernel 0x399 got 0x%02X exp 0x%02X",
        tesla_additive_checksum(0x399, d, 7),
        (uint8_t)(s & 0xFF));
    // 2-byte SCCM-style range
    uint8_t two[2] = {0x42, 0x07};
    CHECK(
        tesla_additive_checksum(0x249, two, 2) == (uint8_t)((0x49 + 0x02 + 0x42 + 0x07) & 0xFF),
        "kernel 0x249 2-byte");
    // zero-length: just the folded id bytes
    CHECK(
        tesla_additive_checksum(0x370, d, 0) == (uint8_t)(0x70 + 0x03),
        "kernel len=0 -> id fold only");
}

// ── shared candump-ASCII formatter (capture-first / cracker input) ────────────
static void test_candump_format(void) {
    char buf[48];
    uint8_t d[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0xAA};
    int n = tesla_format_candump_line(buf, sizeof(buf), 1500, "can0", 0x485, d, 8);
    CHECK(strcmp(buf, "(1.500000) can0 485#00112233445566AA\n") == 0, "candump 8-byte: [%s]", buf);
    CHECK(n == (int)strlen(buf), "candump returns byte count (%d vs %zu)", n, strlen(buf));

    // 11-bit ID is zero-padded to 3 hex; short DLC; elapsed seconds carry.
    uint8_t two[2] = {0xDE, 0xAD};
    tesla_format_candump_line(buf, sizeof(buf), 2007, "can0", 0x7, two, 2);
    CHECK(strcmp(buf, "(2.007000) can0 007#DEAD\n") == 0, "candump short: [%s]", buf);

    // DLC clamped to 8 even if a bogus larger value is passed.
    tesla_format_candump_line(buf, sizeof(buf), 0, "can0", 0x3FD, d, 200);
    CHECK(
        strcmp(buf, "(0.000000) can0 3FD#00112233445566AA\n") == 0,
        "candump dlc clamp: [%s]",
        buf);
}

// ── 0x318 GTW_carState OTA detection (gates TX) ───────────────────────────────
static void gtw_feed(FSDState* s, uint8_t b6) {
    CANFRAME f;
    zero(&f);
    f.canId = CAN_ID_GTW_CAR_STATE;
    f.data_lenght = 8;
    f.buffer[6] = b6;
    fsd_handle_gtw_car_state(s, &f);
}

// Feed a byte6 sequence; true if the OTA pause latched at any point.
static bool gtw_feed_seq(FSDState* s, const uint8_t* b6, int n) {
    bool latched = false;
    for(int i = 0; i < n; i++) {
        gtw_feed(s, b6[i]);
        if(s->tesla_ota_in_progress) latched = true;
    }
    return latched;
}

static void test_gtw_car_state(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CANFRAME f;
    zero(&f);
    f.data_lenght = 6; // short frame: ignored, not even the reference byte
    f.buffer[6] = 0x42;
    fsd_handle_gtw_car_state(&s, &f);
    CHECK(!s.ota_last_valid && s.ota_clear_count == 0, "OTA dlc<7 frame ignored");

    // One raw-2 frame no longer pauses TX: it only seeds the reference byte.
    gtw_feed(&s, 0x02);
    CHECK(s.ota_raw_state == 2, "OTA raw_state 2 got %u", s.ota_raw_state);
    CHECK(!s.tesla_ota_in_progress, "OTA single raw-2 frame -> not in_progress");

    // Stable raw 0/1/3 never latch (raw 1 false-positived in #19).
    static const uint8_t other[] = {0x01, 0x00, 0x03};
    for(size_t k = 0; k < sizeof(other); k++) {
        uint8_t seq[10];
        memset(seq, other[k], sizeof(seq));
        memset(&s, 0, sizeof(s));
        CHECK(
            !gtw_feed_seq(&s, seq, (int)sizeof(seq)), "OTA stable raw %u never latches", other[k]);
    }

    // Stable flag 0x42: frame 1 seeds the reference, each repeat is an asserting
    // sample -> latches on the 3rd repeat (4th frame), not the 2nd.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    for(int i = 1; i <= 3; i++) {
        gtw_feed(&s, 0x42);
        CHECK(!s.tesla_ota_in_progress, "OTA stable 0x42 frame %d -> not yet", i);
    }
    CHECK(s.ota_assert_count == 2, "OTA 2 repeats -> assert_count 2 got %u", s.ota_assert_count);
    gtw_feed(&s, 0x42);
    CHECK(s.tesla_ota_in_progress, "OTA stable 0x42 latches on 3rd repeat (frame 4)");
    CHECK(!fsd_can_transmit(&s), "OTA latched -> TX blocked");

    // Latched: a stable non-2 byte releases on exactly the 6th frame.
    for(int i = 1; i <= 5; i++) {
        gtw_feed(&s, 0x41);
        CHECK(s.tesla_ota_in_progress, "OTA clear frame %d -> still latched", i);
    }
    gtw_feed(&s, 0x41);
    CHECK(!s.tesla_ota_in_progress, "OTA releases on 6th non-asserting frame");
    CHECK(fsd_can_transmit(&s), "OTA released -> TX allowed");

    // A raw-2 frame whose byte6 differs from the previous one resets the count.
    memset(&s, 0, sizeof(s));
    gtw_feed(&s, 0x42);
    gtw_feed(&s, 0x42);
    gtw_feed(&s, 0x42);
    CHECK(s.ota_assert_count == 2, "OTA pre-reset assert_count 2 got %u", s.ota_assert_count);
    gtw_feed(&s, 0x46); // raw 2, byte6 changed
    CHECK(
        s.ota_assert_count == 0,
        "OTA changed raw-2 byte6 resets assert_count got %u",
        s.ota_assert_count);
    gtw_feed(&s, 0x46);
    gtw_feed(&s, 0x46);
    CHECK(!s.tesla_ota_in_progress, "OTA 2 repeats after reset -> not yet");
    gtw_feed(&s, 0x46);
    CHECK(s.tesla_ota_in_progress, "OTA 3 repeats after reset -> latched");
}

// 0x318 byte6 on current cars is a rolling counter (+2, always odd), so
// bits[1:0] alternate 1/3 and RX drops can alias it; none of it may latch (#183).
// Real capture, Model S Palladium 2022, consecutive received frames (decimated):
static const uint8_t k_palladium_318_b6[] = {
    0x47, 0x4B, 0x4D, 0x53, 0x45, 0x4F, 0x5D, 0x4B, 0x5F, 0x49, 0x53, 0x5B, 0x43, 0x4B, 0x5B,
    0x43, 0x5F, 0x53, 0x4F, 0x4D, 0x57, 0x41, 0x5F, 0x5B, 0x4D, 0x5D, 0x49, 0x53, 0x4F,
};

static uint32_t xorshift32(uint32_t* x) {
    *x ^= *x << 13;
    *x ^= *x >> 17;
    *x ^= *x << 5;
    return *x;
}

static void test_gtw_car_state_counter(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CHECK(
        !gtw_feed_seq(&s, k_palladium_318_b6, (int)sizeof(k_palladium_318_b6)),
        "OTA Palladium byte6 capture never latches");

    // +2 counter 0x21..0x3F (16 steps), keeping every k-th frame: k=2 pins
    // bits[1:0] at 1, k=16 aliases to a constant 0x21.
    static const int keep_every[] = {1, 2, 3, 16};
    for(size_t k = 0; k < sizeof(keep_every) / sizeof(keep_every[0]); k++) {
        uint8_t seq[128];
        for(int i = 0; i < (int)sizeof(seq); i++)
            seq[i] = (uint8_t)(0x21 + 2 * ((i * keep_every[k]) % 16));
        memset(&s, 0, sizeof(s));
        CHECK(
            !gtw_feed_seq(&s, seq, (int)sizeof(seq)),
            "OTA +2 counter keep 1/%d never latches",
            keep_every[k]);
    }

    // +2 counter under pseudo-random RX drops (fixed seed), keeping ~1/4..3/4.
    for(int keep_q = 1; keep_q <= 3; keep_q++) {
        uint32_t rng = 0x31800u + (uint32_t)keep_q;
        uint8_t seq[256];
        int n = 0;
        for(int i = 0; n < (int)sizeof(seq); i++)
            if((int)(xorshift32(&rng) & 3u) < keep_q) seq[n++] = (uint8_t)(0x21 + 2 * (i % 16));
        memset(&s, 0, sizeof(s));
        CHECK(
            !gtw_feed_seq(&s, seq, n),
            "OTA +2 counter random drops keep %d/4 never latches",
            keep_q);
    }

    // +1 counter 0x00..0xFF kept every 4th frame: phase 2 reads a constant raw 2,
    // but byte6 changes every frame, so it is never an asserting sample.
    for(int phase = 0; phase < 4; phase++) {
        uint8_t seq[128];
        for(int i = 0; i < (int)sizeof(seq); i++)
            seq[i] = (uint8_t)(phase + 4 * i);
        memset(&s, 0, sizeof(s));
        CHECK(
            !gtw_feed_seq(&s, seq, (int)sizeof(seq)),
            "OTA +1 counter every 4th (phase %d) never latches",
            phase);
    }

    // Nor can that aliased raw-2 counter hold a latch: it releases on frame 6.
    memset(&s, 0, sizeof(s));
    for(int i = 0; i < 4; i++)
        gtw_feed(&s, 0x42);
    CHECK(s.tesla_ota_in_progress, "OTA latched before counter");
    for(int i = 1; i <= 5; i++)
        gtw_feed(&s, (uint8_t)(0x02 + 4 * i));
    CHECK(s.tesla_ota_in_progress, "OTA raw-2 counter frame 5 -> still latched");
    gtw_feed(&s, (uint8_t)(0x02 + 4 * 6));
    CHECK(!s.tesla_ota_in_progress, "OTA raw-2 counter releases on frame 6");
}

// ── 0x045 Legacy stalk + 0x3EE Legacy autopilot ───────────────────────────────
static void test_legacy(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CANFRAME f;
    zero(&f);
    f.data_lenght = 2;
    f.buffer[1] = (uint8_t)(0u << 5);
    fsd_handle_legacy_stalk(&s, &f);
    CHECK(s.speed_profile == 2, "legacy stalk pos0 -> 2 got %d", s.speed_profile);
    f.buffer[1] = (uint8_t)(2u << 5);
    fsd_handle_legacy_stalk(&s, &f);
    CHECK(s.speed_profile == 1, "legacy stalk pos2 -> 1 got %d", s.speed_profile);
    f.buffer[1] = (uint8_t)(4u << 5);
    fsd_handle_legacy_stalk(&s, &f);
    CHECK(s.speed_profile == 0, "legacy stalk pos4 -> 0 got %d", s.speed_profile);

    s.force_fsd = true;
    s.speed_profile = 2;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0; // mux0
    CHECK(fsd_handle_legacy_autopilot(&s, &f, 0), "legacy AP mux0 modified");
    CHECK((f.buffer[5] & 0x40) != 0, "legacy AP mux0 bit46");
    CHECK(((f.buffer[6] >> 1) & 0x03) == 2, "legacy AP mux0 speed profile");
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1; // mux1
    f.buffer[2] = 0x08; // bit19 preset
    CHECK(fsd_handle_legacy_autopilot(&s, &f, 0), "legacy AP mux1 modified");
    CHECK((f.buffer[2] & 0x08) == 0, "legacy AP mux1 bit19 cleared");

    // AP-first gate + stability debounce (ev-open-can-tools#66 / v3.0.2-beta.2):
    // no 0x3EE inject until AP is engaged AND has held stable for AP_FIRST_STABLE_MS.
    memset(&s, 0, sizeof(s));
    s.force_fsd = true;
    s.ap_first = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0; // mux0
    s.das_ap_state = 0; // AP not engaged
    CHECK(
        fsd_handle_legacy_autopilot(&s, &f, 2000) == false,
        "legacy AP-first: blocked, AP not engaged");
    s.das_ap_state = 2; // AVAILABLE = offered, NOT engaged (#108)
    s.ap_unstable_tick_ms = 2000;
    CHECK(
        fsd_handle_legacy_autopilot(&s, &f, 3200) == false,
        "legacy AP-first: blocked, AVAILABLE(2) not engaged");
    s.das_ap_state = 3; // ACTIVE_NOMINAL = first engaged, but...
    s.ap_unstable_tick_ms = 2000; // ...only just became stable
    CHECK(
        fsd_handle_legacy_autopilot(&s, &f, 2500) == false,
        "legacy AP-first: blocked, not stable yet (500ms)");
    CHECK(
        fsd_handle_legacy_autopilot(&s, &f, 3000) != false,
        "legacy AP-first: allowed, engaged + stable >= 1000ms");

    // fsd_ap_first_allows() directly
    FSDState g;
    memset(&g, 0, sizeof(g));
    CHECK(fsd_ap_first_allows(&g, 5000) == true, "ap_first off -> always allowed");
    g.ap_first = true;
    g.das_ap_state = 1;
    CHECK(fsd_ap_first_allows(&g, 5000) == false, "ap_first: UNAVAIL-flicker(1) not enough");
    g.das_ap_state = 2; // AVAILABLE — offered but NOT engaged (#108 bug fix)
    g.ap_unstable_tick_ms = 1000;
    CHECK(
        fsd_ap_first_allows(&g, 5000) == false,
        "ap_first: AVAILABLE(2) blocks even when 'stable'");
    g.das_ap_state = 3; // ACTIVE_NOMINAL — first genuinely engaged
    g.ap_unstable_tick_ms = 1000;
    CHECK(
        fsd_ap_first_allows(&g, 1500) == false, "ap_first: engaged but 500ms < debounce -> block");
    CHECK(
        fsd_ap_first_allows(&g, 2000) == true,
        "ap_first: engaged(3) + 1000ms >= debounce -> allow");
    g.das_ap_state = 6;
    CHECK(fsd_ap_first_allows(&g, 5000) == true, "ap_first: active(6) + stable -> allow");

    // Instant Engage (ap_first_edge, #129/#108): inject at engage onset, skip the
    // AP_FIRST_STABLE_MS debounce. Still blocks until AP is actually engaged (>=3).
    FSDState e;
    memset(&e, 0, sizeof(e));
    e.ap_first = true;
    e.ap_first_edge = true;
    e.das_ap_state = 2; // AVAILABLE — offered but NOT engaged
    e.ap_unstable_tick_ms = 5000;
    CHECK(fsd_ap_first_allows(&e, 5000) == false, "instant engage: AVAILABLE(2) still blocks");
    e.das_ap_state = 3; // engaged this instant (tick == now, 0ms held)
    e.ap_unstable_tick_ms = 5000;
    CHECK(
        fsd_ap_first_allows(&e, 5000) == true,
        "instant engage: engaged(3) allows immediately, no debounce");
    e.ap_first_edge = false; // toggle off -> debounce re-applies
    CHECK(
        fsd_ap_first_allows(&e, 5000) == false,
        "instant engage off: engaged(3) but 0ms < debounce -> block");
    e.das_ap_state = 2; // and still blocks at AVAILABLE(2) with edge on
    e.ap_first_edge = true;
    e.ap_unstable_tick_ms = 5000;
    CHECK(
        fsd_ap_first_allows(&e, 5000) == false,
        "instant engage: AVAILABLE(2) blocks in both modes");

    // Minimal Inject (ap_first_minimal, #108/#129): limit AP-enable injection to a
    // brief burst at engage, then stop until disengage — off the abort edge.
    // ap_first_edge on so the AP-First debounce is skipped and each engaged frame
    // is judged purely by the burst budget.
    FSDState m;
    memset(&m, 0, sizeof(m));
    m.ap_first = true;
    m.ap_first_edge = true; // skip debounce; engaged frames allowed immediately
    m.ap_first_minimal = true; // limit to AP_MINIMAL_INJECT_FRAMES per engagement
    m.das_ap_state = 3; // engaged
    m.ap_unstable_tick_ms = 5000;
    CANFRAME mf;
    zero(&mf);
    mf.data_lenght = 8;
    mf.buffer[0] = 1; // mux=1 -> always modified when gates pass (nag suppress)
    for(unsigned i = 0; i < AP_MINIMAL_INJECT_FRAMES; i++) {
        mf.buffer[0] = 1;
        CHECK(
            fsd_handle_legacy_autopilot(&m, &mf, 5000) != false, "minimal: burst frame modified");
    }
    CHECK(m.ap_inject_count == AP_MINIMAL_INJECT_FRAMES, "minimal: burst budget fully spent");
    mf.buffer[0] = 1;
    CHECK(
        fsd_handle_legacy_autopilot(&m, &mf, 5000) == false,
        "minimal: injection stops once budget spent");
    mf.buffer[0] = 1;
    CHECK(
        fsd_handle_legacy_autopilot(&m, &mf, 6000) == false,
        "minimal: stays stopped for rest of engagement");
    // Disengage re-arms: the platform reset (das_ap_state < ENGAGED) zeroes the counter.
    m.das_ap_state = 2; // AVAILABLE -> disengaged
    m.ap_inject_count = 0; // reset block re-arms the burst
    m.das_ap_state = 3; // engage again
    mf.buffer[0] = 1;
    CHECK(
        fsd_handle_legacy_autopilot(&m, &mf, 7000) != false,
        "minimal: re-armed after disengage, modifies again");
    // Toggle off -> continuous injection (every frame), no burst cap.
    FSDState mc;
    memset(&mc, 0, sizeof(mc));
    mc.ap_first = true;
    mc.ap_first_edge = true;
    mc.ap_first_minimal = false; // continuous
    mc.das_ap_state = 3;
    mc.ap_unstable_tick_ms = 5000;
    for(unsigned i = 0; i < AP_MINIMAL_INJECT_FRAMES + 3u; i++) {
        mf.buffer[0] = 1;
        CHECK(
            fsd_handle_legacy_autopilot(&mc, &mf, 5000) != false,
            "minimal off: injection continues every frame");
    }
    CHECK(mc.ap_inject_count == 0, "minimal off: counter untouched when toggle off");

    // fsd_soft_engage_allows() — steer-jerk soft engage (#108)
    FSDState se;
    memset(&se, 0, sizeof(se));
    fsd_steer_set(&se, 90.0f, 1000u); // hard turn
    CHECK(fsd_soft_engage_allows(&se, 1000u) == true, "soft_engage off -> always allowed");
    se.soft_engage = true;
    CHECK(fsd_soft_engage_allows(&se, 1000u) == false, "soft_engage on + turning -> hold");
    CHECK(se.soft_engage_latched == false, "soft_engage: turning does not latch");
    fsd_steer_set(&se, -3.0f, 1100u); // within +/-5 deg of centre
    CHECK(fsd_soft_engage_allows(&se, 1100u) == true, "soft_engage: centred -> allow + latch");
    CHECK(se.soft_engage_latched == true, "soft_engage: latched once centred");
    fsd_steer_set(&se, 120.0f, 1200u); // now turning, but already latched
    CHECK(
        fsd_soft_engage_allows(&se, 1200u) == true,
        "soft_engage: stays allowed once latched (mid-drive turns OK)");
    se.soft_engage_latched = false; // AP drop resets the latch
    CHECK(
        fsd_soft_engage_allows(&se, 1200u) == false,
        "soft_engage: re-holds after latch reset while turning");

    // Unknown angle with 0x129 on the tap: hold, never latch. A centred reading
    // gone stale, then the #211 parked frame (validity INVALID).
    fsd_steer_set(&se, 0.0f, 2000u);
    CHECK(
        fsd_soft_engage_allows(&se, 2000u + FSD_STEER_FRESH_MS + 1u) == false,
        "soft_engage: stale centred angle -> hold");
    CHECK(!se.soft_engage_latched, "soft_engage: stale angle does not latch");
    const uint8_t parked[8] = {0xC4, 0x27, 0x09, 0x12, 0xFF, 0x1F, 0xFF, 0x3F};
    fsd_steer_update(&se, parked, 8, 3000u);
    CHECK(
        fsd_soft_engage_allows(&se, 3000u) == false && !se.soft_engage_latched,
        "soft_engage: INVALID 0x129 -> hold, no latch");
    // The pre-fix decode read 0x129 bytes 0-1: 819..1228 deg on every frame, so
    // Soft Engage never latched on a car with 0x129. #176 frame DA25F45F0020FF3F
    // is -1.2 deg: centred, latches.
    const uint8_t centred[8] = {0xDA, 0x25, 0xF4, 0x5F, 0x00, 0x20, 0xFF, 0x3F};
    fsd_steer_update(&se, centred, 8, 3010u);
    CHECK(
        fsd_soft_engage_allows(&se, 3010u) == true && se.soft_engage_latched,
        "soft_engage: #176 -1.2 deg frame -> latch");

    // No 0x129 on this tap at all: degrade to AP-First-only (latch at once).
    memset(&se, 0, sizeof(se));
    se.soft_engage = true;
    CHECK(
        fsd_soft_engage_allows(&se, 5000u) == true && se.soft_engage_latched,
        "soft_engage: no 0x129 ever -> AP-First-only");
}

// ── 0x145 ESP_status brake ────────────────────────────────────────────────────
static void test_esp_status(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CANFRAME f;
    zero(&f);
    f.data_lenght = 4;
    f.buffer[3] = 0x40; // bits[6:5] = 2 -> Driver_applying_brakes
    fsd_handle_esp_status(&s, &f);
    CHECK(s.driver_brake_applied, "esp brake applied");
    f.buffer[3] = 0x20; // bits[6:5] = 1 -> Not_Applied
    fsd_handle_esp_status(&s, &f);
    CHECK(!s.driver_brake_applied, "esp brake not applied value 1");
    f.buffer[3] = 0x00;
    fsd_handle_esp_status(&s, &f);
    CHECK(!s.driver_brake_applied, "esp no brake");
}

// ── DAS_status parsers (nag-killer gating source) ─────────────────────────────
static void test_das_status(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CANFRAME f;
    zero(&f);
    f.data_lenght = 6;
    f.buffer[0] = 0x03; // HW3 ap_state = 3 (low nibble)
    f.buffer[5] = (uint8_t)(0x05 << 2); // hands_on = 5 (bits[5:2])
    fsd_handle_das_status_hw3(&s, &f);
    CHECK(s.das_ap_state == 3, "hw3 ap_state got %u", s.das_ap_state);
    CHECK(s.das_hands_on_state == 5, "hw3 hands_on got %u", s.das_hands_on_state);
    CHECK(s.das_seen, "hw3 das_seen");

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 7;
    f.buffer[0] = 0x02; // ap_state = 2 (byte0 low nibble)
    f.buffer[1] = 0x6A; // byte1 noise (fusedSpeedLimit etc) — must NOT be read
    f.buffer[5] = (uint8_t)(0x03 << 2); // hands_on = 3
    f.buffer[4] = 0x02; // side_coll_warn = 2 (bits[1:0])
    f.buffer[2] = 0xC0 | 0x05; // fcw = 3 (bits[7:6]), vision = 5 (bits[4:0])
    fsd_handle_das_status_hw4(&s, &f);
    CHECK(s.das_ap_state == 2, "hw4 ap_state got %u", s.das_ap_state);
    CHECK(s.das_hands_on_state == 3, "hw4 hands_on got %u", s.das_hands_on_state);
    CHECK(s.das_side_coll_warn == 2, "hw4 side_coll got %u", s.das_side_coll_warn);
    CHECK(s.das_fcw == 3, "hw4 fcw got %u", s.das_fcw);
    CHECK(s.das_vision_speed_lim == 5, "hw4 vision got %u", s.das_vision_speed_lim);
    CHECK(s.das_seen, "hw4 das_seen");
    CHECK(s.das_hw4_status_seen, "hw4 das_hw4_status_seen set by 0x39B");

    // hw3 parser must NOT set das_hw4_status_seen (it's the 0x39B-only gate).
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 6;
    f.buffer[5] = (uint8_t)(0x02 << 2);
    fsd_handle_das_status_hw3(&s, &f);
    CHECK(!s.das_hw4_status_seen, "hw3 leaves das_hw4_status_seen false");

    // HW4 0x399 hands-on fallback (#100): real captured nag frame from a Juniper
    // RWD where 0x39B is absent — 010adf80b00ce1a5, byte5=0x0C -> hands_on=3.
    memset(&s, 0, sizeof(s));
    s.das_ap_state = 9; // sentinel: fallback must not touch ap_state
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0x01;
    f.buffer[1] = 0x0a;
    f.buffer[2] = 0xdf;
    f.buffer[3] = 0x80;
    f.buffer[4] = 0xb0;
    f.buffer[5] = 0x0c;
    f.buffer[6] = 0xe1;
    f.buffer[7] = 0xa5;
    fsd_handle_das_handsonly_399(&s, &f);
    CHECK(s.das_hands_on_state == 3, "399 fallback hands_on got %u", s.das_hands_on_state);
    CHECK(s.das_seen, "399 fallback sets das_seen");
    CHECK(s.das_ap_state == 9, "399 fallback leaves das_ap_state untouched");
    CHECK(!s.das_hw4_status_seen, "399 fallback does not set the 0x39B gate");

    // too-short frame is ignored
    memset(&s, 0, sizeof(s));
    s.das_hands_on_state = 0xFF;
    zero(&f);
    f.data_lenght = 5;
    fsd_handle_das_handsonly_399(&s, &f);
    CHECK(s.das_hands_on_state == 0xFF, "399 fallback ignores short frame");
}

// HW4/Highland 0x39B DAS_autopilotState = byte0 low nibble (opendbc party BO_923),
// on real frames. byte1 (DAS_fusedSpeedLimit etc) must never move das_ap_state.
static void test_das_status_hw4_byte0(void) {
    FSDState s;
    CANFRAME f;

    // anoblekman Highland HW4, parked (byte1 = 0x0A constant, #177):
    //   01 0A DF E0 B0 08 71 91 -> ap_state 1, no autopark bits (byte3 0xE0)
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0x01;
    f.buffer[1] = 0x0A;
    f.buffer[2] = 0xDF;
    f.buffer[3] = 0xE0;
    f.buffer[4] = 0xB0;
    f.buffer[5] = 0x08;
    f.buffer[6] = 0x71;
    f.buffer[7] = 0x91;
    fsd_handle_das_status_hw4(&s, &f);
    CHECK(s.das_ap_state == 1, "anoblekman parked ap_state=1 got %u", s.das_ap_state);
    CHECK(
        !s.autopark_ready && !s.autopark_parked && !s.autopark_waiting_brake,
        "parked byte3 0xE0 -> no autopark bits");

    //   01 0A DF E0 B0 08 81 A1 -> ap_state 1 (identical-bytes 0x399 copy decodes same)
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0x01;
    f.buffer[1] = 0x0A;
    f.buffer[2] = 0xDF;
    f.buffer[3] = 0xE0;
    f.buffer[4] = 0xB0;
    f.buffer[5] = 0x08;
    f.buffer[6] = 0x81;
    f.buffer[7] = 0xA1;
    fsd_handle_das_status_hw3(&s, &f);
    CHECK(s.das_ap_state == 1, "anoblekman parked 0x399 copy ap_state=1 got %u", s.das_ap_state);

    // Autopark frame: 06 0A DF E5 B0 08 21 4B -> ap_state 6, byte3 0xE5 (bit0+bit2).
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0x06;
    f.buffer[1] = 0x0A;
    f.buffer[2] = 0xDF;
    f.buffer[3] = 0xE5;
    f.buffer[4] = 0xB0;
    f.buffer[5] = 0x08;
    f.buffer[6] = 0x21;
    f.buffer[7] = 0x4B;
    fsd_handle_das_status_hw4(&s, &f);
    CHECK(s.das_ap_state == 6, "autopark frame ap_state=6 got %u", s.das_ap_state);
    CHECK(
        s.autopark_ready && !s.autopark_parked && s.autopark_waiting_brake,
        "autopark byte3 0xE5 -> ready + waitingForBrake");

    // #116 fixtures (0xAccretion Highland, byte1 = 0x10 constant): must decode to
    // their byte0 states, and byte1 noise never changes das_ap_state.
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0x02;
    f.buffer[1] = 0x10;
    f.buffer[2] = 0xDF;
    f.buffer[3] = 0x80;
    f.buffer[4] = 0xB0;
    f.buffer[5] = 0x44;
    f.buffer[6] = 0x50;
    f.buffer[7] = 0x53;
    fsd_handle_das_status_hw4(&s, &f);
    CHECK(s.das_ap_state == 2, "#116 READY byte0=2 got %u", s.das_ap_state);
    f.buffer[0] = 0x03;
    f.buffer[1] = 0x10;
    fsd_handle_das_status_hw4(&s, &f);
    CHECK(s.das_ap_state == 3, "#116 ENGAGED byte0=3 got %u", s.das_ap_state);

    // byte1 noise sweep (0x6A, 0x10, 0x0A) with byte0 held at 3 must never move it.
    static const uint8_t noise[] = {0x6A, 0x10, 0x0A, 0xF0, 0x00};
    for(unsigned i = 0; i < sizeof(noise); i++) {
        zero(&f);
        f.data_lenght = 8;
        f.buffer[0] = 0x03;
        f.buffer[1] = noise[i];
        fsd_handle_das_status_hw4(&s, &f);
        CHECK(
            s.das_ap_state == 3,
            "byte1=0x%02X noise keeps ap_state=3 got %u",
            noise[i],
            s.das_ap_state);
    }
}

// Engaged helper (shared fsd_autopark.h): 3..6 engaged; 0/1/2/8/9/14/15 not.
static void test_das_state_engaged(void) {
    CHECK(!fsd_das_state_engaged(0), "0 DISABLED not engaged");
    CHECK(!fsd_das_state_engaged(1), "1 UNAVAILABLE not engaged");
    CHECK(!fsd_das_state_engaged(2), "2 AVAILABLE not engaged");
    CHECK(fsd_das_state_engaged(3), "3 ACTIVE_NOMINAL engaged");
    CHECK(fsd_das_state_engaged(4), "4 ACTIVE_RESTRICTED engaged");
    CHECK(fsd_das_state_engaged(5), "5 ACTIVE_NAV engaged");
    CHECK(fsd_das_state_engaged(6), "6 ACTIVE_FSD engaged");
    CHECK(!fsd_das_state_engaged(8), "8 ABORTING not engaged");
    CHECK(!fsd_das_state_engaged(9), "9 ABORTED not engaged");
    CHECK(!fsd_das_state_engaged(14), "14 FAULT not engaged");
    CHECK(!fsd_das_state_engaged(15), "15 SNA not engaged");
}

// In-car Autopark TX pause (#180). Drives fsd_autopark_update() + fsd_can_transmit().
static void ap_update(FSDState* s, uint8_t state6, uint32_t now) {
    s->das_ap_state = state6;
    fsd_autopark_update(s, now);
}
// Feed one 0x257 DI_speed frame through the real parser, stamp freshness the way
// the RX loop does, then run the Autopark update at the same instant.
static void ap_speed(FSDState* s, uint8_t b1, uint8_t b2, uint32_t now) {
    CANFRAME f;
    zero(&f);
    f.canId = CAN_ID_DI_SPEED;
    f.data_lenght = 8;
    f.buffer[1] = b1;
    f.buffer[2] = b2;
    fsd_handle_di_speed(s, &f);
    s->last_speed_tick_ms = now;
    fsd_autopark_update(s, now);
}
static void test_autopark(void) {
    FSDState s;

    // ── Autopark sequence: parked -> autoparkReady -> state 6 held -> back to 1.
    // Blocks exactly during the episode, releases after.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 1, 100); // parked, no bits
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "parked: no block");
    s.autopark_ready = true; // autoparkReady 0->1 (still state 1)
    ap_update(&s, 1, 1000);
    CHECK(!s.autopark_tx_block, "autoparkReady set but not state 6 yet: no episode");
    s.autopark_ready = true;
    s.autopark_waiting_brake = true;
    ap_update(&s, 6, 1200); // 1->6 with bits (byte3 0xE5)
    CHECK(s.autopark_episode && s.autopark_tx_block, "state 6 autopark: blocked");
    CHECK(!fsd_can_transmit(&s), "autopark episode blocks fsd_can_transmit");
    ap_update(&s, 6, 8000); // held ~7 s
    CHECK(s.autopark_tx_block, "held at 6: still blocked");
    s.autopark_ready = false;
    s.autopark_waiting_brake = false;
    ap_update(&s, 1, 22000); // completes, 6->1
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "back to 1: episode ended, released");
    CHECK(fsd_can_transmit(&s), "released: TX allowed again");

    // ── maneuver_recent path: a maneuver bit seen ~1.1 s before the 6, the
    // 6-frame's byte3 carries no bit, episode still opens (reporter's trace).
    // Entering from an already-engaged state so prev<=2 can't open it instead.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 3, 100);
    s.autopark_waiting_brake = true;
    ap_update(&s, 3, 1000); // maneuver bit seen at t=1000
    s.autopark_waiting_brake = false;
    ap_update(&s, 6, 2100); // 3->6, no bit now, recent (1.1 s)
    CHECK(s.autopark_episode && s.autopark_tx_block, "maneuver_recent opens episode on 3->6");

    // ── autoparkReady recency does NOT open an episode entering from an engaged
    // state (AP re-engaging near parked cars, not Autopark) (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 3, 100);
    s.autopark_ready = true;
    ap_update(&s, 3, 1000); // spot found while engaged
    s.autopark_ready = false;
    ap_update(&s, 6, 2100); // 3->6, ready was recent
    CHECK(
        !s.autopark_episode && !s.autopark_tx_block, "autoparkReady recency on 3->6: no episode");

    // ── FSD sequence 2->3->6 with autopark bits clear NEVER blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 2, 100);
    ap_update(&s, 3, 200);
    ap_update(&s, 6, 300);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "FSD 2->3->6 no bits: never blocks");
    CHECK(fsd_can_transmit(&s), "FSD engaged at 6: TX allowed");

    // ── 2->6 with a MISSED 3 at 0 km/h: blocks, then fresh speed past the release
    // speed ENDS the episode; slowing back down in the same 6 does not re-block (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.speed_seen = true;
    s.last_speed_tick_ms = 100;
    s.vehicle_speed_kph = 0.0f;
    ap_update(&s, 2, 100);
    ap_update(&s, 6, 200); // prev 2 (<=2) -> episode
    CHECK(s.autopark_tx_block, "2->6 missed-3 at 0 km/h: blocked");

    // Stale speed keeps the block (fail safe).
    ap_update(&s, 6, 1500); // last speed 1.4 s ago -> stale
    CHECK(s.autopark_tx_block, "stale speed keeps block (fail safe)");

    s.vehicle_speed_kph = 25.0f;
    s.last_speed_tick_ms = 1900;
    ap_update(&s, 6, 2000); // fresh speed 25 km/h
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "fresh 25 km/h ends the false episode");
    CHECK(fsd_can_transmit(&s), "released by speed: TX allowed");
    s.vehicle_speed_kph = 5.0f;
    s.last_speed_tick_ms = 2900;
    ap_update(&s, 6, 3000); // back to 5 km/h, still in 6
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "5 km/h after release, same 6: no block");
    ap_update(&s, 6, 5000); // speed stale again
    CHECK(!s.autopark_tx_block, "stale speed after release: episode stays ended");

    // ── SNA speed must NOT release. DI_vehicleSpeed raw 0xFFF (4095) is SNA and
    // decodes to 287.6 kph; only a fresh VALID reading past the release speed
    // releases (ending the episode, so a later SNA reading cannot re-open it, #176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.autopark_ready = true;
    ap_update(&s, 1, 100);
    ap_update(&s, 6, 200); // autopark episode
    CHECK(s.autopark_tx_block, "SNA test: episode blocked before any speed");
    ap_speed(&s, 0xF0, 0xFF, 300); // raw 0xFFF = SNA, fresh
    CHECK(
        s.vehicle_speed_kph > 287.5f && s.vehicle_speed_kph < 287.7f,
        "SNA raw 0xFFF decodes to %.2f kph (287.6)",
        (double)s.vehicle_speed_kph);
    CHECK(s.autopark_tx_block && !fsd_can_transmit(&s), "fresh SNA speed keeps the block");
    ap_speed(&s, 0xF0, 0xFD, 400); // raw 4063 = 285.04, invalid
    CHECK(s.autopark_tx_block, "raw 4063 (above max valid) keeps the block");
    ap_speed(&s, 0xD0, 0x32, 500); // raw 813 = 25.04 kph, fresh
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "fresh valid 25 kph releases");
    ap_speed(&s, 0xF0, 0xFF, 600); // back to SNA
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "SNA after release: episode stays ended");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.autopark_ready = true;
    ap_update(&s, 1, 100);
    ap_update(&s, 6, 200);
    ap_speed(&s, 0xE0, 0xFD, 300); // raw 4062 = 284.96, max valid
    CHECK(!s.autopark_tx_block, "raw 4062 (max valid 284.96 kph) releases");
    // No speed at all keeps the block.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 2, 100);
    ap_update(&s, 6, 200);
    CHECK(!s.speed_seen && s.autopark_tx_block, "no speed keeps block (fail safe)");

    // ── maneuver bit rising mid-6 at low speed blocks (episode from FSD-6);
    // autoparkReady alone (spot found) does not (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 3, 100);
    ap_update(&s, 6, 200); // FSD 3->6, no bits -> no episode
    CHECK(!s.autopark_tx_block, "FSD 3->6 no bits: no block");
    s.autopark_ready = true; // spot found mid-6
    ap_update(&s, 6, 250);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "autoparkReady alone mid-6: no block");
    s.autopark_parked = true; // a maneuver bit rises mid-6
    ap_update(&s, 6, 300);
    CHECK(
        s.autopark_episode && s.autopark_tx_block,
        "maneuver bit rising mid-6 at low speed: blocks");

    // ── ignore_ota does NOT override the autopark block.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.ignore_ota = true;
    s.autopark_ready = true;
    ap_update(&s, 6, 200);
    CHECK(
        s.autopark_tx_block && !fsd_can_transmit(&s),
        "ignore_ota does not override autopark block");

    // ── listen-only still blocks (fsd_can_transmit false regardless).
    s.op_mode = OpMode_ListenOnly;
    CHECK(!fsd_can_transmit(&s), "listen-only blocks TX (autopark or not)");
}

// #176: state 6 is the NORMAL engaged state on many cars and DAS_autoparkReady
// (spot found) toggles in ordinary city driving. Real 0x39B frames + 0x257 frames
// through the parsers, then fsd_autopark_update() + fsd_can_transmit().
static const uint8_t k_das_parked[8] = {0x01, 0x0A, 0xDF, 0xE0, 0xB0, 0x08, 0x71, 0x91};
static const uint8_t k_das_autopark[8] = {0x06, 0x0A, 0xDF, 0xE5, 0xB0, 0x08, 0x21, 0x4B};
static void ap_frame(FSDState* s, const uint8_t b[8], uint32_t now) {
    CANFRAME f;
    zero(&f);
    f.canId = CAN_ID_DAS_STATUS;
    f.data_lenght = 8;
    memcpy(f.buffer, b, 8);
    fsd_handle_das_status_hw4(s, &f);
    fsd_autopark_update(s, now);
}
// byte0 = DAS_autopilotState, byte3 = autopark bits (0xE0 none, 0xE1 ready).
static void ap_das(FSDState* s, uint8_t b0, uint8_t b3, uint32_t now) {
    const uint8_t b[8] = {b0, 0x0A, 0xDF, b3, 0xB0, 0x08, 0x00, 0x00};
    ap_frame(s, b, now);
}
// DI_vehicleSpeed raw = (kph + 40) / 0.08 -> byte1[7:4] + byte2.
static void ap_kph(FSDState* s, float kph, uint32_t now) {
    uint16_t raw = (uint16_t)((kph + 40.0f) / 0.08f + 0.5f);
    ap_speed(s, (uint8_t)((raw & 0x0Fu) << 4), (uint8_t)(raw >> 4), now);
}
static void test_autopark_176(void) {
    FSDState s;
    int blocked;

    // ── AP engaged 3->6 at 30 km/h, then city traffic: speed wandering 0..30
    // km/h, autoparkReady on/off repeatedly while in 6. Never blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 30.0f, 0);
    ap_das(&s, 0x02, 0xE0, 50);
    ap_das(&s, 0x03, 0xE0, 100);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.autopark_tx_block, "#176 AP 3->6 at 30 km/h: no block");
    blocked = 0;
    for(uint32_t i = 0; i < 600; i++) { // 60 s in 100 ms steps
        uint32_t t = 300 + i * 100;
        uint32_t ph = i % 60; // 0 -> 30 -> 0 km/h
        ap_kph(&s, (float)(ph < 30 ? ph : 60 - ph), t);
        ap_das(&s, 0x06, ((i / 7) & 1u) ? 0xE1 : 0xE0, t + 10);
        if(s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(
        blocked == 0,
        "#176 city traffic in 6, autoparkReady toggling: %d blocked frames",
        blocked);

    // ── 2->6 (missed 3) at 40 km/h fresh: moving at entry -> no episode, and a
    // stop with autoparkReady set later in the same 6 still does not block.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 40.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE1, 200);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "2->6 at 40 km/h: no episode");
    ap_kph(&s, 0.0f, 300);
    ap_das(&s, 0x06, 0xE1, 350);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "2->6 at 40, then stop + ready: no block");

    // ── AP re-engaging from a standstill: 2->3->6 at 0..5 km/h with autoparkReady
    // set (byte3 bit0 only, no maneuver bit), never exceeding 20 km/h. Entering 6
    // from an engaged state (prev 3), so only autoparkReady is set -> NOT Autopark,
    // never blocks even at a crawl near parked cars (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_das(&s, 0x02, 0xE1, 150); // AVAILABLE, spot found
    ap_das(&s, 0x03, 0xE1, 200); // engages -> ACTIVE_NOMINAL
    ap_das(&s, 0x06, 0xE1, 300); // 3->6 with only autoparkReady
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "2->3->6 at 0 km/h + ready: no episode");
    blocked = 0;
    for(uint32_t i = 0; i < 300; i++) { // 30 s crawling 0..5 km/h
        uint32_t t = 400 + i * 100;
        ap_kph(&s, (float)(i % 6), t);
        ap_das(&s, 0x06, ((i / 5) & 1u) ? 0xE1 : 0xE0, t + 10);
        if(s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(
        blocked == 0, "re-engage crawl in 6, autoparkReady toggling: %d blocked frames", blocked);

    // Entry speed boundary: 8.0 km/h is still a standstill start, 8.08 is moving.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 8.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_tx_block, "2->6 at 8.0 km/h (<= ENTRY_MAX): blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 8.08f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.autopark_tx_block, "2->6 at 8.08 km/h (> ENTRY_MAX): no episode");

    // ── 2->6 at 0 km/h with no maneuver bit (FSD engaged from a standstill in
    // stop-and-go, #176): blocked; 12 km/h keeps it; > 12 km/h ends it; back to
    // 5 km/h in the same 6 is NOT blocked again. A maneuver bit re-opens it, and
    // an episode with maneuver evidence keeps the 20 km/h rule.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_episode && s.autopark_tx_block, "2->6 at 0 km/h: blocked");
    CHECK(!s.autopark_evidence, "2->6 at 0 km/h, no maneuver bit: no evidence");
    ap_kph(&s, 12.0f, 300);
    CHECK(s.autopark_tx_block, "no maneuver bit: 12 km/h keeps the block");
    ap_kph(&s, 12.5f, 400);
    CHECK(
        !s.autopark_episode && !s.autopark_tx_block,
        "no maneuver bit: > 12 km/h ends the episode");
    ap_kph(&s, 5.0f, 500);
    ap_das(&s, 0x06, 0xE1, 550);
    CHECK(
        !s.autopark_tx_block && fsd_can_transmit(&s),
        "back to 5 km/h + autoparkReady in the same 6: not re-blocked");
    ap_das(&s, 0x06, 0xE5, 600); // waitingForBrake rises
    CHECK(
        s.autopark_episode && s.autopark_tx_block, "waitingForBrake mid-6 after release: blocked");
    ap_das(&s, 0x06, 0xE0, 700);
    CHECK(s.autopark_tx_block, "maneuver episode holds after the bit clears (still slow)");
    ap_kph(&s, 15.0f, 750);
    CHECK(
        s.autopark_tx_block && s.autopark_evidence, "maneuver evidence: 15 km/h keeps the block");
    ap_kph(&s, 20.0f, 760);
    CHECK(s.autopark_tx_block, "maneuver evidence: 20 km/h keeps the block");
    ap_kph(&s, 25.0f, 800);
    CHECK(!s.autopark_tx_block, "> 20 km/h ends the maneuver episode too");
    ap_kph(&s, 3.0f, 900);
    ap_das(&s, 0x06, 0xE2, 950); // autoParked rises
    CHECK(s.autopark_tx_block, "autoParked mid-6 after release: blocked");
    ap_das(&s, 0x02, 0xE0, 1000); // leaves 6
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "leaving 6 ends the episode");
    ap_kph(&s, 0.0f, 1100);
    ap_das(&s, 0x06, 0xE0, 1150);
    CHECK(s.autopark_tx_block, "new 2->6 at a standstill: blocked (fresh state-6 period)");

    // ── Unknown / stale / SNA speed at a 1->6 entry: fail safe, blocked.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_das(&s, 0x01, 0xE0, 100);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.speed_seen && s.autopark_tx_block, "1->6, speed never seen: blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 30.0f, 100);
    ap_das(&s, 0x01, 0xE0, 1500);
    ap_das(&s, 0x06, 0xE0, 2000); // 30 km/h reading is 1.9 s old
    CHECK(s.autopark_tx_block, "1->6, stale 30 km/h: blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_speed(&s, 0xF0, 0xFF, 100); // SNA, fresh
    ap_das(&s, 0x01, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_tx_block, "1->6, SNA speed: blocked");
    // Same for the lower no-evidence release: SNA or stale speed never ends it.
    ap_speed(&s, 0xF0, 0xFF, 300); // SNA again, fresh
    CHECK(s.autopark_tx_block, "no evidence, fresh SNA: still blocked");
    ap_kph(&s, 30.0f, 400);
    CHECK(!s.autopark_tx_block, "no evidence, fresh 30 km/h: released");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    s.vehicle_speed_kph = 30.0f; // a reading, but 1.2 s old
    s.last_speed_tick_ms = 300;
    ap_das(&s, 0x06, 0xE0, 1500);
    CHECK(s.autopark_tx_block, "no evidence, stale 30 km/h: still blocked");

    // ── #176 stop-and-go: FSD engaged at a standstill (2->6, no bits), creeping
    // 0..10 km/h for 20 s, then away at 15 km/h and back to stop-and-go. Blocked
    // only until the first fresh reading over 12 km/h.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    blocked = 0;
    for(uint32_t i = 0; i < 200; i++) {
        uint32_t t = 300 + i * 100;
        ap_kph(&s, (float)(i % 11), t);
        ap_das(&s, 0x06, 0xE0, t + 10);
        if(s.autopark_tx_block) blocked++;
    }
    CHECK(blocked == 200, "stop-and-go below 12 km/h: blocked %d/200", blocked);
    ap_kph(&s, 15.0f, 20400);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "stop-and-go: 15 km/h releases");
    blocked = 0;
    for(uint32_t i = 0; i < 200; i++) {
        uint32_t t = 20500 + i * 100;
        ap_kph(&s, (float)(i % 11), t);
        ap_das(&s, 0x06, ((i / 7) & 1u) ? 0xE1 : 0xE0, t + 10);
        if(s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(blocked == 0, "stop-and-go after release, same 6: %d blocked frames", blocked);

    // ── An episode opened by its maneuver bit (1->6 on byte3 0xE5, the #180
    // entry) has evidence from the start: 15 km/h keeps the block, > 20 ends it.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_frame(&s, k_das_parked, 150);
    ap_frame(&s, k_das_autopark, 200);
    CHECK(s.autopark_tx_block && s.autopark_evidence, "1->6 with waitingForBrake: evidence");
    ap_kph(&s, 15.0f, 300);
    ap_frame(&s, k_das_autopark, 310);
    CHECK(s.autopark_tx_block, "maneuver-opened episode: 15 km/h keeps the block");
    ap_kph(&s, 21.0f, 400);
    CHECK(!s.autopark_tx_block, "maneuver-opened episode: > 20 km/h releases");

    // ── Real in-car Autopark (#180 trace): parked, autoparkReady rises (byte3
    // 0xE1), ~1.1 s later 1->6 on 06 0A DF E5 B0 08 21 4B at 0..5 km/h, held
    // 20 s, then 6->1. Blocked the whole time in 6, released after.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    uint8_t ready[8];
    memcpy(ready, k_das_parked, 8);
    ready[3] = 0xE1;
    uint32_t t = 0;
    for(int i = 0; i < 10; i++, t += 100) {
        ap_kph(&s, 0.0f, t);
        ap_frame(&s, k_das_parked, t + 10);
    }
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "real trace parked: no block");
    for(int i = 0; i < 11; i++, t += 100) {
        ap_kph(&s, 0.0f, t);
        ap_frame(&s, ready, t + 10);
    }
    CHECK(
        s.das_ap_state == 1 && s.autopark_ready && !s.autopark_tx_block,
        "real trace autoparkReady at state 1: no episode yet");
    blocked = 0;
    for(int i = 0; i < 200; i++, t += 100) {
        ap_kph(&s, (float)(i % 6), t);
        ap_frame(&s, k_das_autopark, t + 10);
        if(s.autopark_tx_block && !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(blocked == 200, "real Autopark held 20 s at 0..5 km/h: blocked %d/200", blocked);
    ap_kph(&s, 0.0f, t);
    ap_frame(&s, k_das_parked, t + 10);
    CHECK(
        !s.autopark_episode && !s.autopark_tx_block && fsd_can_transmit(&s),
        "real trace 6->1: released");
}

// ── 0x7FF tier parse + active override ────────────────────────────────────────
static void test_gtw_tier(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.gtw_autopilot_tier = -1;
    CANFRAME f;
    zero(&f);
    f.data_lenght = 6;
    f.buffer[0] = 2; // mux 2
    f.buffer[5] = (uint8_t)(0x03 << 2); // tier = 3
    fsd_handle_gtw_autopilot_tier(&s, &f);
    CHECK(s.gtw_autopilot_tier == 3, "gtw tier parse got %d", s.gtw_autopilot_tier);
    s.gtw_autopilot_tier = -1;
    f.buffer[0] = 1; // wrong mux ignored
    fsd_handle_gtw_autopilot_tier(&s, &f);
    CHECK(s.gtw_autopilot_tier == -1, "gtw tier mux!=2 ignored");

    s.gtw_tier_override = true;
    zero(&f);
    f.data_lenght = 6;
    f.buffer[0] = 2;
    f.buffer[5] = 0x00;
    CHECK(fsd_handle_gtw_tier_override(&s, &f), "tier override modifies");
    CHECK(
        ((f.buffer[5] >> 2) & 0x07) == 3, "tier override -> 3 got %u", (f.buffer[5] >> 2) & 0x07);
    s.gtw_tier_override = false;
    f.buffer[5] = 0x00;
    CHECK(fsd_handle_gtw_tier_override(&s, &f) == false, "tier override disabled -> noop");
}

// ── 0x3F8 driver-assist override bit map ──────────────────────────────────────
static void test_driver_assist(void) {
    FSDState s;
    CANFRAME f;

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    s.assist_dev_mode = true;
    CHECK(fsd_handle_driver_assist_override(&s, &f), "assist dev modifies");
    CHECK((f.buffer[0] & 0x20) != 0, "assist dev bit5");

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    s.assist_nav_enable = true;
    fsd_handle_driver_assist_override(&s, &f);
    CHECK((f.buffer[1] & 0x20) != 0, "assist nav bit13");
    CHECK((f.buffer[6] & 0x01) != 0, "assist nav bit48");
    CHECK((f.buffer[6] & 0x02) != 0, "assist nav bit49");

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    s.assist_lhd_override = true;
    fsd_handle_driver_assist_override(&s, &f);
    CHECK((f.buffer[5] & 0x01) != 0, "assist lhd bit40 set");
    CHECK((f.buffer[5] & 0x02) == 0, "assist lhd bit41 clear");

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    f.buffer[5] = 0x08; // bit43 preset
    s.assist_telemetry_off = true;
    fsd_handle_driver_assist_override(&s, &f);
    CHECK((f.buffer[5] & 0x08) == 0, "assist telemetry bit43 cleared");

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    CHECK(fsd_handle_driver_assist_override(&s, &f) == false, "assist no flags -> noop");
}

// ── 0x3F8 RHD driving-side override (#66) ─────────────────────────────────────
static void test_rhd_override(void) {
    FSDState s;
    CANFRAME f;

    // RHD on: bit41 set, bit40 clear
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    s.assist_rhd_override = true;
    CHECK(fsd_handle_driver_assist_override(&s, &f), "rhd modifies");
    CHECK((f.buffer[5] & 0x02) != 0, "rhd bit41 set");
    CHECK((f.buffer[5] & 0x01) == 0, "rhd bit40 clear");

    // RHD off: bit41 not set, no modification
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    s.assist_rhd_override = false;
    CHECK(fsd_handle_driver_assist_override(&s, &f) == false, "rhd off -> noop");
    CHECK((f.buffer[5] & 0x02) == 0, "rhd off bit41 clear");
}

// ── Telemetry Off (experimental): reachable 0x3F8 + 0x3FD mux1 flag clears ─────
// 0x3F8 UI_driverAssistControl clears bits 19/42/43/44/55; 0x3FD mux1 clears
// bits 48/50. Plain bit-clears, no checksum. With the toggle off, none move.
static void test_telemetry_off(void) {
    FSDState s;
    CANFRAME f;

    // 0x3F8: with telemetry-off ON, bits 19/42/43/44/55 are cleared.
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    f.buffer[2] = 0x08; // bit19 preset
    f.buffer[5] = 0x04 | 0x08 | 0x10; // bits 42/43/44 preset
    f.buffer[6] = 0x80; // bit55 preset
    s.assist_telemetry_off = true;
    CHECK(fsd_handle_driver_assist_override(&s, &f), "0x3F8 telemetry-off modifies");
    CHECK((f.buffer[2] & 0x08) == 0, "0x3F8 bit19 cleared");
    CHECK((f.buffer[5] & 0x04) == 0, "0x3F8 bit42 cleared");
    CHECK((f.buffer[5] & 0x08) == 0, "0x3F8 bit43 cleared");
    CHECK((f.buffer[5] & 0x10) == 0, "0x3F8 bit44 cleared");
    CHECK((f.buffer[6] & 0x80) == 0, "0x3F8 bit55 cleared");

    // 0x3F8: with telemetry-off OFF, those bits are untouched.
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 8;
    f.buffer[2] = 0x08;
    f.buffer[5] = 0x04 | 0x08 | 0x10;
    f.buffer[6] = 0x80;
    s.assist_telemetry_off = false;
    fsd_handle_driver_assist_override(&s, &f);
    CHECK((f.buffer[2] & 0x08) != 0, "0x3F8 bit19 untouched when off");
    CHECK((f.buffer[5] & 0x1C) == 0x1C, "0x3F8 bits42/43/44 untouched when off");
    CHECK((f.buffer[6] & 0x80) != 0, "0x3F8 bit55 untouched when off");

    // 0x3FD mux1: with telemetry-off ON, bits 48/50 are cleared.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1; // mux1
    f.buffer[6] = 0x01 | 0x04; // bits 48/50 preset
    s.assist_telemetry_off = true;
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "0x3FD mux1 telemetry-off modifies");
    CHECK((f.buffer[6] & 0x01) == 0, "0x3FD bit48 cleared");
    CHECK((f.buffer[6] & 0x04) == 0, "0x3FD bit50 cleared");

    // 0x3FD mux1: with telemetry-off OFF, bits 48/50 are untouched.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1;
    f.buffer[6] = 0x01 | 0x04;
    s.assist_telemetry_off = false;
    fsd_handle_autopilot_frame(&s, &f, 0);
    CHECK((f.buffer[6] & 0x01) != 0, "0x3FD bit48 untouched when off");
    CHECK((f.buffer[6] & 0x04) != 0, "0x3FD bit50 untouched when off");
}

// ── AP branch/tier selector (0x3FD mux1 UI_apmv3Branch, bits 40-42) ──────────
// Experimental, opt-in, non-persistent. 0xFF sentinel = OFF (frame untouched);
// 0-5 write the branch/tier into byte5 bits 0-2.
static void test_apmv3_branch(void) {
    FSDState s;
    CANFRAME f;

    // HW4 mux1: apmv3_branch=4 (EAP) -> byte5 bits0-2 == 4.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW4;
    s.apmv3_branch = 4;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1; // mux1
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW4 mux1 apmv3 modifies");
    CHECK((f.buffer[5] & 0x07) == 4, "HW4 mux1 apmv3=EAP byte5[0:2]=%u exp 4", f.buffer[5] & 0x07);

    // HW3 mux1: apmv3_branch=4 (EAP) -> byte5 bits0-2 == 4.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    s.apmv3_branch = 4;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1; // mux1
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW3 mux1 apmv3 modifies");
    CHECK((f.buffer[5] & 0x07) == 4, "HW3 mux1 apmv3=EAP byte5[0:2]=%u exp 4", f.buffer[5] & 0x07);

    // OFF (0xFF sentinel): byte5 bits0-2 left untouched (preset 0x05 preserved).
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW4;
    s.apmv3_branch = 0xFF;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1; // mux1
    f.buffer[5] = 0x05; // preset low 3 bits to prove they survive
    fsd_handle_autopilot_frame(&s, &f, 0);
    CHECK(
        (f.buffer[5] & 0x07) == 0x05,
        "HW4 mux1 apmv3 OFF leaves byte5[0:2]=%u exp 5",
        f.buffer[5] & 0x07);

    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    s.apmv3_branch = 0xFF;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1; // mux1
    f.buffer[5] = 0x05;
    fsd_handle_autopilot_frame(&s, &f, 0);
    CHECK(
        (f.buffer[5] & 0x07) == 0x05,
        "HW3 mux1 apmv3 OFF leaves byte5[0:2]=%u exp 5",
        f.buffer[5] & 0x07);
}

// ── 0x7FF GTW Config Replay: learn -> arm -> replay ───────────────────────────
static void test_gtw_shield(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    // Learning: feed all 8 mux frames; none transmit, arms after the 8th.
    for(uint8_t m = 0; m < 8; m++) {
        CANFRAME f;
        zero(&f);
        f.data_lenght = 8;
        f.buffer[0] = m; // mux
        f.buffer[3] = 0xAA; // "healthy" payload
        CHECK(fsd_handle_gtw_shield(&s, &f) == false, "shield learning -> false (mux %u)", m);
    }
    CHECK(s.gtw_shield_armed, "shield armed after 8 mux snapshots");

    // Armed, unchanged frame -> no replay.
    CANFRAME ok;
    zero(&ok);
    ok.data_lenght = 8;
    ok.buffer[0] = 0;
    ok.buffer[3] = 0xAA;
    CHECK(fsd_handle_gtw_shield(&s, &ok) == false, "shield unchanged -> false");

    // Armed, tampered frame -> replay healthy snapshot.
    CANFRAME bad;
    zero(&bad);
    bad.data_lenght = 8;
    bad.buffer[0] = 0;
    bad.buffer[3] = 0xBB; // gateway changed it
    CHECK(fsd_handle_gtw_shield(&s, &bad), "shield tampered -> true (replay)");
    CHECK(bad.buffer[3] == 0xAA, "shield restored byte3 to 0xAA got 0x%02X", bad.buffer[3]);
    CHECK(s.gtw_shield_blocks == 1, "shield block counted");
}

// ── 0x3C2 Scroll-Press AP engage: timed state machine ─────────────────────────
// Phase timings mirror the #defines in fsd_handler.c (PRESS1=250, SCROLL1=150,
// PRESS2=250 ms). swcRightPressed -> byte1 bits[5:4]; scrollTicks -> byte3 bits[5:0].
static void mux1(CANFRAME* f) {
    zero(f);
    f->data_lenght = 8;
    f->buffer[0] = 1; // VCLEFT_switchStatusIndex mux=1
}

static void test_scroll_press(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW4;
    s.op_mode = OpMode_Service;
    s.scroll_press_ap = true;

    CANFRAME f;

    // Gates: HW4-only, Service-only, mux==1 only.
    s.hw_version = TeslaHW_HW3;
    mux1(&f);
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 0) == false, "scroll gated: not HW4");
    s.hw_version = TeslaHW_HW4;
    s.op_mode = OpMode_Active;
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 0) == false, "scroll gated: not Service");
    s.op_mode = OpMode_Service;
    mux1(&f);
    f.buffer[0] = 2; // wrong mux
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 0) == false, "scroll gated: mux!=1");

    // Arm on AP UNAVAIL(0), no fire yet.
    s.das_ap_state = 0;
    mux1(&f);
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 100) == false, "scroll arms on UNAVAIL, no fire");
    CHECK(s.scroll_press_armed, "scroll armed");

    // Rising edge UNAVAIL->AVAIL fires phase 1 (press).
    s.das_ap_state = 1;
    mux1(&f);
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 1000), "scroll phase1 fires");
    CHECK((f.buffer[1] & 0x30) == 0x10, "scroll phase1 press bits got 0x%02X", f.buffer[1] & 0x30);
    CHECK(s.scroll_press_state == 1, "scroll state==1");

    // After >=250ms, phase1 -> phase2.
    mux1(&f);
    fsd_handle_scroll_press_inject(&s, &f, 1260);
    CHECK(s.scroll_press_state == 2, "scroll -> state2 got %u", s.scroll_press_state);

    // Phase2 emits scroll-up.
    mux1(&f);
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 1260), "scroll phase2 fires");
    CHECK(
        (f.buffer[3] & 0x3F) == 0x01, "scroll phase2 scroll bits got 0x%02X", f.buffer[3] & 0x3F);

    // After >=150ms, phase2 -> phase3.
    mux1(&f);
    fsd_handle_scroll_press_inject(&s, &f, 1420);
    CHECK(s.scroll_press_state == 3, "scroll -> state3 got %u", s.scroll_press_state);

    // After >=250ms, phase3 -> phase4.
    mux1(&f);
    fsd_handle_scroll_press_inject(&s, &f, 1680);
    CHECK(s.scroll_press_state == 4, "scroll -> state4 got %u", s.scroll_press_state);

    // Phase4 emits the final scroll and enters cooldown(5).
    mux1(&f);
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 1700), "scroll phase4 fires");
    CHECK((f.buffer[3] & 0x3F) == 0x01, "scroll phase4 scroll bits");
    CHECK(s.scroll_press_state == 5, "scroll -> cooldown(5) got %u", s.scroll_press_state);

    // Cooldown: no re-fire while AP stays engaged.
    mux1(&f);
    CHECK(fsd_handle_scroll_press_inject(&s, &f, 2000) == false, "scroll cooldown no fire");

    // Re-arm only after AP returns to UNAVAIL.
    s.das_ap_state = 0;
    mux1(&f);
    fsd_handle_scroll_press_inject(&s, &f, 2100);
    CHECK(s.scroll_press_state == 0 && s.scroll_press_armed, "scroll re-armed after UNAVAIL");
}

// ── read-only Party-CAN parsers ───────────────────────────────────────────────
static void test_misc_parsers(void) {
    FSDState s;
    CANFRAME f;

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 5;
    f.buffer[1] = 0x20; // cruise_state = 2 (bits[6:4])
    f.buffer[4] = 0x03; // park_brake = 3
    f.buffer[3] = 0x06; // autopark = 3 (bits[4:1])
    fsd_handle_di_state(&s, &f);
    CHECK(s.di_cruise_state == 2, "di_state cruise got %u", s.di_cruise_state);
    CHECK(s.di_park_brake_state == 3, "di_state park got %u", s.di_park_brake_state);
    CHECK(s.di_autopark_state == 3, "di_state autopark got %u", s.di_autopark_state);

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 2;
    f.buffer[0] = 0x00;
    f.buffer[1] = 0x0C; // raw = 3072 -> 3072*0.25 - 750 = 18.0 Nm
    fsd_handle_di_torque(&s, &f);
    CHECK(fabs(s.di_torque_nm - 18.0f) < 0.1f, "di_torque got %.2f", (double)s.di_torque_nm);
    CHECK(s.di_torque_seen, "di_torque seen");

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 7;
    f.buffer[1] = 0x20; // buckle (bit5)
    f.buffer[2] = 0xC0; // left (bit6) + right (bit7)
    f.buffer[3] = 0x10; // door (bit4)
    f.buffer[6] = 0x04; // high beam (bit2)
    fsd_handle_ui_warning(&s, &f);
    CHECK(s.ui_buckle_status, "ui buckle");
    CHECK(s.ui_left_blinker && s.ui_right_blinker, "ui blinkers");
    CHECK(s.ui_any_door_open, "ui door");
    CHECK(s.ui_high_beam, "ui high beam");

    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 3;
    f.buffer[0] = 0xE8;
    f.buffer[1] = 0x43; // set_speed raw = 0x3E8 = 1000 -> 100.0 kph; acc_state = 4
    fsd_handle_das_control(&s, &f);
    CHECK(
        fabs(s.das_set_speed_kph - 100.0f) < 0.1f,
        "das_control speed got %.1f",
        (double)s.das_set_speed_kph);
    CHECK(s.das_acc_state == 4, "das_control acc_state got %u", s.das_acc_state);
}

// ── remaining read-only parsers ───────────────────────────────────────────────
static void test_readonly_parsers(void) {
    FSDState s;
    CANFRAME f;

    // VCRIGHT_status (0x343): rear defrost = byte1 bits[2:0]
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 2;
    f.buffer[1] = 0x02;
    fsd_handle_vcright_status(&s, &f);
    CHECK(s.rear_defrost_state == 2, "vcright defrost got %u", s.rear_defrost_state);

    // DI_systemStatus (0x118): track mode = byte6[1:0], traction = byte5[2:0]
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 7;
    f.buffer[6] = 0x02;
    f.buffer[5] = 0x05;
    fsd_handle_di_system_status(&s, &f);
    CHECK(s.track_mode_state == 2, "di_sys track got %u", s.track_mode_state);
    CHECK(s.traction_ctrl_mode == 5, "di_sys traction got %u", s.traction_ctrl_mode);

    // EPAS3S_currentTuneMode (0x370): mode = byte0[7:5]; torsion = ((byte2&0x0F)<<8|byte3)*0.01-20.5
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 4;
    f.buffer[0] = 0xA0; // bits[7:5] = 5
    f.buffer[2] = 0x01;
    f.buffer[3] = 0x90; // raw = 0x190 = 400 -> 4.0 - 20.5 = -16.5
    fsd_handle_epas_steering_mode(&s, &f);
    CHECK(s.steering_tune_mode == 5, "epas tune got %u", s.steering_tune_mode);
    CHECK(
        fabs(s.torsion_bar_torque_nm + 16.5f) < 0.05f,
        "epas torsion got %.2f",
        (double)s.torsion_bar_torque_nm);

    // DAS_status2 (0x389): acc_report = byte3[6:2], activation_fail = byte1[7:6]
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 5;
    f.buffer[3] = 0x14; // (0x14>>2)&0x1F = 5
    f.buffer[1] = 0xC0; // (0xC0>>6)&3 = 3
    fsd_handle_das_status2(&s, &f);
    CHECK(s.das_acc_report == 5, "das2 acc_report got %u", s.das_acc_report);
    CHECK(s.das_activation_fail == 3, "das2 activation_fail got %u", s.das_activation_fail);

    // DAS_settings (0x293): autosteer = byte4[6]
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 5;
    f.buffer[4] = 0x40;
    fsd_handle_das_settings(&s, &f);
    CHECK(s.das_autosteer_on, "das_settings autosteer on");

    // DAS_steeringControl (0x488): type = byte2[7:6]; angle = ((byte0&0x7F)<<8|byte1)*0.1-1638.35
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 3;
    f.buffer[2] = 0xC0; // type = 3
    f.buffer[0] = 0x40;
    f.buffer[1] = 0x00; // raw = 0x4000 = 16384 -> 1638.4 - 1638.35 = 0.05
    fsd_handle_das_steering(&s, &f);
    CHECK(s.das_steer_type == 3, "das_steer type got %u", s.das_steer_type);
    CHECK(
        fabs(s.das_steer_angle_req - 0.05f) < 0.1f,
        "das_steer angle got %.2f",
        (double)s.das_steer_angle_req);
}

// ── tyre pressures, 0x219 (fsd_tpms.h) ───────────────────────────────────────
// The 7-byte frames are copied from captures, not built from a table: the #211
// dump (2024 Fremont Model Y, Vehicle CAN, parked), in capture order with its
// timestamps, and the frames quoted in ukinora's fork from another car.
static void tpms_zero(FSDTpms* t) {
    memset(t, 0, sizeof(*t));
}

static void test_tpms_real_frames(void) {
    static const struct {
        uint32_t ms;
        uint8_t d[7];
    } k211[] = {
        {1963, {0x02, 0x80, 0xFF, 0xFF, 0xFF, 0xF8, 0x0F}},
        {2163, {0x04, 0x74, 0x74, 0x00, 0x00, 0x00, 0x00}},
        {2476, {0x01, 0x80, 0xFF, 0xFF, 0xFF, 0xFB, 0x0F}},
        {2663, {0x03, 0x80, 0xFF, 0xFF, 0xFF, 0xF9, 0x0F}},
        {3063, {0x01, 0x80, 0xFF, 0xFF, 0xFF, 0xFB, 0x0F}},
        {3563, {0x00, 0x80, 0xFF, 0xFF, 0xFF, 0xFA, 0x0F}},
        {3663, {0x01, 0x80, 0xFF, 0xFF, 0xFF, 0xFB, 0x0F}},
        {3763, {0x02, 0x80, 0xFF, 0xFF, 0xFF, 0xF8, 0x0F}},
        {4063, {0x05, 0x00, 0x6F, 0x6E, 0x72, 0x70, 0x00}},
    };
    FSDTpms t;
    tpms_zero(&t);
    CHECK(!fsd_tpms_seen(&t) && !fsd_tpms_has_reading(&t), "tpms: zeroed state has seen nothing");

    // The per-sensor frames carry 0x80 (3.2 bar) in byte 1 on every wheel: never a reading.
    for(int i = 0; i < 8; i++) {
        FSDTpmsRx rx = fsd_tpms_update(&t, k211[i].d, 7, k211[i].ms);
        FSDTpmsRx want = k211[i].d[0] == 4 ? FSD_TPMS_RX_TARGET : FSD_TPMS_RX_IGNORED;
        CHECK(
            rx == want,
            "tpms #211 frame %d (kind %u) got rx %d want %d",
            i,
            k211[i].d[0],
            rx,
            want);
        CHECK(!fsd_tpms_has_reading(&t), "tpms #211 frame %d: no reading yet", i);
    }
    CHECK(fsd_tpms_seen(&t) && t.frame_dlc == 7, "tpms: seen, dlc %u", t.frame_dlc);
    CHECK(
        t.target[0] == 0x74 && t.target[1] == 0x74,
        "tpms: target %02X %02X",
        t.target[0],
        t.target[1]);
    CHECK(
        fsd_tpms_mbar(t.target[0]) == 2900,
        "tpms: target 2900 mbar, got %u",
        fsd_tpms_mbar(t.target[0]));

    CHECK(
        fsd_tpms_update(&t, k211[8].d, 7, k211[8].ms) == FSD_TPMS_RX_READING,
        "tpms: kind 5 is a reading");
    CHECK(t.source == FSD_TPMS_SRC_ALL, "tpms: source ALL, got %u", t.source);
    static const unsigned mbar[4] = {2775, 2750, 2850, 2800};
    for(int i = 0; i < 4; i++) {
        CHECK(
            fsd_tpms_mbar(t.pressure[i]) == mbar[i],
            "tpms slot %d %u mbar want %u",
            i,
            fsd_tpms_mbar(t.pressure[i]),
            mbar[i]);
        CHECK(t.temp[i] == FSD_TPMS_NA, "tpms slot %d: kind 5 has no temperature", i);
        CHECK(
            t.pos[i] == (uint8_t)(FSD_TPMS_POS_FL + i),
            "tpms slot %d: kind 5 corner %u",
            i,
            t.pos[i]);
    }
    CHECK(
        t.last_ms == 4063 && fsd_tpms_age_ms(&t, 4663) == 600,
        "tpms: age 600 ms, got %u",
        (unsigned)fsd_tpms_age_ms(&t, 4663));
    CHECK(!fsd_tpms_stale(&t, 4663), "tpms: fresh after one cycle");

    // The next cycle's per-sensor frames leave the four values alone.
    for(int i = 0; i < 8; i++)
        fsd_tpms_update(&t, k211[i].d, 7, 4163 + k211[i].ms);
    CHECK(
        t.source == FSD_TPMS_SRC_ALL && t.pressure[0] == 0x6F && t.pressure[3] == 0x70,
        "tpms: per-sensor frames do not overwrite kind 5");
    for(int i = 0; i < 4; i++)
        CHECK(t.pressure[i] != 0x80, "tpms slot %d is not 0x80", i);

    // ukinora's car: the same 80 FF FF FF with another byte-5 permutation, the
    // same target, and kind 5 on three days. On the last one the driver's tyre
    // had been let down and byte 2 alone moved.
    static const uint8_t fork_sensor[4][7] = {
        {0x00, 0x80, 0xFF, 0xFF, 0xFF, 0xF8, 0x0F},
        {0x01, 0x80, 0xFF, 0xFF, 0xFF, 0xF9, 0x0F},
        {0x02, 0x80, 0xFF, 0xFF, 0xFF, 0xFB, 0x0F},
        {0x03, 0x80, 0xFF, 0xFF, 0xFF, 0xFA, 0x0F},
    };
    static const uint8_t fork_target[7] = {0x04, 0x74, 0x74, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t fork_0901[7] = {0x05, 0x00, 0x7A, 0x7A, 0x7A, 0x7A, 0x00};
    static const uint8_t fork_0903[7] = {0x05, 0x00, 0x75, 0x76, 0x74, 0x75, 0x00};
    static const uint8_t fork_0906[7] = {0x05, 0x00, 0x6D, 0x74, 0x73, 0x74, 0x00};
    tpms_zero(&t);
    for(int i = 0; i < 4; i++)
        CHECK(
            fsd_tpms_update(&t, fork_sensor[i], 7, 100) == FSD_TPMS_RX_IGNORED,
            "tpms fork kind %d ignored",
            i);
    CHECK(
        fsd_tpms_update(&t, fork_target, 7, 200) == FSD_TPMS_RX_TARGET && t.target[0] == 0x74 &&
            t.target[1] == 0x74,
        "tpms fork target pair");
    CHECK(
        fsd_tpms_update(&t, fork_0901, 7, 300) == FSD_TPMS_RX_READING && t.pressure[0] == 0x7A &&
            t.pressure[3] == 0x7A,
        "tpms fork 09-01 reads 0x7A x4");
    CHECK(
        fsd_tpms_update(&t, fork_0903, 7, 400) == FSD_TPMS_RX_READING && t.pressure[0] == 0x75 &&
            t.pressure[1] == 0x76 && t.pressure[2] == 0x74 && t.pressure[3] == 0x75,
        "tpms fork 09-03 in byte order");
    CHECK(
        fsd_tpms_update(&t, fork_0906, 7, 500) == FSD_TPMS_RX_READING && t.pressure[0] == 0x6D,
        "tpms fork 09-06 slot 0 = byte 2, got %02X",
        t.pressure[0]);
    CHECK(
        t.pressure[0] < t.pressure[1] && t.pressure[0] < t.pressure[2] &&
            t.pressure[0] < t.pressure[3],
        "tpms fork 09-06: the let-down tyre is the low slot");
    for(int i = 0; i < 4; i++)
        CHECK(
            fsd_tpms_update(&t, fork_sensor[i], 7, 600) == FSD_TPMS_RX_IGNORED &&
                t.pressure[i] != 0x80,
            "tpms fork kind %d leaves slot %d alone",
            i,
            i);

    // #237: the car's tyre screen read FL 2.2, FR 2.7, RL 1.6 (let down),
    // RR 2.3 bar while the dashboard showed slots 1..4 as 2.15, 2.68, 1.55,
    // 2.25 bar. At 0.025 bar per count each of those comes from one byte value
    // only, so this kind 5 is rebuilt from them, not copied from a capture.
    static const uint8_t photo_237[7] = {0x05, 0x00, 0x56, 0x6B, 0x3E, 0x5A, 0x00};
    static const unsigned photo_mbar[4] = {2150, 2675, 1550, 2250};
    static const uint8_t corner[4] = {
        FSD_TPMS_POS_FL, FSD_TPMS_POS_FR, FSD_TPMS_POS_RL, FSD_TPMS_POS_RR};
    tpms_zero(&t);
    CHECK(fsd_tpms_update(&t, photo_237, 7, 700) == FSD_TPMS_RX_READING, "tpms #237 photo frame");
    for(int i = 0; i < 4; i++)
        CHECK(
            fsd_tpms_mbar(t.pressure[i]) == photo_mbar[i] && t.pos[i] == corner[i],
            "tpms #237 byte %d: %u mbar at corner %u",
            2 + i,
            fsd_tpms_mbar(t.pressure[i]),
            t.pos[i]);
    CHECK(
        t.pos[2] == FSD_TPMS_POS_RL && t.pressure[2] < t.pressure[0] &&
            t.pressure[2] < t.pressure[1] && t.pressure[2] < t.pressure[3],
        "tpms #237: the let-down tyre is RL");
    // An empty kind 5 drops the values but keeps the corners.
    static const uint8_t empty_all[7] = {0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CHECK(
        fsd_tpms_update(&t, empty_all, 7, 800) == FSD_TPMS_RX_EMPTY && !fsd_tpms_has_reading(&t) &&
            t.source == FSD_TPMS_SRC_ALL,
        "tpms: empty kind 5 clears the values");
    for(int i = 0; i < 4; i++)
        CHECK(t.pos[i] == corner[i], "tpms: empty kind 5 keeps slot %d at corner %u", i, t.pos[i]);
}

// The 5-byte form exactly as the public DBC lays it out (no capture of it yet).
static void
    tpms_dbc(uint8_t* d, uint8_t sensor, uint8_t p, uint8_t temp, uint8_t volt, uint8_t loc) {
    d[0] = sensor;
    d[1] = p;
    d[2] = temp;
    d[3] = volt;
    d[4] = loc;
}

static void test_tpms_dbc_form(void) {
    FSDTpms t;
    uint8_t d[5];
    tpms_zero(&t);

    // Asleep before anything was read: nothing to show, source stays empty.
    tpms_dbc(d, 0, 0x80, 0xFF, 0xFF, 0x02);
    CHECK(fsd_tpms_update(&t, d, 5, 10) == FSD_TPMS_RX_EMPTY, "tpms dbc: placeholder is empty");
    CHECK(
        !fsd_tpms_has_reading(&t) && t.source == FSD_TPMS_SRC_NONE,
        "tpms dbc: placeholder starts no source");

    // Four sensors, the car placing them RL RR FL FR. 0x3C = 20 C, 0x8C = 2.9 V.
    static const uint8_t p[4] = {0x70, 0x71, 0x72, 0x73};
    static const uint8_t loc[4] = {2, 3, 0, 1};
    for(uint8_t k = 0; k < 4; k++) {
        tpms_dbc(d, k, p[k], (uint8_t)(0x3C + k), 0x8C, loc[k]);
        CHECK(fsd_tpms_update(&t, d, 5, 100 + k) == FSD_TPMS_RX_READING, "tpms dbc sensor %u", k);
    }
    CHECK(t.source == FSD_TPMS_SRC_SENSOR && t.frame_dlc == 5, "tpms dbc: source SENSOR");
    for(int k = 0; k < 4; k++) {
        CHECK(t.pressure[k] == p[k], "tpms dbc slot %d pressure %02X", k, t.pressure[k]);
        CHECK(
            fsd_tpms_temp_c(t.temp[k]) == 20 + k,
            "tpms dbc slot %d temp %d",
            k,
            fsd_tpms_temp_c(t.temp[k]));
        CHECK(t.pos[k] == loc[k], "tpms dbc slot %d pos %u", k, t.pos[k]);
    }
    CHECK(t.last_ms == 103, "tpms dbc: last reading at 103, got %u", (unsigned)t.last_ms);

    // The car loses sensor 1: its slot goes back to unknown, the rest stay.
    tpms_dbc(d, 1, 0x80, 0xFF, 0xFF, 0x03);
    CHECK(fsd_tpms_update(&t, d, 5, 200) == FSD_TPMS_RX_EMPTY, "tpms dbc: sensor 1 placeholder");
    CHECK(t.pressure[1] == FSD_TPMS_NA && t.temp[1] == FSD_TPMS_NA, "tpms dbc: slot 1 cleared");
    CHECK(
        t.pressure[0] == 0x70 && t.pressure[2] == 0x72 && t.pressure[3] == 0x73,
        "tpms dbc: other slots kept");
    CHECK(t.last_ms == 103, "tpms dbc: an empty frame is not a reading");
    tpms_dbc(d, 2, 0xFF, 0x3C, 0x8C, 0x00);
    CHECK(
        fsd_tpms_update(&t, d, 5, 210) == FSD_TPMS_RX_EMPTY && t.pressure[2] == FSD_TPMS_NA,
        "tpms dbc: pressure FF is no reading");

    // Temperature missing but battery there: still a reading.
    tpms_dbc(d, 3, 0x74, 0xFF, 0x8C, 0x01);
    CHECK(
        fsd_tpms_update(&t, d, 5, 220) == FSD_TPMS_RX_READING && t.pressure[3] == 0x74 &&
            t.temp[3] == FSD_TPMS_NA,
        "tpms dbc: no temperature, pressure kept");

    // 0 bar from a live sensor is a flat tyre, not "unknown".
    tpms_dbc(d, 0, 0x00, 0x3C, 0x8C, 0x02);
    CHECK(
        fsd_tpms_update(&t, d, 5, 230) == FSD_TPMS_RX_READING && t.pressure[0] == 0x00,
        "tpms dbc: 0 bar kept as 0");

    // Location 4 is unknown in the DBC; 5..7 are not in it at all.
    tpms_dbc(d, 0, 0x70, 0x3C, 0x8C, 0x04);
    fsd_tpms_update(&t, d, 5, 240);
    CHECK(t.pos[0] == FSD_TPMS_POS_UNKNOWN, "tpms dbc: location 4 unknown, got %u", t.pos[0]);
    tpms_dbc(d, 0, 0x70, 0x3C, 0x8C, 0x07);
    fsd_tpms_update(&t, d, 5, 250);
    CHECK(t.pos[0] == FSD_TPMS_POS_UNKNOWN, "tpms dbc: location 7 unknown, got %u", t.pos[0]);
    tpms_dbc(d, 0, 0x70, 0x3C, 0x8C, 0xF9); // upper bits are not part of the location
    fsd_tpms_update(&t, d, 5, 260);
    CHECK(t.pos[0] == FSD_TPMS_POS_FR, "tpms dbc: location from bits 0-2, got %u", t.pos[0]);
}

static void test_tpms_malformed(void) {
    FSDTpms t;
    uint8_t d[8] = {0x05, 0x00, 0x6F, 0x6E, 0x72, 0x70, 0x00, 0x00};
    tpms_zero(&t);

    CHECK(
        fsd_tpms_update(&t, d, 0, 10) == FSD_TPMS_RX_IGNORED && !fsd_tpms_seen(&t),
        "tpms: dlc 0 is not a frame");
    CHECK(fsd_tpms_update(NULL, d, 7, 10) == FSD_TPMS_RX_IGNORED, "tpms: null state");
    CHECK(
        fsd_tpms_update(&t, NULL, 7, 10) == FSD_TPMS_RX_IGNORED && !fsd_tpms_seen(&t),
        "tpms: null data");

    // Kind 5 bytes at every length but 7: nothing taken.
    for(uint8_t dlc = 1; dlc <= 8; dlc++) {
        if(dlc == 7) continue;
        CHECK(fsd_tpms_update(&t, d, dlc, 20) == FSD_TPMS_RX_IGNORED, "tpms: dlc %u ignored", dlc);
        CHECK(t.frame_dlc == dlc, "tpms: dlc %u recorded", dlc);
    }
    CHECK(!fsd_tpms_has_reading(&t), "tpms: no reading from odd lengths");
    // The first frame turned every value into "not available", not 0.
    CHECK(
        t.target[0] == FSD_TPMS_NA && t.target[1] == FSD_TPMS_NA && t.pressure[0] == FSD_TPMS_NA,
        "tpms: unknown is FF, not 0");

    // Kinds a form does not have.
    static const uint8_t kinds[] = {0x06, 0x07, 0x45, 0x84, 0x85, 0xFF};
    for(size_t i = 0; i < sizeof(kinds); i++) {
        d[0] = kinds[i];
        CHECK(
            fsd_tpms_update(&t, d, 7, 30) == FSD_TPMS_RX_IGNORED,
            "tpms: 7-byte kind %02X ignored",
            kinds[i]);
        CHECK(
            fsd_tpms_update(&t, d, 5, 30) == FSD_TPMS_RX_IGNORED,
            "tpms: 5-byte kind %02X ignored",
            kinds[i]);
    }
    d[0] = 0x04;
    CHECK(
        fsd_tpms_update(&t, d, 5, 40) == FSD_TPMS_RX_IGNORED,
        "tpms: no kind 4 in the 5-byte form");
    d[0] = 0x05;
    CHECK(
        fsd_tpms_update(&t, d, 5, 40) == FSD_TPMS_RX_IGNORED,
        "tpms: no kind 5 in the 5-byte form");
    // A per-sensor kind in the 7-byte form, however live it looks.
    static const uint8_t live_looking[7] = {0x00, 0x72, 0x3C, 0x8C, 0x00, 0xFA, 0x0F};
    CHECK(
        fsd_tpms_update(&t, live_looking, 7, 50) == FSD_TPMS_RX_IGNORED,
        "tpms: 7-byte kind 0 never decoded");
    CHECK(!fsd_tpms_has_reading(&t) && t.target[0] == FSD_TPMS_NA, "tpms: still nothing");

    // Kind 5 slots that say nothing, and kind 4 with nothing in it.
    static const uint8_t gaps[7] = {0x05, 0x00, 0x6F, 0x00, 0xFF, 0x70, 0x00};
    CHECK(fsd_tpms_update(&t, gaps, 7, 60) == FSD_TPMS_RX_READING, "tpms: partial kind 5");
    CHECK(
        t.pressure[0] == 0x6F && t.pressure[1] == FSD_TPMS_NA && t.pressure[2] == FSD_TPMS_NA &&
            t.pressure[3] == 0x70,
        "tpms: 0 and FF slots unknown");
    static const uint8_t none5[7] = {0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CHECK(
        fsd_tpms_update(&t, none5, 7, 70) == FSD_TPMS_RX_EMPTY && !fsd_tpms_has_reading(&t),
        "tpms: empty kind 5 clears kind 5 values");
    static const uint8_t none4[7] = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CHECK(
        fsd_tpms_update(&t, none4, 7, 80) == FSD_TPMS_RX_EMPTY && t.target[0] == FSD_TPMS_NA,
        "tpms: empty kind 4");
}

static void test_tpms_sources_and_age(void) {
    FSDTpms t;
    uint8_t d[5];
    static const uint8_t all[7] = {0x05, 0x00, 0x6F, 0x6E, 0x72, 0x70, 0x00};
    tpms_zero(&t);

    CHECK(fsd_tpms_update(&t, all, 7, 1000) == FSD_TPMS_RX_READING, "tpms src: kind 5 first");
    // A live sensor reading replaces all four: kind 5 slots are in another order.
    tpms_dbc(d, 2, 0x71, 0x3C, 0x8C, 0x00);
    CHECK(
        fsd_tpms_update(&t, d, 5, 1100) == FSD_TPMS_RX_READING && t.source == FSD_TPMS_SRC_SENSOR,
        "tpms src: sensor reading wins");
    CHECK(
        t.pressure[0] == FSD_TPMS_NA && t.pressure[1] == FSD_TPMS_NA && t.pressure[2] == 0x71 &&
            t.pressure[3] == FSD_TPMS_NA,
        "tpms src: only the sensor's own slot is set");
    CHECK(
        fsd_tpms_update(&t, all, 7, 1200) == FSD_TPMS_RX_IGNORED && t.pressure[2] == 0x71,
        "tpms src: kind 5 waits while sensor readings are fresh");
    CHECK(
        fsd_tpms_update(&t, all, 7, 1100 + FSD_TPMS_STALE_MS) == FSD_TPMS_RX_IGNORED,
        "tpms src: still fresh at the limit");
    CHECK(
        fsd_tpms_update(&t, all, 7, 1101 + FSD_TPMS_STALE_MS) == FSD_TPMS_RX_READING &&
            t.source == FSD_TPMS_SRC_ALL && t.pressure[2] == 0x72,
        "tpms src: kind 5 takes over from stale sensor readings");

    // Sensor source with every slot cleared by the car: kind 5 at once.
    tpms_dbc(d, 0, 0x71, 0x3C, 0x8C, 0x00);
    fsd_tpms_update(&t, d, 5, 50000);
    tpms_dbc(d, 0, 0x80, 0xFF, 0xFF, 0x00);
    fsd_tpms_update(&t, d, 5, 50100);
    CHECK(
        t.source == FSD_TPMS_SRC_SENSOR && !fsd_tpms_has_reading(&t),
        "tpms src: sensor source with nothing left");
    CHECK(
        fsd_tpms_update(&t, all, 7, 50200) == FSD_TPMS_RX_READING && t.source == FSD_TPMS_SRC_ALL,
        "tpms src: kind 5 fills an empty sensor source");

    // Age and staleness.
    CHECK(fsd_tpms_age_ms(&t, 50200) == 0 && !fsd_tpms_stale(&t, 50200), "tpms age: 0 right away");
    CHECK(!fsd_tpms_stale(&t, 50200 + FSD_TPMS_STALE_MS), "tpms age: not stale at the limit");
    CHECK(fsd_tpms_stale(&t, 50201 + FSD_TPMS_STALE_MS), "tpms age: stale past the limit");
    CHECK(fsd_tpms_age_ms(&t, 50200 + 312000) == 312000, "tpms age: 312 s");
    t.last_ms = 0xFFFFF000u;
    CHECK(
        fsd_tpms_age_ms(&t, 0x100u) == 0x1100u && !fsd_tpms_stale(&t, 0x100u),
        "tpms age: across the millis wrap");
    FSDTpms none;
    tpms_zero(&none);
    CHECK(
        fsd_tpms_age_ms(&none, 123456) == 0 && !fsd_tpms_stale(&none, 123456),
        "tpms age: nothing read is never stale");
}

// ── extras write handlers (Service-gated) + frame builders ────────────────────
static void test_extras_and_builders(void) {
    FSDState s;
    CANFRAME f;

    // Hazard inject: byte0[7:4] = 1, Service-gated.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Service;
    s.extra_hazard_lights = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0x0F;
    CHECK(fsd_handle_hazard_inject(&s, &f), "hazard modifies");
    CHECK(f.buffer[0] == 0x1F, "hazard byte0 got 0x%02X exp 0x1F", f.buffer[0]);
    s.op_mode = OpMode_Active; // gate
    CHECK(fsd_handle_hazard_inject(&s, &f) == false, "hazard gated outside Service");

    // Wiper off: byte0[7:4] = 0, Service-gated.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Service;
    s.extra_wiper_off = true;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0xF5;
    CHECK(fsd_handle_wiper_off(&s, &f), "wiper modifies");
    CHECK(f.buffer[0] == 0x05, "wiper byte0 got 0x%02X exp 0x05", f.buffer[0]);

    // Park frame builder (0x229).
    zero(&f);
    fsd_build_park_frame(&f);
    CHECK(f.canId == CAN_ID_SCCM_RSTALK, "park id 0x229");
    CHECK(f.data_lenght == 3, "park dlc 3");
    CHECK(f.buffer[2] == 0x01, "park button pressed byte2");

    // Precondition frame builder (0x082).
    zero(&f);
    fsd_build_precondition_frame(&f);
    CHECK(f.canId == CAN_ID_TRIP_PLANNING, "precond id 0x082");
    CHECK(f.data_lenght == 8, "precond dlc 8");
    CHECK(f.buffer[0] == 0x05, "precond byte0 got 0x%02X exp 0x05", f.buffer[0]);
}

// ── .cantest profile parser + send interlock ─────────────────────────────────
static void test_profile(void) {
    FsdProfileStep s;
    char name[40];

    // bare candump-style line (non-denied id)
    CHECK(
        fsd_profile_parse_line("118#00112233445566AA", &s, NULL, 0) == FSD_PLINE_STEP,
        "profile bare line -> step");
    CHECK(s.can_id == 0x118, "profile id got 0x%lX", (unsigned long)s.can_id);
    CHECK(s.dlc == 8, "profile dlc 8 got %u", s.dlc);
    CHECK(s.data[0] == 0x00 && s.data[7] == 0xAA, "profile data bytes");
    CHECK(s.repeat == 1 && s.delay_ms == 50, "profile defaults repeat=1 delay=50");

    // repeat=/delay= suffixes
    CHECK(
        fsd_profile_parse_line("3FD#1000000000004000 repeat=20 delay=100", &s, NULL, 0) ==
            FSD_PLINE_STEP,
        "profile with suffixes");
    CHECK(
        s.can_id == 0x3FD && s.repeat == 20 && s.delay_ms == 100,
        "profile r=%u d=%u",
        s.repeat,
        s.delay_ms);

    // "delay=Nms" form + short dlc
    CHECK(
        fsd_profile_parse_line("370#0011 delay=250ms", &s, NULL, 0) == FSD_PLINE_STEP,
        "profile delay=Nms");
    CHECK(s.delay_ms == 250 && s.dlc == 2, "profile d=%u dlc=%u", s.delay_ms, s.dlc);

    // a raw capture-log line (copy-from-capture loop) must parse
    CHECK(
        fsd_profile_parse_line("(1.234000) can0 370#0000000000000000", &s, NULL, 0) ==
            FSD_PLINE_STEP,
        "profile accepts candump line");
    CHECK(s.can_id == 0x370 && s.dlc == 8, "profile candump id/dlc");

    // name header, comment, blank
    CHECK(
        fsd_profile_parse_line("# Name: poke 229", &s, name, sizeof(name)) == FSD_PLINE_NAME,
        "profile name header");
    CHECK(strcmp(name, "poke 229") == 0, "profile name [%s]", name);
    CHECK(
        fsd_profile_parse_line("# just a note", &s, NULL, 0) == FSD_PLINE_EMPTY,
        "profile comment");
    CHECK(fsd_profile_parse_line("   ", &s, NULL, 0) == FSD_PLINE_EMPTY, "profile blank");

    // malformed
    CHECK(fsd_profile_parse_line("no hash here", &s, NULL, 0) == FSD_PLINE_ERROR, "profile no-#");
    CHECK(fsd_profile_parse_line("229#0", &s, NULL, 0) == FSD_PLINE_ERROR, "profile odd hex");
    CHECK(
        fsd_profile_parse_line("229#001122334455667788", &s, NULL, 0) == FSD_PLINE_ERROR,
        "profile >8 bytes");

    // safety denylist: 0x229 right stalk (gear / AP engage) must be blocked, never sent
    CHECK(fsd_profile_id_blocked(0x229), "0x229 is on the denylist");
    CHECK(!fsd_profile_id_blocked(0x3FD), "0x3FD is not denied");
    CHECK(!fsd_profile_id_blocked(0x118), "0x118 is not denied");
    CHECK(
        fsd_profile_parse_line("229#460000", &s, NULL, 0) == FSD_PLINE_BLOCKED,
        "profile 0x229 idle -> blocked");
    CHECK(s.can_id == 0x229, "blocked step still names the id");
    CHECK(
        fsd_profile_parse_line("(0.085000) can0 229#B74000", &s, NULL, 0) == FSD_PLINE_BLOCKED,
        "profile 0x229 candump pull-down -> blocked");

    // ── send interlock (fail-closed) ──
    FSDState st;
    memset(&st, 0, sizeof(st));
    st.op_mode = OpMode_ListenOnly;
    CHECK(fsd_profile_tx_allowed(&st, 1000) == false, "tx blocked in Listen-Only");
    st.op_mode = OpMode_Active;
    CHECK(fsd_profile_tx_allowed(&st, 1000) == false, "tx blocked: no speed seen (fail-closed)");
    st.speed_seen = true;
    st.last_speed_tick_ms = 1000;
    st.vehicle_speed_kph = 0.0f;
    CHECK(fsd_profile_tx_allowed(&st, 1500) == true, "tx allowed: active + fresh + stationary");
    CHECK(fsd_profile_tx_allowed(&st, 3000) == false, "tx blocked: stale speed frame");
    st.vehicle_speed_kph = 5.0f;
    CHECK(fsd_profile_tx_allowed(&st, 1200) == false, "tx blocked: car moving");
    st.vehicle_speed_kph = 0.0f;
    st.op_mode = OpMode_Service;
    st.last_speed_tick_ms = 1200;
    CHECK(fsd_profile_tx_allowed(&st, 1300) == true, "tx allowed in Service when stationary");
}

// ── state init ────────────────────────────────────────────────────────────────
static void test_state_init(void) {
    FSDState s;
    fsd_state_init(&s, TeslaHW_HW4);
    CHECK(s.hw_version == TeslaHW_HW4, "init applies HW4");
    CHECK(!s.hw3_speed_override, "init: hw3_speed_override default OFF (#209)");
}

// ── black-box capture ID filter (#124) ───────────────────────────────────────
static void test_blackbox_filter(void) {
    // Every key id in the curated set is recorded.
    for(size_t i = 0; i < FSD_BLACKBOX_KEY_ID_COUNT; i++)
        CHECK(
            fsd_blackbox_should_record(FSD_BLACKBOX_KEY_IDS[i]),
            "key id 0x%lX recorded",
            (unsigned long)FSD_BLACKBOX_KEY_IDS[i]);

    // The abort / steer-jerk analysis ids must be present (the whole point).
    CHECK(fsd_blackbox_should_record(CAN_ID_EPAS_STATUS), "0x370 EPAS in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_DAS_STATUS_HW3), "0x399 DAS_status HW3 in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_DAS_STATUS), "0x39B DAS_status HW4 in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_DAS_STEER), "0x488 DAS_steeringControl in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_STEER_ANGLE), "0x129 steering angle in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_AP_LEGACY), "0x3EE AP legacy in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_AP_CONTROL), "0x3FD AP control in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_ESP_STATUS), "0x145 brake in set");
    CHECK(fsd_blackbox_should_record(0x238u), "0x238 map limit in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_DAS_STATUS2), "0x389 ACC limit in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_DAS_CONTROL), "0x2B9 DAS_control in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_DI_SYS_STATUS), "0x118 state in set");
    // Speed / Acceleration Mode / battery questions (#176, #211, #251).
    CHECK(fsd_blackbox_should_record(CAN_ID_DI_SPEED), "0x257 speed in set");
    CHECK(fsd_blackbox_should_record(0x334u), "0x334 pedal map in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_BMS_THERMAL), "0x312 pack temp in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_BMS_SOC), "0x292 pack SOC in set");
    CHECK(fsd_blackbox_should_record(CAN_ID_UI_RANGE), "0x33A car SOC in set");

    // 0x257 thinned to one frame per FSD_BLACKBOX_THIN_MS per bus; other ids untouched.
    FSDBlackboxThin th;
    memset(&th, 0, sizeof(th));
    int kept = 0;
    for(uint32_t t = 1000u; t < 2000u; t += 20u) // 50 Hz for 1 s
        if(fsd_blackbox_thin_keep(CAN_ID_DI_SPEED, t, &th)) kept++;
    CHECK(kept == 10, "0x257 at 50 Hz thinned to 10 per second (kept %d)", kept);
    CHECK(
        fsd_blackbox_thin_keep(CAN_ID_DI_SPEED, 2080u, &th),
        "0x257 100 ms after the last kept: stored");
    CHECK(!fsd_blackbox_thin_keep(CAN_ID_DI_SPEED, 2179u, &th), "0x257 99 ms after: dropped");
    int other = 0;
    for(uint32_t t = 1000u; t < 2000u; t += 10u)
        if(fsd_blackbox_thin_keep(CAN_ID_STEER_ANGLE, t, &th)) other++;
    CHECK(other == 100, "0x129 not thinned (kept %d/100)", other);
    memset(&th, 0, sizeof(th));
    CHECK(fsd_blackbox_thin_keep(CAN_ID_DI_SPEED, 0u, &th), "first 0x257 at t=0 stored");

    // Chatty non-diagnostic frames are dropped (the ~15x rate cut).
    CHECK(!fsd_blackbox_should_record(CAN_ID_BMS_HV_BUS), "0x132 BMS dropped");
    CHECK(!fsd_blackbox_should_record(CAN_ID_ESP_WHEELSPD), "0x175 wheel speeds dropped");
    CHECK(!fsd_blackbox_should_record(0x000), "0x000 dropped");
    CHECK(!fsd_blackbox_should_record(0x7FF), "0x7FF dropped");
}

// ─────────────────────────────────────────────────────────────────────────────
// Power / reset log
//
// The question this answers is whether the rear tap's 12V is switched or
// always-on, and it exists because a multimeter cannot answer it comfortably:
// reaching the connector means opening a door, and opening a door wakes the
// car. It also separates three causes a voltage reading collapses into one --
// the supply going away, the supply sagging, and us crashing.
// ─────────────────────────────────────────────────────────────────────────────
static void test_power_verdict(void) {
    printf("\n-- power: switched or always-on --\n");

    FsdPwrRecord slept = {true, 2400000u, 1200000u}; // up 40 min, quiet 20 min
    FsdPwrRecord awake = {true, 2400000u, 0u}; // up 40 min, bus never quiet

    // The whole point: quiet bus, then the supply died.
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_POWERON, &slept) == FSD_PWR_SWITCHED,
        "quiet then power-cycled -> switched");

    // 🔴 The trap. "Came back from a power cut" looks like switched, but if the
    // bus never went quiet the car never slept -- sentry left on, a scheduled
    // charge, someone opening the app. Reporting SWITCHED here would be a
    // confident answer to a question that was never asked.
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_POWERON, &awake) == FSD_PWR_NO_SLEEP,
        "power cut but the bus never went quiet -> inconclusive");

    // A sagging supply is a wiring verdict, not a switching one. Thin tap wire,
    // a cold crimp, or an e-fuse near its limit all land here, and all of them
    // want a different response than "the car switched us off".
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_BROWNOUT, &slept) == FSD_PWR_BROWNOUT,
        "brownout is reported as brownout, not as switched");

    // We fell over. The supply is not implicated and must not be blamed.
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_SOFTWARE, &slept) == FSD_PWR_CRASH,
        "software reset says nothing about the supply");
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_EXTERNAL, &slept) == FSD_PWR_CRASH,
        "reset pin says nothing about the supply");

    // Nothing recorded yet -- first boot, or NVS cleared.
    FsdPwrRecord none = {false, 0u, 0u};
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_POWERON, &none) == FSD_PWR_UNKNOWN, "no record -> unknown");
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_POWERON, NULL) == FSD_PWR_UNKNOWN, "NULL record -> unknown");
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_UNKNOWN, &slept) == FSD_PWR_UNKNOWN,
        "unknown reset reason -> unknown");

    // The USB host reset us (S3 USB serial/JTAG): USB is connected and feeds
    // the board, with or without a record.
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_USB, &slept) == FSD_PWR_USB,
        "USB reset after a quiet record -> USB, not switched");
    CHECK(fsd_pwr_verdict(FSD_PWR_RESET_USB, &none) == FSD_PWR_USB, "USB reset, no record -> USB");
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_USB, NULL) == FSD_PWR_USB, "USB reset, NULL record -> USB");
    CHECK(fsd_pwr_restart_count(FSD_PWR_RESET_USB, true, 2u) == 3u, "a USB reset is a restart");

    // ALWAYS_ON can never come from the boot path: a module that kept its power
    // never rebooted to report anything. It has to come from the live path.
    CHECK(
        fsd_pwr_live_verdict(FSD_PWR_ALWAYS_ON_MS) == FSD_PWR_ALWAYS_ON,
        "still alive long after the bus went quiet -> always-on");
    CHECK(
        fsd_pwr_live_verdict(FSD_PWR_ALWAYS_ON_MS - 1u) == FSD_PWR_UNKNOWN,
        "not quiet long enough yet -> no claim");
    CHECK(fsd_pwr_live_verdict(0) == FSD_PWR_UNKNOWN, "bus busy -> no claim");

    // Every value has to be printable, or the serial line reports a number.
    for(int r = 0; r <= FSD_PWR_RESET_USB; r++)
        CHECK(fsd_pwr_reset_str((FsdPwrReset)r)[0] != '\0', "reset %d has a name", r);
    for(int v = 0; v <= FSD_PWR_CRASH; v++)
        CHECK(fsd_pwr_verdict_str((FsdPwrVerdict)v)[0] != '\0', "verdict %d has a name", v);
}

static void test_power_quiet_ms(void) {
    // Never received anything: we have not seen the bus go quiet, we have seen
    // nothing at all. Returning a huge quiet time here would make a module that
    // was never wired to the car look like a car that fell asleep.
    CHECK(fsd_pwr_quiet_ms(60000u, 0u, false) == 0u, "no frame ever -> not quiet");

    // Traffic within the threshold is not quiet.
    CHECK(fsd_pwr_quiet_ms(10000u, 9999u, true) == 0u, "recent frame -> not quiet");
    CHECK(
        fsd_pwr_quiet_ms(10000u + FSD_PWR_QUIET_MS - 1u, 10000u, true) == 0u,
        "just under the threshold -> not quiet");

    // Past the threshold the answer is the full gap, not the excess. The
    // operator reads "quiet for N minutes", which is the gap.
    uint32_t gap = FSD_PWR_QUIET_MS + 5000u;
    CHECK(
        fsd_pwr_quiet_ms(10000u + gap, 10000u, true) == gap,
        "quiet returns the whole gap, got %u",
        fsd_pwr_quiet_ms(10000u + gap, 10000u, true));

    // The ms clock wraps after ~49 days. Unsigned subtraction handles it; a
    // signed comparison would report a wildly quiet bus for a moment and the
    // module would announce that the car had slept when it had not.
    //
    // 0xFFFFFF00 -> 50 is a gap of 306 ms across the wrap, which is short.
    CHECK(
        fsd_pwr_quiet_ms(50u, 0xFFFFFF00u, true) == 0u,
        "a short gap across the wrap is not quiet, got %u",
        fsd_pwr_quiet_ms(50u, 0xFFFFFF00u, true));

    // ... and a long one across the wrap is reported as the whole gap.
    CHECK(
        fsd_pwr_quiet_ms(20000u, 0xFFFFFF00u, true) == 20256u,
        "long gap across the wrap, got %u",
        fsd_pwr_quiet_ms(20000u, 0xFFFFFF00u, true));

    // The loop samples its clock once, then drains the bus, and every frame
    // stamps millis() as it lands. A frame that landed after the loop's clock
    // is a stamp ahead of now: traffic, not 49.7 days of quiet. That read as
    // "bus quiet 4294967s" and committed a checkpoint while frames arrived.
    CHECK(
        fsd_pwr_quiet_ms(60000u, 60001u, true) == 0u,
        "frame stamped 1 ms after now -> not quiet, got %u",
        fsd_pwr_quiet_ms(60000u, 60001u, true));
    CHECK(
        fsd_pwr_quiet_ms(60000u, 60000u + 250u, true) == 0u,
        "frame stamped during a long loop pass -> not quiet");
    CHECK(
        fsd_pwr_quiet_ms(0xFFFFFFFEu, 3u, true) == 0u,
        "frame stamped just after the wrap, now just before it -> not quiet");
    {
        FsdPwrCheckpoint cp = {false, 0, false};
        CHECK(
            !fsd_pwr_checkpoint_due(&cp, fsd_pwr_quiet_ms(60000u, 60004u, true)),
            "a stamp ahead of now never commits a checkpoint");
        CHECK(
            fsd_pwr_live_verdict(fsd_pwr_quiet_ms(60000u, 60004u, true)) == FSD_PWR_UNKNOWN,
            "a stamp ahead of now is not an always-on bus");
    }
    // Quiet times a verdict waits for are far inside the range kept: a day, a
    // week, three weeks.
    for(uint32_t days = 1u; days <= 21u; days *= 3u) {
        uint32_t q = days * 86400000u;
        CHECK(
            fsd_pwr_quiet_ms(1000u + q, 1000u, true) == q,
            "quiet for %u days is reported, got %u",
            days,
            fsd_pwr_quiet_ms(1000u + q, 1000u, true));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Power / reset log — checkpoint schedule, deep-sleep and USB (#218 rework)
//
// The persistence has one hard rule: no flash write while the bus is active, so
// a commit never drops a frame to the flash stall. The scheduler below is what
// enforces it, as pure logic fed the current quiet duration, so the cadence is
// decided here rather than on a board.
// ─────────────────────────────────────────────────────────────────────────────
static void test_power_checkpoint(void) {
    printf("\n-- power: flash-checkpoint schedule --\n");
    FsdPwrCheckpoint cp = {false, 0, false};

    // Bus active: never written here, and it re-arms the next episode.
    CHECK(!fsd_pwr_checkpoint_due(&cp, 0u), "bus active -> no commit");

    // The trap: power lost before the first checkpoint. Quiet climbs toward the
    // window but never reaches it, so nothing is ever committed, and the next
    // boot reads a cleared record (NO_SLEEP), not a false SWITCHED.
    CHECK(!fsd_pwr_checkpoint_due(&cp, FSD_PWR_QUIET_MS), "just quiet -> no commit yet");
    CHECK(
        !fsd_pwr_checkpoint_due(&cp, FSD_PWR_CHECKPOINT_MS - 1u), "under the window -> no commit");
    CHECK(cp.writes == 0, "nothing written before the window");

    // Settled quiet: exactly one commit for the episode, however many ticks.
    CHECK(fsd_pwr_checkpoint_due(&cp, FSD_PWR_CHECKPOINT_MS), "window reached -> commit");
    CHECK(cp.writes == 1, "one write so far");
    CHECK(
        !fsd_pwr_checkpoint_due(&cp, FSD_PWR_CHECKPOINT_MS + 60000u),
        "still quiet -> no second commit");
    CHECK(
        !fsd_pwr_checkpoint_due(&cp, FSD_PWR_CHECKPOINT_MS + 600000u),
        "an hour later -> still one");
    CHECK(cp.writes == 1, "a long sleep is still a single write");

    // Bus wakes, then goes quiet again: a new episode, a new commit.
    CHECK(!fsd_pwr_checkpoint_due(&cp, 0u), "bus busy again -> re-arm");
    CHECK(fsd_pwr_checkpoint_due(&cp, FSD_PWR_CHECKPOINT_MS), "second quiet episode -> commit");
    CHECK(cp.writes == 2, "two episodes -> two writes");

    // The per-session cap bounds writes no matter how the bus flaps.
    FsdPwrCheckpoint cap = {false, 0, false};
    unsigned commits = 0;
    for(int episode = 0; episode < (int)FSD_PWR_MAX_CHECKPOINTS + 5; episode++) {
        fsd_pwr_checkpoint_due(&cap, 0u); // bus busy (re-arm)
        if(fsd_pwr_checkpoint_due(&cap, FSD_PWR_CHECKPOINT_MS)) commits++; // then quiet
    }
    CHECK(
        commits == FSD_PWR_MAX_CHECKPOINTS,
        "writes capped at %u per session, got %u",
        FSD_PWR_MAX_CHECKPOINTS,
        commits);

    CHECK(!fsd_pwr_checkpoint_due(NULL, FSD_PWR_CHECKPOINT_MS), "NULL checkpoint -> no commit");
}

/* One loop tick as power_log_tick() runs it: the correction check, then the
 * checkpoint. Returns 1 for a correcting write, 2 for a checkpoint, 0 for none. */
static int pwr_tick(FsdPwrCheckpoint* cp, uint32_t quiet) {
    int w = 0;
    if(fsd_pwr_correction_due(cp, quiet)) w = 1;
    if(fsd_pwr_checkpoint_due(cp, quiet)) w = 2;
    return w;
}

static void test_power_correction(void) {
    printf("\n-- power: correcting a quiet record when the bus comes back --\n");
    FsdPwrCheckpoint cp = {false, 0, false};

    // An hour of driving from boot: nothing on flash to correct, no writes.
    unsigned writes = 0;
    for(uint32_t t = 0; t < 3600000u; t += 10u)
        writes += pwr_tick(&cp, 0u) != 0;
    CHECK(writes == 0u, "a busy session from boot -> no writes, got %u", writes);

    // The car sleeps: one checkpoint at the window, the flash now says quiet.
    CHECK(pwr_tick(&cp, FSD_PWR_QUIET_MS) == 0, "just quiet -> nothing yet");
    CHECK(pwr_tick(&cp, FSD_PWR_CHECKPOINT_MS) == 2, "settled quiet -> checkpoint");
    CHECK(cp.quiet_on_flash, "flash says quiet");
    CHECK(pwr_tick(&cp, FSD_PWR_CHECKPOINT_MS + 600000u) == 0, "a long sleep -> no more");

    // 🔴 The trap: the car wakes and drives. Left alone, a power cut now reads
    // "quiet, then power lost" = SWITCHED. One correcting write at the first
    // busy tick, then nothing for the rest of the drive.
    CHECK(pwr_tick(&cp, 0u) == 1, "bus back after a checkpoint -> one correction");
    CHECK(!cp.quiet_on_flash, "flash no longer says quiet");
    writes = 0;
    for(uint32_t t = 0; t < 3600000u; t += 10u)
        writes += pwr_tick(&cp, 0u) != 0;
    CHECK(writes == 0u, "the rest of the drive -> no writes, got %u", writes);
    FsdPwrRecord corrected = {true, 7200000u, 0u};
    CHECK(
        fsd_pwr_verdict(FSD_PWR_RESET_POWERON, &corrected) == FSD_PWR_NO_SLEEP,
        "power cut mid-drive -> NO_SLEEP, not SWITCHED");

    // Quiet, but not for the window (a stop, a short lull): no checkpoint, so
    // nothing to correct when the frames come back.
    CHECK(pwr_tick(&cp, FSD_PWR_CHECKPOINT_MS - 1u) == 0, "short quiet -> nothing");
    CHECK(pwr_tick(&cp, 0u) == 0, "bus back without a checkpoint -> no correction");

    // A bus that flaps: each checkpoint is corrected once, and the cap on
    // checkpoints bounds the corrections with it.
    FsdPwrCheckpoint flap = {false, 0, false};
    unsigned checkpoints = 0, corrections = 0;
    for(int episode = 0; episode < (int)FSD_PWR_MAX_CHECKPOINTS + 5; episode++) {
        int w = pwr_tick(&flap, FSD_PWR_CHECKPOINT_MS);
        if(w == 2) checkpoints++;
        w = pwr_tick(&flap, 0u);
        if(w == 1) corrections++;
    }
    CHECK(checkpoints == FSD_PWR_MAX_CHECKPOINTS, "checkpoints capped, got %u", checkpoints);
    CHECK(corrections == checkpoints, "one correction per checkpoint, got %u", corrections);

    CHECK(!fsd_pwr_correction_due(NULL, 0u), "NULL checkpoint -> no correction");
}

static void test_power_filter_gate(void) {
    printf("\n-- power: no judgement while a capture filter is on --\n");
    FsdPwrFilterGate g = {false, false, 0};
    FsdPwrCheckpoint cp = {false, 0, false};
    uint32_t q = 0xDEADu;

    // No filter: what fsd_pwr_quiet_ms says.
    CHECK(fsd_pwr_judge_quiet(&g, 100000u, 99990u, true, false, &q) && q == 0u, "busy, no filter");
    CHECK(
        fsd_pwr_judge_quiet(&g, 100000u, 50000u, true, false, &q) && q == 50000u,
        "quiet 50 s, no filter, got %u",
        q);

    // A checkpoint is on flash, then a single-ID capture starts: the filter
    // hides the bus. Long silences through it judge nothing: no checkpoint,
    // no correction, however long.
    CHECK(pwr_tick(&cp, FSD_PWR_CHECKPOINT_MS) == 2, "checkpoint before the capture");
    unsigned judged = 0;
    for(uint32_t t = 200000u; t < 800000u; t += 10u)
        if(fsd_pwr_judge_quiet(&g, t, 200000u, true, true, &q)) judged++;
    CHECK(judged == 0u, "filtered: never judged, got %u", judged);
    CHECK(cp.quiet_on_flash, "the checkpoint is not corrected by the capture");

    // Filter off at 800 s, bus still silent: the quiet time restarts there,
    // and until it reaches FSD_PWR_QUIET_MS the bus is not judged (no
    // correction from "no frame for a moment").
    CHECK(
        !fsd_pwr_judge_quiet(&g, 800000u, 200000u, true, false, &q), "just cleared -> not judged");
    CHECK(
        !fsd_pwr_judge_quiet(&g, 800000u + FSD_PWR_QUIET_MS - 1u, 200000u, true, false, &q),
        "under the quiet threshold since the clear -> not judged");
    CHECK(
        fsd_pwr_judge_quiet(&g, 800000u + FSD_PWR_QUIET_MS, 200000u, true, false, &q) &&
            q == FSD_PWR_QUIET_MS,
        "quiet counts from the clear, got %u",
        q);
    CHECK(
        fsd_pwr_judge_quiet(&g, 800000u + FSD_PWR_CHECKPOINT_MS, 200000u, true, false, &q) &&
            q == FSD_PWR_CHECKPOINT_MS,
        "checkpoint window counted from the clear, got %u",
        q);

    // A frame after the clear: normal judging again, and real traffic may
    // correct the checkpoint.
    FsdPwrFilterGate h = {false, false, 0};
    CHECK(!fsd_pwr_judge_quiet(&h, 1000u, 900u, true, true, &q), "filtered");
    CHECK(!fsd_pwr_judge_quiet(&h, 2000u, 900u, true, false, &q), "cleared, nothing since");
    CHECK(
        fsd_pwr_judge_quiet(&h, 2500u, 2400u, true, false, &q) && q == 0u,
        "frame after clear -> busy");
    CHECK(pwr_tick(&cp, q) == 1, "real traffic after the capture -> the correction");
    // A frame stamped just ahead of now is traffic too.
    FsdPwrFilterGate k = {false, false, 0};
    fsd_pwr_judge_quiet(&k, 1000u, 900u, true, true, &q);
    fsd_pwr_judge_quiet(&k, 2000u, 900u, true, false, &q);
    CHECK(
        fsd_pwr_judge_quiet(&k, 2001u, 2003u, true, false, &q) && q == 0u, "stamp ahead -> busy");
    // Never heard a frame: the normal "not quiet", filter or not.
    FsdPwrFilterGate n = {false, false, 0};
    fsd_pwr_judge_quiet(&n, 1000u, 0u, false, true, &q);
    CHECK(fsd_pwr_judge_quiet(&n, 60000u, 0u, false, false, &q) && q == 0u, "no frame ever -> 0");
    CHECK(!fsd_pwr_judge_quiet(NULL, 0u, 0u, true, false, &q), "NULL gate");
}

static void test_power_deepsleep_usb(void) {
    printf("\n-- power: deep-sleep wake and USB guard --\n");

    // A board that woke from its own deep sleep kept its power through it: a
    // switched feed would have cut power and booted POWERON instead.
    CHECK(
        fsd_pwr_deepsleep_verdict(true) == FSD_PWR_ALWAYS_ON,
        "deep-sleep wake on a quiet bus -> always-on");
    CHECK(
        fsd_pwr_deepsleep_verdict(false) == FSD_PWR_UNKNOWN,
        "deep-sleep wake but not on a quiet bus -> no claim");

    // The trap: USB powered the board, so "still alive" proves nothing. An
    // always-on claim becomes inconclusive; everything else stands, because it
    // came from a reboot that happened whether or not USB was attached.
    CHECK(
        fsd_pwr_apply_usb_guard(FSD_PWR_ALWAYS_ON, true) == FSD_PWR_USB,
        "always-on + USB -> inconclusive (USB)");
    CHECK(
        fsd_pwr_apply_usb_guard(FSD_PWR_ALWAYS_ON, false) == FSD_PWR_ALWAYS_ON,
        "always-on without USB stands");
    CHECK(
        fsd_pwr_apply_usb_guard(FSD_PWR_SWITCHED, true) == FSD_PWR_SWITCHED,
        "switched is not masked by USB");
    CHECK(
        fsd_pwr_apply_usb_guard(FSD_PWR_CRASH, true) == FSD_PWR_CRASH,
        "crash is not masked by USB");
    CHECK(
        fsd_pwr_apply_usb_guard(FSD_PWR_UNKNOWN, true) == FSD_PWR_UNKNOWN,
        "no verdict stays no verdict");

    // The two values added in the rework are printable, or the serial/dash line
    // reports a number.
    CHECK(fsd_pwr_reset_str(FSD_PWR_RESET_DEEPSLEEP)[0] != '\0', "deep-sleep reset has a name");
    CHECK(fsd_pwr_verdict_str(FSD_PWR_USB)[0] != '\0', "USB verdict has a name");
}

static void test_power_restart_count(void) {
    printf("\n-- power: restarts since power-on --\n");

    // A power-on starts from zero, whatever RTC RAM held.
    CHECK(fsd_pwr_restart_count(FSD_PWR_RESET_POWERON, false, 0u) == 0u, "power-on -> 0");
    CHECK(fsd_pwr_restart_count(FSD_PWR_RESET_POWERON, true, 7u) == 0u, "power-on resets a count");

    // Crashes, watchdogs, brownouts, software and pin resets each add one.
    CHECK(fsd_pwr_restart_count(FSD_PWR_RESET_SOFTWARE, true, 2u) == 3u, "crash/software -> +1");
    CHECK(fsd_pwr_restart_count(FSD_PWR_RESET_BROWNOUT, true, 2u) == 3u, "brownout -> +1");
    CHECK(fsd_pwr_restart_count(FSD_PWR_RESET_EXTERNAL, true, 2u) == 3u, "reset pin -> +1");
    CHECK(fsd_pwr_restart_count(FSD_PWR_RESET_UNKNOWN, true, 2u) == 3u, "unknown reset -> +1");

    // A deep-sleep wake is a boot but not a restart: the count stays.
    CHECK(
        fsd_pwr_restart_count(FSD_PWR_RESET_DEEPSLEEP, true, 2u) == 2u,
        "deep-sleep wake -> unchanged");

    // RTC RAM lost without a power-on (a deep brownout): count from zero.
    CHECK(
        fsd_pwr_restart_count(FSD_PWR_RESET_BROWNOUT, false, 99u) == 1u,
        "lost RTC + brownout -> 1");

    // A LilyGO's night: power-on, ten CAN wakes, one crash, ten more wakes.
    uint32_t n = fsd_pwr_restart_count(FSD_PWR_RESET_POWERON, false, 0u);
    for(int i = 0; i < 10; i++)
        n = fsd_pwr_restart_count(FSD_PWR_RESET_DEEPSLEEP, true, n);
    n = fsd_pwr_restart_count(FSD_PWR_RESET_SOFTWARE, true, n);
    for(int i = 0; i < 10; i++)
        n = fsd_pwr_restart_count(FSD_PWR_RESET_DEEPSLEEP, true, n);
    CHECK(n == 1u, "20 deep-sleep wakes and one crash -> 1 restart, got %u", n);

    CHECK(
        fsd_pwr_restart_count(FSD_PWR_RESET_SOFTWARE, true, UINT32_MAX) == UINT32_MAX,
        "the count saturates");
}

int main(void) {
    printf("test_fsd_core: Tesla FSD protocol core host tests\n");
    test_set_bit();
    test_read_mux();
    test_is_selected();
    test_detect_hw();
    test_follow_distance();
    test_autopilot_hw4();
    test_hw4_mux2_profile_layout();
    test_autopilot_hw3();
    test_hw4_ignores_hw3_override();
    test_summon_unlock();
    test_continue_on_green();
    test_tlssc_bit38();
    test_isa_checksum();
    test_di_speed();
    test_tlssc_restore();
    test_track_mode_crc();
    test_track_mode_adjust();
    test_sccm_crc();
    test_nag_killer();
    test_nag_killer_faithful();
    test_steering_angle();
    test_bms_thermal();
    test_ui_soc();
    test_bms_soc();
    test_nag_burst_cap();
    test_signal_config();
    test_abort_guard();
    test_fsd_events();
    test_capability();
    test_profile_db();
    test_blackbox_summary();
    test_can_ops();
    test_additive_checksum();
    test_candump_format();
    test_gtw_car_state();
    test_gtw_car_state_counter();
    test_legacy();
    test_esp_status();
    test_das_status();
    test_das_status_hw4_byte0();
    test_das_state_engaged();
    test_autopark();
    test_autopark_176();
    test_gtw_tier();
    test_driver_assist();
    test_rhd_override();
    test_telemetry_off();
    test_apmv3_branch();
    test_gtw_shield();
    test_scroll_press();
    test_misc_parsers();
    test_readonly_parsers();
    test_tpms_real_frames();
    test_tpms_dbc_form();
    test_tpms_malformed();
    test_tpms_sources_and_age();
    test_extras_and_builders();
    test_profile();
    test_state_init();
    test_blackbox_filter();

    test_power_verdict();
    test_power_quiet_ms();
    test_power_checkpoint();
    test_power_correction();
    test_power_filter_gate();
    test_power_deepsleep_usb();
    test_power_restart_count();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
