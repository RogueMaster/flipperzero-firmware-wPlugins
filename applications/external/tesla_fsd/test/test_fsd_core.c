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
#include "fsd_capture.h"
#include "fsd_checksum.h"
#include "fsd_handler.h"

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
    f.buffer[0] = 0x00; // 0
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_Legacy, "das_hw=0 -> Legacy");
    f.buffer[0] = 0x40; // 1
    CHECK(fsd_detect_hw_version(&f) == TeslaHW_Legacy, "das_hw=1 -> Legacy");
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
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW4 mux0 reports modified");
    CHECK((f.buffer[5] & 0x40) != 0, "HW4 mux0 bit46 set");
    CHECK((f.buffer[7] & 0x10) != 0, "HW4 mux0 bit60 set");
    CHECK(s.fsd_enabled, "HW4 mux0 sets fsd_enabled");

    // mux1 -> nag bit19 cleared, bit47 set
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 1;
    f.buffer[2] = 0x08; // pre-set bit19 so we prove it is cleared
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW4 mux1 reports modified");
    CHECK((f.buffer[2] & 0x08) == 0, "HW4 mux1 bit19 cleared");
    CHECK((f.buffer[5] & 0x80) != 0, "HW4 mux1 bit47 set");

    // mux2 -> speed profile written to byte7 bits7:5
    zero(&f);
    f.data_lenght = 8;
    f.buffer[0] = 2;
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW4 mux2 reports modified");
    CHECK(
        ((f.buffer[7] >> 5) & 0x07) == 4,
        "HW4 mux2 speed_profile=4 got %u",
        (f.buffer[7] >> 5) & 0x07);
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
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW3 mux0 reports modified");
    CHECK((f.buffer[5] & 0x40) != 0, "HW3 mux0 bit46 set");
    CHECK(
        ((f.buffer[6] >> 1) & 0x03) == 2,
        "HW3 mux0 speed_profile bits got %u",
        (f.buffer[6] >> 1) & 0x03);
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
    s.op_mode = OpMode_Service; // gated behind Service mode
    s.track_mode_state = 2; // user toggled
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

    // Service-mode gate: not in Service -> no-op
    s.op_mode = OpMode_Active;
    CANFRAME g;
    zero(&g);
    g.data_lenght = 8;
    CHECK(fsd_handle_track_mode_inject(&s, &g) == false, "track-mode gated outside Service");
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
    CHECK(fsd_handle_nag_killer(&s, &in, &out), "nag echo emitted on level 0");
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
    CHECK(fsd_handle_nag_killer(&s, &in, &out2) == false, "nag skips when hands detected");
    in.buffer[4] = 0x00;
    s.das_hands_on_state = 0; // DAS satisfied
    CHECK(fsd_handle_nag_killer(&s, &in, &out2) == false, "nag skips when DAS satisfied");
    s.das_hands_on_state = 0xFF;
    s.nag_killer = false;
    CHECK(fsd_handle_nag_killer(&s, &in, &out2) == false, "nag skips when disabled");
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

// ── state init ────────────────────────────────────────────────────────────────
static void test_state_init(void) {
    FSDState s;
    fsd_state_init(&s, TeslaHW_HW4);
    CHECK(s.hw_version == TeslaHW_HW4, "init applies HW4");
}

int main(void) {
    printf("test_fsd_core: Tesla FSD protocol core host tests\n");
    test_set_bit();
    test_read_mux();
    test_is_selected();
    test_detect_hw();
    test_follow_distance();
    test_autopilot_hw4();
    test_autopilot_hw3();
    test_isa_checksum();
    test_di_speed();
    test_tlssc_restore();
    test_track_mode_crc();
    test_sccm_crc();
    test_nag_killer();
    test_can_ops();
    test_additive_checksum();
    test_candump_format();
    test_state_init();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
