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
#include "fsd_blackbox_summary.h"
#include "fsd_capability.h"
#include "fsd_capture.h"
#include "fsd_checksum.h"
#include "fsd_events.h"
#include "fsd_handler.h"
#include "fsd_profile.h"
#include "fsd_profile_db.h"

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

// ── 0x3FD autopilot frame, HW3 ────────────────────────────────────────────────
static void test_autopilot_hw3(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW3;
    s.force_fsd = true;
    s.speed_profile = 2;

    CANFRAME f;
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 0; // mux0
    CHECK(fsd_handle_autopilot_frame(&s, &f, 0), "HW3 mux0 reports modified");
    CHECK((f.buffer[5] & 0x40) != 0, "HW3 mux0 bit46 set");
    CHECK(
        ((f.buffer[6] >> 1) & 0x03) == 2,
        "HW3 mux0 speed_profile bits got %u",
        (f.buffer[6] >> 1) & 0x03);
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

    // Steering config: 0x129, signed LE bytes hi=1 lo=0, *0.1.
    s.cfg_steer_id = 0x129;
    s.cfg_steer_hi = 1;
    s.cfg_steer_lo = 0;
    CANFRAME h;
    zero(&h);
    h.canId = 0x129;
    h.data_lenght = 4;
    h.buffer[0] = 0x2C;
    h.buffer[1] = 0x01; // 0x012C = 300 -> 30.0 deg
    fsd_apply_signal_config(&s, &h, 12000u);
    CHECK(
        s.steering_angle_deg > 29.0f && s.steering_angle_deg < 31.0f,
        "cfg: steering = %.1f deg (expect ~30)",
        (double)s.steering_angle_deg);

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

    // ── bus hint: steering, no 0x370 -> Chassis/Vehicle-like ──
    FSDCapSeen chassis = {0};
    chassis.steer = true;
    r = fsd_capability_eval(chassis, TeslaHW_HW4);
    CHECK(r.bus_hint == CAP_HINT_CHASSIS, "steer w/o 0x370: hint Chassis-like");
    CHECK(r.soft_engage == CAP_OK, "0x129 present: soft engage OK");

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
    se.steering_angle_deg = 90.0f; // hard turn
    CHECK(fsd_soft_engage_allows(&se) == true, "soft_engage off -> always allowed");
    se.soft_engage = true;
    CHECK(fsd_soft_engage_allows(&se) == false, "soft_engage on + turning -> hold");
    CHECK(se.soft_engage_latched == false, "soft_engage: turning does not latch");
    se.steering_angle_deg = -3.0f; // within +/-5 deg of centre
    CHECK(fsd_soft_engage_allows(&se) == true, "soft_engage: centred -> allow + latch");
    CHECK(se.soft_engage_latched == true, "soft_engage: latched once centred");
    se.steering_angle_deg = 120.0f; // now turning, but already latched
    CHECK(
        fsd_soft_engage_allows(&se) == true,
        "soft_engage: stays allowed once latched (mid-drive turns OK)");
    se.soft_engage_latched = false; // AP drop resets the latch
    CHECK(
        fsd_soft_engage_allows(&se) == false,
        "soft_engage: re-holds after latch reset while turning");
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

    // ── 2->6 with a MISSED 3 at 0 km/h: blocks, then fresh speed > 20 ENDS the
    // episode; slowing back down in the same state 6 does not re-block (#176).
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
    ap_update(&s, 6, 2000); // fresh speed 25 > 20
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "fresh speed >20 ends the false episode");
    CHECK(fsd_can_transmit(&s), "released by speed: TX allowed");
    s.vehicle_speed_kph = 5.0f;
    s.last_speed_tick_ms = 2900;
    ap_update(&s, 6, 3000); // back to 5 km/h, still in 6
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "5 km/h after release, same 6: no block");
    ap_update(&s, 6, 5000); // speed stale again
    CHECK(!s.autopark_tx_block, "stale speed after release: episode stays ended");

    // ── SNA speed must NOT release. DI_vehicleSpeed raw 0xFFF (4095) is SNA and
    // decodes to 287.6 kph; only a fresh VALID reading > 20 releases (ending the
    // episode, so a later SNA reading cannot re-open it, #176).
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

    // ── 2->6 at 0 km/h: blocked; > 20 km/h ends the episode; back to 5 km/h in
    // the same 6 is NOT blocked again. A maneuver bit re-opens it.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_episode && s.autopark_tx_block, "2->6 at 0 km/h: blocked");
    ap_kph(&s, 12.0f, 300);
    CHECK(s.autopark_tx_block, "12 km/h (<= 20) keeps the block");
    ap_kph(&s, 25.0f, 400);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "> 20 km/h ends the episode");
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

    // SCCM_steeringAngle (0x129): int16 LE byte0-1 * 0.1
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 4;
    f.buffer[0] = 0x64;
    f.buffer[1] = 0x00; // raw = 100 -> 10.0 deg
    fsd_handle_steering_angle(&s, &f);
    CHECK(
        fabs(s.steering_angle_deg - 10.0f) < 0.05f,
        "steer angle got %.2f",
        (double)s.steering_angle_deg);

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

    // UI_ratedConsumption (0x33A): raw = byte1<<8|byte0, * 0.1
    memset(&s, 0, sizeof(s));
    zero(&f);
    f.data_lenght = 4;
    f.buffer[0] = 0x10;
    f.buffer[1] = 0x00; // raw = 16 -> 1.6
    fsd_handle_energy_consumption(&s, &f);
    CHECK(fabs(s.energy_wh_per_km - 1.6f) < 0.05f, "energy got %.2f", (double)s.energy_wh_per_km);
    CHECK(s.energy_seen, "energy seen");
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

    // Chatty non-diagnostic frames are dropped (the ~15x rate cut).
    CHECK(!fsd_blackbox_should_record(CAN_ID_BMS_HV_BUS), "0x132 BMS dropped");
    CHECK(!fsd_blackbox_should_record(CAN_ID_ESP_WHEELSPD), "0x175 wheel speeds dropped");
    CHECK(!fsd_blackbox_should_record(0x000), "0x000 dropped");
    CHECK(!fsd_blackbox_should_record(0x7FF), "0x7FF dropped");
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
    test_extras_and_builders();
    test_profile();
    test_state_init();
    test_blackbox_filter();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
