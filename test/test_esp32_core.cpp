/*
 * test_esp32_core.cpp — host unit tests for the ESP32 firmware handlers.
 *
 * Compiles esp32/.firmware/fsd_handler.cpp with the host C++ compiler (it has
 * no Arduino dependencies), so the ESP32 copies of the protocol handlers get
 * the same red-CI coverage as the Flipper core in test_fsd_core.c. The two
 * builds disagreed on 0x318 OTA detection (#183) because nothing here ever
 * compiled the ESP32 side.
 *
 * Build + run:  make -C test check
 */

#include <stdio.h>
#include <string.h>

#include "fsd_handler.h"  // esp32/.firmware/fsd_handler.h (first on the include path)
#include "fsd_ota.h"      // shared reference: fsd_ota_update()
#include "fsd_can_ops.h"  // tesla_can_rx_accept / tesla_can_tx_valid (driver RX/TX filter)
#include "fsd_checksum.h" // tesla_additive_checksum (0x334 / 0x118 vectors)
#include "fsd_cp_status.h" // 0x25D CP_status decode + plug guard + rewrite predicate (#228)
#include "fsd_steer.h"     // 0x129 steering-angle decode + freshness
#include "fsd_bms.h"       // 0x312 pack-temperature gate
#include "fsd_freshness.h" // stale-frame TX gate + nag fault/override guard (#176)
#include "blackbox_flush.h" // black-box ring push, window search, save slices
#include "blackbox_ram.h"   // black-box RAM-backend save memory discipline (#191)
#include "blackbox_name.h"  // black-box event names and their order across reboots
#include "gear_tx_route.h" // Continuous AP 0x229 press bus choice

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, ...)                                                        \
    do {                                                                        \
        if (cond) {                                                             \
            g_pass++;                                                           \
        } else {                                                                \
            g_fail++;                                                           \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                       \
            printf(__VA_ARGS__);                                                \
            printf("\n");                                                       \
        }                                                                       \
    } while (0)

// Real 0x318 byte6, Model S Palladium 2022, consecutive received frames
// (decimated): a +2 rolling counter, always odd, not an update flag (#183).
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

static void esp32_feed(FSDState* s, uint8_t b6) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_GTW_CAR_STATE;
    f.dlc = 8;
    f.data[6] = b6;
    fsd_handle_gtw_car_state(s, &f);
}

// Feed one byte6 sequence to the ESP32 handler and to the shared fsd_ota_update()
// the Flipper handler runs; both must agree after every frame. Returns how many
// times the ESP32 side latched.
static int parity_run(const char* name, const uint8_t* b6, int n) {
    FSDState esp, ref;
    memset(&esp, 0, sizeof(esp));
    memset(&ref, 0, sizeof(ref));
    int diverged_at = -1;
    int latches = 0;
    for (int i = 0; i < n; i++) {
        bool was = esp.tesla_ota_in_progress;
        esp32_feed(&esp, b6[i]);
        fsd_ota_update(&ref, b6[i]);
        if (diverged_at < 0 && (esp.tesla_ota_in_progress != ref.tesla_ota_in_progress ||
                                esp.ota_raw_state != ref.ota_raw_state ||
                                esp.ota_assert_count != ref.ota_assert_count ||
                                esp.ota_clear_count != ref.ota_clear_count))
            diverged_at = i;
        if (!was && esp.tesla_ota_in_progress) latches++;
    }
    CHECK(diverged_at < 0, "%s: ESP32 diverges from fsd_ota_update at frame %d", name,
          diverged_at);
    return latches;
}

// ── 0x318 GTW_carState on the ESP32 handler ───────────────────────────────────
static void test_gtw_car_state(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));

    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.dlc = 6; // short frame: ignored
    f.data[6] = 0x42;
    fsd_handle_gtw_car_state(&s, &f);
    CHECK(!s.ota_last_valid && s.ota_clear_count == 0, "ESP32 OTA dlc<7 frame ignored");

    // Stable raw 1 used to latch the ESP32 (old OTA_IN_PROGRESS_RAW_VALUE); it must not.
    for (int i = 0; i < 10; i++)
        esp32_feed(&s, 0x41);
    CHECK(!s.tesla_ota_in_progress, "ESP32 OTA stable raw 1 never latches");

    // Stable raw-2 flag: latches on the 3rd repeat (frame 4), not the 2nd.
    memset(&s, 0, sizeof(s));
    for (int i = 0; i < 3; i++)
        esp32_feed(&s, 0x42);
    CHECK(!s.tesla_ota_in_progress, "ESP32 OTA 2 repeats -> not yet");
    esp32_feed(&s, 0x42);
    CHECK(s.tesla_ota_in_progress, "ESP32 OTA 3rd repeat -> latched");
    CHECK(s.ota_raw_state == 2, "ESP32 OTA raw_state 2 got %u", s.ota_raw_state);

    // Released by exactly 6 non-asserting frames.
    for (int i = 0; i < 5; i++)
        esp32_feed(&s, 0x41);
    CHECK(s.tesla_ota_in_progress, "ESP32 OTA 5 clear frames -> still latched");
    esp32_feed(&s, 0x41);
    CHECK(!s.tesla_ota_in_progress, "ESP32 OTA 6th clear frame -> released");
}

// ── ESP32 vs shared reference, frame by frame ─────────────────────────────────
static void test_ota_parity(void) {
    int run = 0, max_run = 0;
    for (size_t i = 0; i < sizeof(k_palladium_318_b6); i++) {
        run = ((k_palladium_318_b6[i] & 0x03u) == 1u) ? run + 1 : 0;
        if (run > max_run) max_run = run;
    }
    CHECK(max_run >= 3, "Palladium fixture raw-1 run %d (old ESP32 3-frame trigger)", max_run);
    CHECK(parity_run("Palladium", k_palladium_318_b6, (int)sizeof(k_palladium_318_b6)) == 0,
          "ESP32 OTA Palladium capture never latches");

    FSDState s;
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    bool tx_ok = true;
    for (size_t i = 0; i < sizeof(k_palladium_318_b6); i++) {
        esp32_feed(&s, k_palladium_318_b6[i]);
        if (!fsd_can_transmit(&s)) tx_ok = false;
    }
    CHECK(tx_ok, "ESP32 TX never paused by the Palladium counter");

    char name[48];
    uint8_t seq[512];

    // +2 counter 0x21..0x3F, keeping every k-th frame (2: raw pinned at 1,
    // 16: aliased to a constant 0x21).
    static const int keep_every[] = {1, 2, 3, 16};
    for (size_t k = 0; k < sizeof(keep_every) / sizeof(keep_every[0]); k++) {
        for (int i = 0; i < 128; i++)
            seq[i] = (uint8_t)(0x21 + 2 * ((i * keep_every[k]) % 16));
        snprintf(name, sizeof(name), "+2 counter keep 1/%d", keep_every[k]);
        CHECK(parity_run(name, seq, 128) == 0, "ESP32 OTA %s never latches", name);
    }

    // +2 counter under pseudo-random RX drops (fixed seed).
    for (int keep_q = 1; keep_q <= 3; keep_q++) {
        uint32_t rng = 0x31800u + (uint32_t)keep_q;
        int n = 0;
        for (int i = 0; n < 256; i++)
            if ((int)(xorshift32(&rng) & 3u) < keep_q) seq[n++] = (uint8_t)(0x21 + 2 * (i % 16));
        snprintf(name, sizeof(name), "+2 counter random drops keep %d/4", keep_q);
        CHECK(parity_run(name, seq, n) == 0, "ESP32 OTA %s never latches", name);
    }

    // +1 counter kept every 4th frame: phase 2 is a constant raw 2.
    for (int phase = 0; phase < 4; phase++) {
        for (int i = 0; i < 128; i++)
            seq[i] = (uint8_t)(phase + 4 * i);
        snprintf(name, sizeof(name), "+1 counter every 4th phase %d", phase);
        CHECK(parity_run(name, seq, 128) == 0, "ESP32 OTA %s never latches", name);
    }

    // Flag latch, release, then a changed raw-2 byte restarting the count.
    static const uint8_t flag[] = {
        0x42, 0x42, 0x42, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x42, 0x42, 0x42, 0x46, 0x46, 0x46, 0x46, 0x02, 0x06, 0x0A,
    };
    CHECK(parity_run("flag", flag, (int)sizeof(flag)) == 2,
          "ESP32 OTA flag sequence latches twice");

    // Random byte6 runs (1..8 frames, biased to raw 2): every transition type.
    uint32_t rng = 0x318u;
    int n = 0;
    while (n < (int)sizeof(seq)) {
        uint8_t v = (uint8_t)xorshift32(&rng);
        if (xorshift32(&rng) & 1u) v = (uint8_t)((v & 0xFCu) | 0x02u);
        int len = 1 + (int)(xorshift32(&rng) % 8u);
        for (int j = 0; j < len && n < (int)sizeof(seq); j++)
            seq[n++] = v;
    }
    CHECK(parity_run("random runs", seq, n) >= 2, "ESP32 OTA random runs latch and release");
}

// ── TX gate: OTA latch vs ignore_ota vs listen-only ───────────────────────────
static void test_ota_tx_gate(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    CHECK(fsd_can_transmit(&s), "Active, no OTA -> TX allowed");
    for (int i = 0; i < 4; i++)
        esp32_feed(&s, 0x42);
    CHECK(s.tesla_ota_in_progress, "stable raw-2 flag latched");
    CHECK(!fsd_can_transmit(&s), "OTA latched -> TX blocked");
    s.ignore_ota = true;
    CHECK(fsd_can_transmit(&s), "OTA latched + ignore_ota -> TX allowed");
    s.op_mode = OpMode_ListenOnly;
    CHECK(!fsd_can_transmit(&s), "listen-only blocks TX even with ignore_ota");
    s.ignore_ota = false;
    for (int i = 0; i < 6; i++)
        esp32_feed(&s, 0x41);
    CHECK(!s.tesla_ota_in_progress, "OTA released");
    CHECK(!fsd_can_transmit(&s), "listen-only blocks TX with no OTA");
    s.op_mode = OpMode_Active;
    CHECK(fsd_can_transmit(&s), "Active after release -> TX allowed");
}

// ── HW4/HW3 DAS decode: byte0 low nibble (#177) + autopark bits (#180) ────────
static void esp32_das(FSDState* s, uint8_t b0, uint8_t b1, uint8_t b3,
                      void (*fn)(FSDState*, const CanFrame*)) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x39B; f.dlc = 8;
    f.data[0] = b0; f.data[1] = b1; f.data[3] = b3;
    f.data[5] = (uint8_t)(2u << 2);  // hands-on 2 (valid, keeps das_seen meaningful)
    fn(s, &f);
}

static void test_das_decode(void) {
    FSDState s;

    // Real anoblekman Highland HW4 frames: ap_state from byte0, byte1 = 0x0A noise.
    memset(&s, 0, sizeof(s));
    esp32_das(&s, 0x01, 0x0A, 0xE0, fsd_handle_das_status_hw4);   // parked, no bits
    CHECK(s.das_ap_state == 1, "hw4 parked byte0 ap_state=1 got %u", s.das_ap_state);
    CHECK(!s.ap_active, "hw4 state 1 -> ap_active false");
    CHECK(!s.autopark_ready && !s.autopark_waiting_brake, "byte3 0xE0 -> no autopark bits");

    esp32_das(&s, 0x06, 0x0A, 0xE5, fsd_handle_das_status_hw4);   // autopark, byte3 0xE5
    CHECK(s.das_ap_state == 6, "hw4 autopark byte0 ap_state=6 got %u", s.das_ap_state);
    CHECK(s.ap_active, "hw4 state 6 -> ap_active true (engaged 3..6)");
    CHECK(s.autopark_ready && s.autopark_waiting_brake && !s.autopark_parked,
          "byte3 0xE5 -> ready+waitingForBrake");

    // #116 fixtures decode to their byte0 states; byte1 noise never changes it.
    memset(&s, 0, sizeof(s));
    esp32_das(&s, 0x02, 0x10, 0x00, fsd_handle_das_status_hw4);
    CHECK(s.das_ap_state == 2, "#116 READY byte0=2 got %u", s.das_ap_state);
    static const uint8_t noise[] = {0x6A, 0x10, 0x0A, 0xF0};
    for (unsigned i = 0; i < sizeof(noise); i++) {
        esp32_das(&s, 0x03, noise[i], 0x00, fsd_handle_das_status_hw4);
        CHECK(s.das_ap_state == 3, "byte1=0x%02X noise keeps ap_state=3 got %u",
              noise[i], s.das_ap_state);
    }

    // HW3 0x399 parser: same byte0 decode + engaged 3..6 (not == 3).
    memset(&s, 0, sizeof(s));
    esp32_das(&s, 0x06, 0x00, 0x00, fsd_handle_das_status_hw3);
    CHECK(s.das_ap_state == 6 && s.ap_active, "hw3 state 6 -> ap_active (engaged, not ==3)");
    esp32_das(&s, 0x02, 0x00, 0x00, fsd_handle_das_status_hw3);
    CHECK(s.das_ap_state == 2 && !s.ap_active, "hw3 AVAILABLE(2) -> not active");
}

// ── engaged helper (shared) ───────────────────────────────────────────────────
static void test_engaged(void) {
    for (uint8_t v = 0; v <= 15; v++) {
        bool exp = (v >= 3 && v <= 6);
        CHECK(fsd_das_state_engaged(v) == exp, "engaged(%u) exp %d", v, exp);
    }
}

// ── in-car Autopark pause (#180) ──────────────────────────────────────────────
static void ap_update(FSDState* s, uint8_t st, uint32_t now) {
    s->das_ap_state = st;
    fsd_autopark_update(s, now);
}
// Feed one 0x257 DI_speed frame through the ESP32 parser, stamp freshness the way
// main.cpp does, then run the Autopark update at the same instant.
static void ap_speed(FSDState* s, uint8_t b1, uint8_t b2, uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x257u; f.dlc = 8;
    f.data[1] = b1; f.data[2] = b2;
    fsd_handle_di_speed(s, &f);
    s->last_speed_tick_ms = now;
    fsd_autopark_update(s, now);
}
static void test_autopark(void) {
    FSDState s;

    // Autopark sequence: blocks during the state-6 episode, releases after.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 1, 100);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "parked: no block");
    s.autopark_ready = true; s.autopark_waiting_brake = true;
    ap_update(&s, 6, 1200);
    CHECK(s.autopark_episode && s.autopark_tx_block, "state 6 autopark: blocked");
    CHECK(!fsd_can_transmit(&s), "autopark blocks fsd_can_transmit");
    s.autopark_ready = false; s.autopark_waiting_brake = false;
    ap_update(&s, 1, 22000);
    CHECK(!s.autopark_episode && fsd_can_transmit(&s), "6->1: released");

    // FSD 2->3->6 with bits clear never blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 2, 100); ap_update(&s, 3, 200); ap_update(&s, 6, 300);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "FSD 2->3->6: no block");

    // 2->6 (missed 3) at 0 km/h blocks; stale keeps block; fresh speed past the
    // release speed ENDS the episode, so slowing back down in the same 6 does not
    // re-block (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.speed_seen = true; s.last_speed_tick_ms = 100; s.vehicle_speed_kph = 0.0f;
    ap_update(&s, 2, 100); ap_update(&s, 6, 200);
    CHECK(s.autopark_tx_block, "2->6 missed-3 at 0 km/h: blocked");
    ap_update(&s, 6, 1500);       // last speed 1.4 s ago -> stale
    CHECK(s.autopark_tx_block, "stale speed keeps block (fail safe)");
    s.vehicle_speed_kph = 25.0f; s.last_speed_tick_ms = 1900;
    ap_update(&s, 6, 2000);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "fresh 25 km/h ends the episode");
    s.vehicle_speed_kph = 5.0f; s.last_speed_tick_ms = 2900;
    ap_update(&s, 6, 3000);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "5 km/h after release, same 6: no block");
    ap_update(&s, 6, 5000);       // stale again
    CHECK(!s.autopark_tx_block, "stale speed after release: episode stays ended");

    // SNA speed must NOT release: raw 0xFFF (4095) decodes to 287.6 kph. A valid
    // release ends the episode, so a later SNA reading cannot re-open it (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.autopark_ready = true;
    ap_update(&s, 1, 100); ap_update(&s, 6, 200);
    CHECK(s.autopark_tx_block, "SNA test: episode blocked before any speed");
    ap_speed(&s, 0xF0, 0xFF, 300);                    // raw 0xFFF = SNA, fresh
    CHECK(s.vehicle_speed_kph > 287.5f && s.vehicle_speed_kph < 287.7f,
          "SNA raw 0xFFF decodes to %.2f kph (287.6)", s.vehicle_speed_kph);
    CHECK(s.autopark_tx_block && !fsd_can_transmit(&s), "fresh SNA speed keeps the block");
    ap_speed(&s, 0xF0, 0xFD, 400);                    // raw 4063 = 285.04, invalid
    CHECK(s.autopark_tx_block, "raw 4063 (above max valid) keeps the block");
    ap_speed(&s, 0xD0, 0x32, 500);                    // raw 813 = 25.04 kph, fresh
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "fresh valid 25 kph releases");
    ap_speed(&s, 0xF0, 0xFF, 600);                    // back to SNA
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "SNA after release: episode stays ended");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.autopark_ready = true;
    ap_update(&s, 1, 100); ap_update(&s, 6, 200);
    ap_speed(&s, 0xE0, 0xFD, 300);                    // raw 4062 = 284.96, max valid
    CHECK(!s.autopark_tx_block, "raw 4062 (max valid 284.96 kph) releases");

    // Maneuver bit rising mid-6 at low speed blocks; autoparkReady alone does not.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 3, 100); ap_update(&s, 6, 200);
    CHECK(!s.autopark_tx_block, "FSD 3->6 no bits: no block");
    s.autopark_ready = true; ap_update(&s, 6, 250);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "autoparkReady alone mid-6: no block");
    s.autopark_parked = true; ap_update(&s, 6, 300);
    CHECK(s.autopark_tx_block, "maneuver bit rising mid-6: blocks");

    // ignore_ota does NOT override; listen-only still blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.ignore_ota = true; s.autopark_ready = true;
    ap_update(&s, 6, 200);
    CHECK(s.autopark_tx_block && !fsd_can_transmit(&s), "ignore_ota does not override autopark");
    s.op_mode = OpMode_ListenOnly;
    CHECK(!fsd_can_transmit(&s), "listen-only blocks TX");
}

// ── #176 parity: Autopark episode vs normal state-6 driving ──────────────────
// Same sequences as test_autopark_176 in test_fsd_core.c, through the ESP32
// 0x39B / 0x257 parsers and fsd_can_transmit().
static const uint8_t k_das_parked[8]   = {0x01, 0x0A, 0xDF, 0xE0, 0xB0, 0x08, 0x71, 0x91};
static const uint8_t k_das_autopark[8] = {0x06, 0x0A, 0xDF, 0xE5, 0xB0, 0x08, 0x21, 0x4B};
static void ap_frame(FSDState* s, const uint8_t b[8], uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x39Bu; f.dlc = 8;
    memcpy(f.data, b, 8);
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

    // AP engaged 3->6 at 30 km/h, then city traffic 0..30 km/h with
    // autoparkReady toggling while in 6: never blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 30.0f, 0);
    ap_das(&s, 0x02, 0xE0, 50); ap_das(&s, 0x03, 0xE0, 100); ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.autopark_tx_block, "#176 AP 3->6 at 30 km/h: no block");
    blocked = 0;
    for (uint32_t i = 0; i < 600; i++) {
        uint32_t t = 300 + i * 100;
        uint32_t ph = i % 60;
        ap_kph(&s, (float)(ph < 30 ? ph : 60 - ph), t);
        ap_das(&s, 0x06, ((i / 7) & 1u) ? 0xE1 : 0xE0, t + 10);
        if (s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(blocked == 0, "#176 city traffic in 6, autoparkReady toggling: %d blocked frames",
          blocked);

    // 2->6 (missed 3) at 40 km/h: moving at entry -> no episode, ever.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 40.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150); ap_das(&s, 0x06, 0xE1, 200);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "2->6 at 40 km/h: no episode");
    ap_kph(&s, 0.0f, 300); ap_das(&s, 0x06, 0xE1, 350);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "2->6 at 40, then stop + ready: no block");

    // maneuver_recent opens an episode entering 6 from an engaged state; a recent
    // autoparkReady does not (AP re-engaging near parked cars, not Autopark) (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_das(&s, 0x03, 0xE0, 100);
    ap_das(&s, 0x03, 0xE4, 1000);                     // waitingForBrake seen at t=1000
    ap_das(&s, 0x06, 0xE0, 2100);                     // 3->6, no bit now, recent (1.1 s)
    CHECK(s.autopark_tx_block, "maneuver_recent opens episode on 3->6");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_das(&s, 0x03, 0xE0, 100);
    ap_das(&s, 0x03, 0xE1, 1000);                     // autoparkReady seen while engaged
    ap_das(&s, 0x06, 0xE0, 2100);                     // 3->6, ready was recent
    CHECK(!s.autopark_tx_block, "autoparkReady recency on 3->6: no episode");

    // AP re-engaging from a standstill: 2->3->6 at 0..5 km/h with autoparkReady
    // set (byte3 bit0 only, no maneuver bit), never exceeding 20 km/h. Enters 6
    // from an engaged state -> not Autopark, never blocks near parked cars (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_das(&s, 0x02, 0xE1, 150); ap_das(&s, 0x03, 0xE1, 200); ap_das(&s, 0x06, 0xE1, 300);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "2->3->6 at 0 km/h + ready: no episode");
    blocked = 0;
    for (uint32_t i = 0; i < 300; i++) {
        uint32_t t = 400 + i * 100;
        ap_kph(&s, (float)(i % 6), t);
        ap_das(&s, 0x06, ((i / 5) & 1u) ? 0xE1 : 0xE0, t + 10);
        if (s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(blocked == 0, "re-engage crawl in 6, autoparkReady toggling: %d blocked frames", blocked);

    // Entry boundary: 8.0 km/h still a standstill start, 8.08 is moving.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 8.0f, 100); ap_das(&s, 0x02, 0xE0, 150); ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_tx_block, "2->6 at 8.0 km/h (<= ENTRY_MAX): blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 8.08f, 100); ap_das(&s, 0x02, 0xE0, 150); ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.autopark_tx_block, "2->6 at 8.08 km/h (> ENTRY_MAX): no episode");

    // 2->6 at 0 km/h, no maneuver bit (#176 stop-and-go): blocked; 12 km/h keeps
    // it; > 12 ends it; 5 km/h in the same 6 stays released; a maneuver bit
    // re-opens it and that episode needs > 20; leaving 6 resets.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100); ap_das(&s, 0x02, 0xE0, 150); ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_episode && s.autopark_tx_block, "2->6 at 0 km/h: blocked");
    CHECK(!s.autopark_evidence, "2->6 at 0 km/h, no maneuver bit: no evidence");
    ap_kph(&s, 12.0f, 300);
    CHECK(s.autopark_tx_block, "no maneuver bit: 12 km/h keeps the block");
    ap_kph(&s, 12.5f, 400);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "no maneuver bit: > 12 km/h ends the episode");
    ap_kph(&s, 5.0f, 500); ap_das(&s, 0x06, 0xE1, 550);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s),
          "back to 5 km/h + autoparkReady in the same 6: not re-blocked");
    ap_das(&s, 0x06, 0xE5, 600);
    CHECK(s.autopark_episode && s.autopark_tx_block, "waitingForBrake mid-6 after release: blocked");
    ap_das(&s, 0x06, 0xE0, 700);
    CHECK(s.autopark_tx_block, "maneuver episode holds after the bit clears (still slow)");
    ap_kph(&s, 15.0f, 750);
    CHECK(s.autopark_tx_block && s.autopark_evidence, "maneuver evidence: 15 km/h keeps the block");
    ap_kph(&s, 20.0f, 760);
    CHECK(s.autopark_tx_block, "maneuver evidence: 20 km/h keeps the block");
    ap_kph(&s, 25.0f, 800);
    CHECK(!s.autopark_tx_block, "> 20 km/h ends the maneuver episode too");
    ap_kph(&s, 3.0f, 900); ap_das(&s, 0x06, 0xE2, 950);
    CHECK(s.autopark_tx_block, "autoParked mid-6 after release: blocked");
    ap_das(&s, 0x02, 0xE0, 1000);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "leaving 6 ends the episode");
    ap_kph(&s, 0.0f, 1100); ap_das(&s, 0x06, 0xE0, 1150);
    CHECK(s.autopark_tx_block, "new 2->6 at a standstill: blocked (fresh state-6 period)");

    // Unknown / stale / SNA speed at a 1->6 entry: fail safe, blocked.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_das(&s, 0x01, 0xE0, 100); ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.speed_seen && s.autopark_tx_block, "1->6, speed never seen: blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 30.0f, 100); ap_das(&s, 0x01, 0xE0, 1500); ap_das(&s, 0x06, 0xE0, 2000);
    CHECK(s.autopark_tx_block, "1->6, stale 30 km/h: blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_speed(&s, 0xF0, 0xFF, 100); ap_das(&s, 0x01, 0xE0, 150); ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_tx_block, "1->6, SNA speed: blocked");
    ap_speed(&s, 0xF0, 0xFF, 300);
    CHECK(s.autopark_tx_block, "no evidence, fresh SNA: still blocked");
    ap_kph(&s, 30.0f, 400);
    CHECK(!s.autopark_tx_block, "no evidence, fresh 30 km/h: released");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100); ap_das(&s, 0x02, 0xE0, 150); ap_das(&s, 0x06, 0xE0, 200);
    s.vehicle_speed_kph = 30.0f; s.last_speed_tick_ms = 300;   // 1.2 s old below
    ap_das(&s, 0x06, 0xE0, 1500);
    CHECK(s.autopark_tx_block, "no evidence, stale 30 km/h: still blocked");

    // #176 stop-and-go: blocked only until the first fresh reading over 12 km/h.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100); ap_das(&s, 0x02, 0xE0, 150); ap_das(&s, 0x06, 0xE0, 200);
    blocked = 0;
    for (uint32_t i = 0; i < 200; i++) {
        uint32_t t = 300 + i * 100;
        ap_kph(&s, (float)(i % 11), t);
        ap_das(&s, 0x06, 0xE0, t + 10);
        if (s.autopark_tx_block) blocked++;
    }
    CHECK(blocked == 200, "stop-and-go below 12 km/h: blocked %d/200", blocked);
    ap_kph(&s, 15.0f, 20400);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "stop-and-go: 15 km/h releases");
    blocked = 0;
    for (uint32_t i = 0; i < 200; i++) {
        uint32_t t = 20500 + i * 100;
        ap_kph(&s, (float)(i % 11), t);
        ap_das(&s, 0x06, ((i / 7) & 1u) ? 0xE1 : 0xE0, t + 10);
        if (s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(blocked == 0, "stop-and-go after release, same 6: %d blocked frames", blocked);

    // An episode opened by its maneuver bit (#180 entry, byte3 0xE5) has
    // evidence from the start: 15 km/h keeps the block, > 20 ends it.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100); ap_frame(&s, k_das_parked, 150); ap_frame(&s, k_das_autopark, 200);
    CHECK(s.autopark_tx_block && s.autopark_evidence, "1->6 with waitingForBrake: evidence");
    ap_kph(&s, 15.0f, 300); ap_frame(&s, k_das_autopark, 310);
    CHECK(s.autopark_tx_block, "maneuver-opened episode: 15 km/h keeps the block");
    ap_kph(&s, 21.0f, 400);
    CHECK(!s.autopark_tx_block, "maneuver-opened episode: > 20 km/h releases");

    // Real in-car Autopark (#180 trace): parked, autoparkReady rises, ~1.1 s
    // later 1->6 on 06 0A DF E5 B0 08 21 4B at 0..5 km/h held 20 s, then 6->1.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    uint8_t ready[8];
    memcpy(ready, k_das_parked, 8);
    ready[3] = 0xE1;
    uint32_t t = 0;
    for (int i = 0; i < 10; i++, t += 100) { ap_kph(&s, 0.0f, t); ap_frame(&s, k_das_parked, t + 10); }
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "real trace parked: no block");
    for (int i = 0; i < 11; i++, t += 100) { ap_kph(&s, 0.0f, t); ap_frame(&s, ready, t + 10); }
    CHECK(s.das_ap_state == 1 && s.autopark_ready && !s.autopark_tx_block,
          "real trace autoparkReady at state 1: no episode yet");
    blocked = 0;
    for (int i = 0; i < 200; i++, t += 100) {
        ap_kph(&s, (float)(i % 6), t);
        ap_frame(&s, k_das_autopark, t + 10);
        if (s.autopark_tx_block && !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(blocked == 200, "real Autopark held 20 s at 0..5 km/h: blocked %d/200", blocked);
    ap_kph(&s, 0.0f, t); ap_frame(&s, k_das_parked, t + 10);
    CHECK(!s.autopark_episode && !s.autopark_tx_block && fsd_can_transmit(&s),
          "real trace 6->1: released");
}

// ── Signal Map hardening (#100): mask-0 ignored + configured-but-absent flag ──
// ── 0x33A car SOC beside 0x292 through the ESP32 handlers (#251) ─────────────
// One #211 Model Y pair, same capture: 0x292 socUI 81.8 %, 0x33A 80 % (bit 27 set).
static void test_ui_soc(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CanFrame f;
    memset(&f, 0, sizeof(f));
    const uint8_t soc292[8] = {0x3D, 0xCB, 0x5C, 0xB4, 0xD0, 0x01, 0x00, 0x00};
    const uint8_t ui33a[8]  = {0xD2, 0x34, 0x04, 0x7D, 0xFE, 0xFF, 0x13, 0x63};
    f.id = CAN_ID_BMS_SOC; f.dlc = 8; memcpy(f.data, soc292, 8);
    fsd_handle_bms_soc(&s, &f);
    f.id = CAN_ID_UI_RANGE; memcpy(f.data, ui33a, 8);
    fsd_handle_ui_range(&s, &f);
    CHECK(s.soc_percent > 81.75f && s.soc_percent < 81.85f, "0x292 81.8 %% (got %.2f)",
          (double)s.soc_percent);
    CHECK(s.ui_soc_seen && s.ui_soc == 80, "0x33A 80 %% (got %u)", (unsigned)s.ui_soc);
    f.data[2] = 0xF0; f.data[3] = 0x07;                            // 127: refused
    fsd_handle_ui_range(&s, &f);
    CHECK(s.ui_soc == 80, "0x33A above 100 refused, last value kept");
    f.id = CAN_ID_BMS_SOC; memset(f.data, 0, 8); f.data[1] = 0xFC; f.data[2] = 0x0F;
    fsd_handle_bms_soc(&s, &f);                                    // socUI 102.3: refused
    CHECK(s.soc_percent > 81.75f && s.soc_percent < 81.85f, "0x292 above 100 refused, last value kept");
}

// ── 0x312 pack temperature through the ESP32 handler (fsd_bms.h) ─────────────
static void bms312_feed(FSDState* s, uint8_t b4, uint8_t b5, uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_BMS_THERMAL; f.dlc = 8;
    f.data[4] = b4; f.data[5] = b5;
    fsd_handle_bms_thermal(s, &f, now);
}
static void test_bms_thermal(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    bms312_feed(&s, 0xF6, 0x30, 1000u);                         // 206~8 (shown as -50~8)
    CHECK(s.bms_seen && !s.batt_temp_ok && s.batt_temp_rejected == 1, "206~8 C rejected");
    bms312_feed(&s, 0x40, 0x8D, 1100u);                         // #211 dump 24~101
    CHECK(!s.batt_temp_ok && s.batt_temp_rejected == 2, "24~101 C rejected");
    bms312_feed(&s, 0x3A, 0x3D, 1200u);                         // 18~21
    CHECK(fsd_bms_temp_fresh(&s, 1200u) && s.batt_temp_min_c == 18 && s.batt_temp_max_c == 21,
          "18~21 C stored (%d~%d)", s.batt_temp_min_c, s.batt_temp_max_c);
    bms312_feed(&s, 0xF5, 0x08, 1300u);                         // 205~-32 (shown as -51~-32)
    CHECK(s.batt_temp_min_c == 18 && s.batt_temp_ms == 1200u && s.batt_temp_rejected == 3,
          "205~-32 C rejected, last plausible kept");
    CHECK(!fsd_bms_temp_fresh(&s, 1201u + FSD_BMS_TEMP_STALE_MS), "stale after FSD_BMS_TEMP_STALE_MS");
}

// ── 0x129 steering angle: decode, Soft Engage, EPAS-faithful direction ───────
// Same #176 / #211 frames as test_steering_angle in test_fsd_core.c, through
// the ESP32 handlers.
static const uint8_t k_steer_hw3_m1_2[8]  = {0xDA, 0x25, 0xF4, 0x5F, 0x00, 0x20, 0xFF, 0x3F};
static const uint8_t k_steer_hw3_p40_9[8] = {0x63, 0x2B, 0x99, 0x61, 0x0C, 0x20, 0xFF, 0x3F};
static const uint8_t k_steer_my_p21_6[8]  = {0x0E, 0x27, 0xD8, 0x60, 0xFF, 0x1F, 0xFF, 0x3F};
static const uint8_t k_steer_my_m5_9[8]   = {0xC9, 0x21, 0xC5, 0x5F, 0xFF, 0x1F, 0xFF, 0x3F};
static const uint8_t k_steer_my_parked[8] = {0xC4, 0x27, 0x09, 0x12, 0xFF, 0x1F, 0xFF, 0x3F};
static void steer_feed(FSDState* s, const uint8_t d[8], uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_STEER_ANGLE; f.dlc = 8;
    memcpy(f.data, d, 8);
    fsd_handle_steering_angle(s, &f, now);
}
static int modec_raw(const CanFrame* out) {
    return ((out->data[2] & 0x0F) << 8) | out->data[3];
}
static void test_steering_angle(void) {
    FSDState s;
    const struct { const uint8_t* d; bool valid; float deg; const char* where; } v[] = {
        {k_steer_hw3_m1_2, true, -1.2f, "#176 -1.2"},
        {k_steer_hw3_p40_9, true, 40.9f, "#176 +40.9"},
        {k_steer_my_p21_6, true, 21.6f, "#211 +21.6"},
        {k_steer_my_m5_9, true, -5.9f, "#211 -5.9"},
        {k_steer_my_parked, false, 0.0f, "#211 parked INVALID"},
    };
    for (size_t i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
        memset(&s, 0, sizeof(s));
        steer_feed(&s, v[i].d, 1000u);
        CHECK(s.steering_angle_seen && s.steering_angle_valid == v[i].valid, "%s: seen/valid",
              v[i].where);
        if (v[i].valid)
            CHECK(s.steering_angle_deg > v[i].deg - 0.05f && s.steering_angle_deg < v[i].deg + 0.05f,
                  "%s: got %.2f", v[i].where, (double)s.steering_angle_deg);
    }

    // Soft Engage: no 0x129 ever -> AP-First-only; INVALID / stale -> hold; a
    // fresh centred angle latches; a turn holds.
    memset(&s, 0, sizeof(s));
    s.soft_engage = true;
    CHECK(fsd_soft_engage_allows(&s, 100u) && s.soft_engage_latched, "soft: no 0x129 -> latch");
    memset(&s, 0, sizeof(s));
    s.soft_engage = true;
    steer_feed(&s, k_steer_my_parked, 1000u);
    CHECK(!fsd_soft_engage_allows(&s, 1000u) && !s.soft_engage_latched, "soft: INVALID -> hold");
    steer_feed(&s, k_steer_hw3_p40_9, 1010u);
    CHECK(!fsd_soft_engage_allows(&s, 1010u) && !s.soft_engage_latched, "soft: +40.9 -> hold");
    steer_feed(&s, k_steer_hw3_m1_2, 1020u);
    CHECK(!fsd_soft_engage_allows(&s, 1020u + FSD_STEER_FRESH_MS + 1u), "soft: stale centred -> hold");
    CHECK(fsd_soft_engage_allows(&s, 1020u) && s.soft_engage_latched, "soft: -1.2 fresh -> latch");

    // EPAS-faithful direction: opposite the wheel, held near centre and while
    // unknown. Strong demand (3) after its 1 s pause, phase past the ramp.
    memset(&s, 0, sizeof(s));
    s.nag_killer = true; s.nag_epas_faithful = true; s.das_ap_state = 3;
    CanFrame in, out;
    memset(&in, 0, sizeof(in));
    in.id = CAN_ID_EPAS_STATUS; in.dlc = 8;
    uint32_t t = 300000u;
    s.das_hands_on_state = 0; fsd_handle_nag_killer(&s, &in, &out, t);
    s.das_hands_on_state = 3; fsd_handle_nag_killer(&s, &in, &out, t);
    memset(&out, 0, sizeof(out));
    CHECK(fsd_handle_nag_killer(&s, &in, &out, t + 1600u) && modec_raw(&out) > 2048,
          "faithful dir: no angle -> + (raw %d)", modec_raw(&out));
    steer_feed(&s, k_steer_my_p21_6, t + 1700u);
    memset(&out, 0, sizeof(out));
    CHECK(fsd_handle_nag_killer(&s, &in, &out, t + 1700u) && modec_raw(&out) < 2048,
          "faithful dir: +21.6 -> - (raw %d)", modec_raw(&out));
    steer_feed(&s, k_steer_hw3_m1_2, t + 1800u);                 // inside the hold band
    memset(&out, 0, sizeof(out));
    CHECK(fsd_handle_nag_killer(&s, &in, &out, t + 1800u) && modec_raw(&out) < 2048,
          "faithful dir: -1.2 holds - (raw %d)", modec_raw(&out));
    steer_feed(&s, k_steer_my_m5_9, t + 1900u);
    memset(&out, 0, sizeof(out));
    CHECK(fsd_handle_nag_killer(&s, &in, &out, t + 1900u) && modec_raw(&out) > 2048,
          "faithful dir: -5.9 -> + (raw %d)", modec_raw(&out));
    steer_feed(&s, k_steer_my_parked, t + 2000u);                // INVALID -> unknown
    memset(&out, 0, sizeof(out));
    CHECK(fsd_handle_nag_killer(&s, &in, &out, t + 2000u) && modec_raw(&out) > 2048,
          "faithful dir: INVALID holds + (raw %d)", modec_raw(&out));
}

static void test_signal_map(void) {
    FSDState s;

    // Mask 0 means "not mapped": the field is ignored, not forced to 0.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW4;
    s.das_ap_state = 5; s.das_hands_on_state = 3; s.ap_active = true;
    s.cfg_das_id = 0x39B;
    s.cfg_apstate_byte = 0; s.cfg_apstate_shift = 0; s.cfg_apstate_mask = 0x00;
    s.cfg_handson_byte = 5; s.cfg_handson_shift = 2; s.cfg_handson_mask = 0x00;
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x39B; f.dlc = 8; f.data[0] = 0x02; f.data[5] = (uint8_t)(1u << 2);
    fsd_apply_signal_config(&s, &f, 5000u);
    CHECK(s.das_ap_state == 5, "mask-0 apstate ignored (kept 5, got %u)", s.das_ap_state);
    CHECK(s.das_hands_on_state == 3, "mask-0 hands-on ignored (kept 3, got %u)", s.das_hands_on_state);
    CHECK(s.das_ctx_seen_ms == 5000u, "mask-0 still stamps freshness");
    // A real mask maps normally + syncs ap_active via the engaged helper.
    s.cfg_apstate_mask = 0x0F;
    f.data[0] = 0x06;
    fsd_apply_signal_config(&s, &f, 5100u);
    CHECK(s.das_ap_state == 6 && s.ap_active, "mapped mask reads byte0 + ap_active engaged");

    // Configured-but-absent DAS id raises the flag after the timeout, clears on sight.
    memset(&s, 0, sizeof(s));
    s.cfg_das_id = 0x39B;
    CHECK(!fsd_signal_map_das_missing(&s, 1000u), "within boot grace -> not missing");
    CHECK(fsd_signal_map_das_missing(&s, 4000u), "never seen after timeout -> missing");
    s.das_ctx_seen_ms = 4000u;
    CHECK(!fsd_signal_map_das_missing(&s, 4500u), "id shows up -> clears");
    CHECK(fsd_signal_map_das_missing(&s, 8000u), "stale again -> missing");
    s.cfg_das_id = 0;
    CHECK(!fsd_signal_map_das_missing(&s, 8000u), "auto mode -> never missing");

    // Steering id 0x129 is read with its own layout whatever hi/lo say: bytes
    // 0-1 are CRC + counter, which the old hi=1/lo=0 default read as the angle
    // (0x25DA = 969.0 deg for the #176 -1.2 frame).
    memset(&s, 0, sizeof(s));
    s.cfg_steer_id = 0x129; s.cfg_steer_hi = 1; s.cfg_steer_lo = 0;
    memset(&f, 0, sizeof(f));
    f.id = 0x129; f.dlc = 8;
    memcpy(f.data, k_steer_hw3_m1_2, 8);
    fsd_apply_signal_config(&s, &f, 6000u);
    CHECK(s.steering_angle_valid && s.steering_angle_deg > -1.25f && s.steering_angle_deg < -1.15f,
          "sigmap 0x129: DBC angle -1.2 (got %.1f)", (double)s.steering_angle_deg);
    CHECK(fsd_steer_known(&s, 6000u) && s.steer_ctx_seen_ms == 6000u, "sigmap 0x129: known, stamped");
    memcpy(f.data, k_steer_my_parked, 8);
    fsd_apply_signal_config(&s, &f, 6100u);
    CHECK(s.steering_angle_seen && !s.steering_angle_valid && !fsd_steer_known(&s, 6100u),
          "sigmap 0x129: INVALID -> angle unknown");
    f.dlc = 3;
    memcpy(f.data, k_steer_hw3_m1_2, 8);
    fsd_apply_signal_config(&s, &f, 6200u);
    CHECK(!s.steering_angle_valid && s.steer_ctx_seen_ms == 6100u, "sigmap 0x129: DLC 3 ignored");

    // Any other id: signed 16-bit at hi/lo, 0.1 deg per bit, as documented.
    memset(&s, 0, sizeof(s));
    s.cfg_steer_id = 0x123; s.cfg_steer_hi = 1; s.cfg_steer_lo = 0;
    memset(&f, 0, sizeof(f));
    f.id = 0x123; f.dlc = 4; f.data[0] = 0x2C; f.data[1] = 0x01;   // 300 -> 30.0
    fsd_apply_signal_config(&s, &f, 7000u);
    CHECK(s.steering_angle_valid && s.steering_angle_deg > 29.95f && s.steering_angle_deg < 30.05f,
          "sigmap custom id: 30.0 deg (got %.1f)", (double)s.steering_angle_deg);
    f.data[0] = 0xD4; f.data[1] = 0xFE;                              // -300 -> -30.0
    fsd_apply_signal_config(&s, &f, 7100u);
    CHECK(s.steering_angle_deg < -29.95f && s.steering_angle_deg > -30.05f,
          "sigmap custom id: -30.0 deg (got %.1f)", (double)s.steering_angle_deg);
    // hi == lo (the default, 0/0) maps nothing.
    memset(&s, 0, sizeof(s));
    s.cfg_steer_id = 0x123;
    fsd_apply_signal_config(&s, &f, 7200u);
    CHECK(!s.steering_angle_seen && s.steer_ctx_seen_ms == 0u, "sigmap custom id, hi == lo: not mapped");
}

// ── DI_speed decode parity with the Flipper (0x257) ───────────────────────────
static void test_di_speed(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x257u; f.dlc = 8;   // DI_speed (the handler ignores the id)
    f.data[1] = 0x10; f.data[2] = 0x27; f.data[3] = 0x42;
    fsd_handle_di_speed(&s, &f);   // raw = (0x27<<4)|(0x10>>4) = 625 -> 625*0.08-40 = 10.0
    CHECK(s.vehicle_speed_kph > 9.99f && s.vehicle_speed_kph < 10.01f,
          "di_speed got %.3f exp 10.0", s.vehicle_speed_kph);
    CHECK(s.ui_speed == 0x42 && s.speed_seen, "di_speed ui_speed + speed_seen");
}

// Performance read-out vs DI_vehicleSpeed SNA: raw 4095 decodes to 287.6 km/h and
// used to lock every acceleration timer and spike the est. G. Fed the way the
// main.cpp 0x257 path does: parse, stamp, fsd_perf_on_speed().
static void perf_speed(FSDState* s, uint16_t raw, uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x257u; f.dlc = 8;
    f.data[1] = (uint8_t)((raw & 0x0Fu) << 4); f.data[2] = (uint8_t)(raw >> 4);
    fsd_handle_di_speed(s, &f);
    s->last_speed_tick_ms = now;
    fsd_perf_on_speed(s, now);
}
static void test_perf_speed_sna(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    perf_speed(&s, 500, 0);                // 0 km/h
    perf_speed(&s, 500, 100);
    perf_speed(&s, 4095, 200);             // SNA right at standstill
    CHECK(!s.perf.accel_armed && !s.perf.lock_0_50 && !s.perf.lock_0_100,
          "SNA at standstill: no run, no lock");
    CHECK(s.perf.g_peak < 0.1f, "SNA: no G spike (peak %.2f)", (double)s.perf.g_peak);
    CHECK(!fsd_speed_valid(&s, 200), "SNA: speed not valid for the card");
    perf_speed(&s, 563, 300);              // 5.04 km/h -> armed from the 100 ms sample
    CHECK(s.perf.accel_armed && s.perf.accel_start_ms == 100, "valid 5 km/h arms the run");
    perf_speed(&s, 4095, 400);             // SNA mid-run
    perf_speed(&s, 4063, 500);             // above max valid
    CHECK(!s.perf.lock_0_50 && s.perf.best_0_50_ms == 0, "SNA / 4063 mid-run: no lock, no best");
    perf_speed(&s, 1188, 4100);            // 55.04 km/h -> 0-50 locks at 4000 ms
    CHECK(s.perf.lock_0_50 && s.perf.t_0_50_ms == 4000u, "valid 55 km/h locks 0-50 (%lu ms)",
          (unsigned long)s.perf.t_0_50_ms);
    CHECK(fsd_speed_valid(&s, 4100) && !fsd_speed_valid(&s, 4100 + FSD_AUTOPARK_SPEED_FRESH_MS + 1),
          "card speed valid while fresh, not once stale");
}

// ── state init ────────────────────────────────────────────────────────────────
static void test_state_init(void) {
    FSDState s;
    memset(&s, 0xA5, sizeof(s)); // init must not rely on a zeroed struct
    fsd_state_init(&s, TeslaHW_Unknown);
    CHECK(!s.tesla_ota_in_progress && !s.ota_last_valid && s.ota_last_byte6 == 0 &&
              s.ota_assert_count == 0 && s.ota_clear_count == 0,
          "init clears OTA detection state");
    CHECK(s.op_mode == OpMode_ListenOnly && !fsd_can_transmit(&s), "init: listen-only, TX blocked");
    CHECK(!s.hw3_speed_override, "init: hw3_speed_override default OFF (#209)");
    CHECK(s.accel_mode == ACCEL_MODE_OFF && s.accel_mode_applied == ACCEL_MODE_OFF &&
              !s.accel_car_seen,
          "init: Acceleration Mode default Off (#211)");
}

// ── 0x3FD mux2 HW4 speed profile layout (#59), parity with the Flipper core ───
// Real HW4 mux2 payload: byte7 = 0x90 (bit 63 = mux2 valid, bits 60-62 = profile 1).
static void test_hw4_mux2_profile_layout(void) {
    static const uint8_t k_hw4_mux2[8] = {0x02, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90};
    for (int p = 0; p <= 4; p++) {
        FSDState s;
        memset(&s, 0, sizeof(s));
        s.hw_version = TeslaHW_HW4;
        s.fsd_unlock = true;
        s.speed_profile = p;
        CanFrame f;
        memset(&f, 0, sizeof(f));
        f.id = 0x3FD;
        f.dlc = 8;
        memcpy(f.data, k_hw4_mux2, 8);
        CHECK(fsd_handle_autopilot_frame(&s, &f), "#59 p%d: HW4 mux2 modified", p);
        CHECK(((f.data[7] >> 4) & 0x07) == p, "#59 p%d: profile in bits 60-62, got %u", p,
              (f.data[7] >> 4) & 0x07);
        CHECK((f.data[7] & 0x80) != 0, "#59 p%d: bit63 (mux2 valid) kept, byte7=0x%02X", p,
              f.data[7]);
        CHECK((f.data[7] & 0x0F) == 0x00, "#59 p%d: byte7 low nibble untouched", p);
        CHECK(memcmp(f.data, k_hw4_mux2, 7) == 0, "#59 p%d: bytes 0-6 untouched", p);
    }
}

// ── 0x3FD HW3 speed fields (#209), parity with the Flipper core ───────────────
// Default is pass-through: with FSD unlock, mux0 gets bit46 only; the car's own
// FSD profile (mux0 bits 49-50) and the whole mux2 frame (FSD max-speed offset,
// bits 6-13) are left as received, and mux2 is not re-sent. hw3_speed_override
// restores the legacy write byte-for-byte.
static const uint8_t k_hw3_mux0[8] = {0x00, 0x11, 0x22, 0x46, 0x00, 0x00, 0x83, 0x80};
static const uint8_t k_hw3_mux2[8] = {0x82, 0xCB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};

static void ap3fd(CanFrame* f, const uint8_t b[8]) {
    memset(f, 0, sizeof(*f));
    f->id = CAN_ID_AP_CONTROL;
    f->dlc = 8;
    memcpy(f->data, b, 8);
}

static void hw3_state(FSDState* s, bool override_on) {
    memset(s, 0, sizeof(*s));
    s->hw_version = TeslaHW_HW3;
    s->fsd_unlock = true;
    s->force_fsd = true;
    s->nag_killer = true;          // mux1 path on, must not leak into mux2
    s->speed_profile = 2;
    s->hw3_speed_override = override_on;
}

static void test_hw3_speed_passthrough(void) {
    FSDState s;
    CanFrame f;

    // ── default: pass-through ──
    hw3_state(&s, false);
    ap3fd(&f, k_hw3_mux0);
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW3 mux0 modified (bit46)");
    static const uint8_t k_mux0_bit46[8] = {0x00, 0x11, 0x22, 0x46, 0x00, 0x40, 0x83, 0x80};
    CHECK(memcmp(f.data, k_mux0_bit46, 8) == 0,
          "HW3 default: mux0 = car frame + bit46 only (byte6 0x%02X)", f.data[6]);
    CHECK(s.speed_offset == 25, "HW3 AP offset still read: %d exp 25", s.speed_offset);
    CHECK(s.hw3_profile_seen && s.hw3_car_profile == 1 && s.hw3_sent_profile == 1,
          "HW3 default read-out: profile 1 -> 1");

    ap3fd(&f, k_hw3_mux2);
    CHECK(!fsd_handle_autopilot_frame(&s, &f), "HW3 default: mux2 NOT modified");
    CHECK(memcmp(f.data, k_hw3_mux2, 8) == 0, "HW3 default: mux2 payload untouched");
    CHECK(s.hw3_offset_seen && s.hw3_car_offset == 46 && s.hw3_sent_offset == 46,
          "HW3 default read-out: offset 46 -> 46 (got %u -> %u)", s.hw3_car_offset,
          s.hw3_sent_offset);

    // FSD unlock off: nothing on mux0/mux2, read-out still tracks the car.
    hw3_state(&s, true);
    s.fsd_unlock = false;
    ap3fd(&f, k_hw3_mux0);
    CHECK(!fsd_handle_autopilot_frame(&s, &f), "HW3 unlock off: mux0 not modified");
    CHECK(s.hw3_profile_seen && s.hw3_sent_profile == 1, "HW3 unlock off: read-out tracks");

    // ── hw3_speed_override: legacy bytes ──
    hw3_state(&s, true);
    CanFrame z;
    memset(&z, 0, sizeof(z));
    z.dlc = 8;                     // old test inputs: zero mux0, profile 2
    CHECK(fsd_handle_autopilot_frame(&s, &z), "HW3 override zero mux0 modified");
    static const uint8_t k_zero_legacy[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x04, 0x00};
    CHECK(memcmp(z.data, k_zero_legacy, 8) == 0, "HW3 override zero mux0 = legacy bytes");

    ap3fd(&f, k_hw3_mux0);
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW3 override mux0 modified");
    static const uint8_t k_mux0_legacy[8] = {0x00, 0x11, 0x22, 0x46, 0x00, 0x40, 0x85, 0x80};
    CHECK(memcmp(f.data, k_mux0_legacy, 8) == 0,
          "HW3 override mux0 = legacy bytes (byte6 0x%02X exp 0x85)", f.data[6]);
    CHECK(s.hw3_car_profile == 1 && s.hw3_sent_profile == 2, "HW3 override read-out: 1 -> 2");

    ap3fd(&f, k_hw3_mux2);
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW3 override mux2 modified");
    static const uint8_t k_mux2_legacy[8] = {0x42, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
    CHECK(memcmp(f.data, k_mux2_legacy, 8) == 0,
          "HW3 override mux2 = legacy bytes (got %02X %02X)", f.data[0], f.data[1]);
    CHECK(s.hw3_car_offset == 46 && s.hw3_sent_offset == 25, "HW3 override read-out: 46 -> 25");

    // Follow distance: read-out always, profile write only with the override.
    CanFrame fd;
    memset(&fd, 0, sizeof(fd));
    fd.id = CAN_ID_FOLLOW_DIST;
    fd.dlc = 8;
    fd.data[5] = (uint8_t)(3u << 5);   // fd3 -> profile 0
    fsd_handle_follow_distance(&s, &fd);
    CHECK(s.follow_distance_seen && s.follow_distance == 3, "fd read-out 3 got %u",
          s.follow_distance);
    ap3fd(&f, k_hw3_mux0);
    fsd_handle_autopilot_frame(&s, &f);
    CHECK(((f.data[6] >> 1) & 0x03) == 0, "HW3 override: fd3 -> profile 0 written");
    s.hw3_speed_override = false;
    ap3fd(&f, k_hw3_mux0);
    fsd_handle_autopilot_frame(&s, &f);
    CHECK(((f.data[6] >> 1) & 0x03) == 1, "HW3 default: fd3 ignored, car profile 1 kept");
}

// ── #209 regression guard: hw3_speed_override never touches HW4 ──────────────
static void test_hw4_ignores_hw3_override(void) {
    static const uint8_t k_hw4_mux0[8] = {0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x83, 0x80};
    static const uint8_t k_hw4_mux2[8] = {0x02, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90};
    static const uint8_t k_hw4_mux0_out[8] = {0x00, 0x00, 0x00, 0x46, 0x00, 0x40, 0x83, 0x90};
    static const uint8_t k_hw4_mux2_out[8] = {0x02, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0};
    for (int ov = 0; ov <= 1; ov++) {
        FSDState s;
        memset(&s, 0, sizeof(s));
        s.hw_version = TeslaHW_HW4;
        s.fsd_unlock = true;
        s.force_fsd = true;
        s.speed_profile = 4;
        s.hw3_speed_override = (ov == 1);
        CanFrame f;
        ap3fd(&f, k_hw4_mux0);
        CHECK(fsd_handle_autopilot_frame(&s, &f), "HW4 ov%d mux0 modified", ov);
        CHECK(memcmp(f.data, k_hw4_mux0_out, 8) == 0, "HW4 ov%d mux0 = bit46+bit60 only", ov);
        ap3fd(&f, k_hw4_mux2);
        CHECK(fsd_handle_autopilot_frame(&s, &f), "HW4 ov%d mux2 modified", ov);
        CHECK(memcmp(f.data, k_hw4_mux2_out, 8) == 0, "HW4 ov%d mux2 = profile 4 in bits 60-62",
              ov);
        CHECK(!s.hw3_profile_seen && !s.hw3_offset_seen, "HW4 ov%d: HW3 read-out untouched", ov);
    }
}

// ── driver RX/TX frame filter ─────────────────────────────────────────────────
// The TWAI driver hands up DLC 9..15 unchanged (twai_message_t.data is 8 bytes)
// plus extended and remote frames. receive() drops DLC > 8; process_frame()
// records extended / remote frames to captures but tesla_can_rx_accept() keeps
// them away from every parser and handler.
static void test_can_frame_filter(void) {
    // Standard data frames the handlers use are accepted, DLC 0..8.
    CHECK(tesla_can_rx_accept(0x3FDu, false, false, 8), "0x3FD dlc8 accepted");
    CHECK(tesla_can_rx_accept(0x000u, false, false, 0), "id0 dlc0 accepted");
    CHECK(tesla_can_rx_accept(0x7FFu, false, false, 8), "max std id accepted");
    // DLC 9..15 is "8 bytes" on the wire but would overflow data[8] if copied.
    for (uint8_t dlc = 9; dlc <= 15; dlc++)
        CHECK(!tesla_can_rx_accept(0x3FDu, false, false, dlc), "dlc %u rejected", dlc);
    // A 29-bit frame whose id masks down to a target id must not alias it.
    CHECK(!tesla_can_rx_accept(0x3FDu, true, false, 8), "extended 0x3FD rejected");
    CHECK(!tesla_can_rx_accept(0x18DB33F1u, true, false, 8), "extended OBD id rejected");
    // A remote frame carries no payload; a modified copy would be garbage.
    CHECK(!tesla_can_rx_accept(0x370u, false, true, 8), "remote 0x370 rejected");
    CHECK(!tesla_can_rx_accept(0x800u, false, false, 8), "id > 0x7FF rejected");

    CHECK(tesla_can_tx_valid(0x3FDu, 8), "tx 0x3FD dlc8 valid");
    CHECK(tesla_can_tx_valid(0x082u, 0), "tx dlc0 valid");
    CHECK(!tesla_can_tx_valid(0x3FDu, 9), "tx dlc9 invalid");
    CHECK(!tesla_can_tx_valid(0x800u, 8), "tx id 0x800 invalid");
}

// ── Acceleration Mode override: 0x334 UI_powertrainControl (#211) ─────────────
// Real 0x334 frames (pedal map 1 = SPORT/Standard), ban-analysis captures:
//   HW3 Model 3, hw3_unbanned_D-light-off.csv @820512
//   HW4 Model 3, hw4_unbanned_d-off.csv @1012328 and hw4_unbanned_p-on.csv @260483
static const uint8_t k_334_hw3[8]   = {0x3F, 0x3F, 0x14, 0x80, 0xFC, 0x07, 0xE0, 0x2C};
static const uint8_t k_334_hw4[8]   = {0x3F, 0x3F, 0x14, 0x80, 0xDC, 0x15, 0x24, 0x5E};
static const uint8_t k_334_hw4p[8]  = {0x3F, 0x3F, 0x14, 0x80, 0xFC, 0x87, 0xE2, 0xAE};
// Payload from the tesla-can-boost README (byte0 0xBF: bit7 set), counter and
// checksum filled in here — checks that bit7 survives the rewrite.
static const uint8_t k_334_bit7[8]  = {0xBF, 0x3F, 0x14, 0x80, 0xFC, 0x07, 0xE0, 0xAC};
// Real 0x257 DI_speed / 0x118 DI_systemStatus, hw4_unbanned_d-on.csv.
static const uint8_t k_257_stop[8]  = {0x74, 0x4A, 0x1F, 0x00, 0x02, 0xA0, 0x0F, 0x01}; // raw 500 =  0.00 kph
static const uint8_t k_257_0p72[8]  = {0xB5, 0xDF, 0x1F, 0x00, 0x66, 0xE8, 0x0F, 0x01}; // raw 509 =  0.72 kph
static const uint8_t k_257_1p60[8]  = {0x27, 0x8E, 0x20, 0x01, 0xD6, 0x38, 0x10, 0x01}; // raw 520 =  1.60 kph
static const uint8_t k_257_rev[8]   = {0xAF, 0x84, 0x1C, 0x03, 0x5E, 0x46, 0x0E, 0x01}; // raw 456 = -3.52 kph
static const uint8_t k_257_31[8]    = {0xC3, 0x4B, 0x37, 0x1F, 0xFE, 0xAF, 0x1B, 0x01}; // raw 884 = 30.72 kph
static const uint8_t k_257_sna[8]   = {0x00, 0xF0, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00}; // raw 4095 = SNA
static const uint8_t k_118_p[8]     = {0x30, 0x6D, 0x32, 0x30, 0x00, 0x48, 0x00, 0x00}; // DI_gear 1 P
static const uint8_t k_118_d[8]     = {0xF9, 0x6B, 0x95, 0x70, 0x00, 0x68, 0x08, 0x00}; // DI_gear 4 D

static void accel_state(FSDState* s, uint8_t mode) {
    memset(s, 0, sizeof(*s));
    s->op_mode = OpMode_Active;
    s->accel_mode = mode;
}

// main.cpp stamps last_speed_tick_ms after the 0x257 parse; mirror that here.
static void accel_speed(FSDState* s, const uint8_t* d, uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x257u; f.dlc = 8;
    memcpy(f.data, d, 8);
    fsd_handle_di_speed(s, &f);
    s->last_speed_tick_ms = now;
}

static bool accel_gear(FSDState* s, const uint8_t* d, uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_DI_SYS_STATUS; f.dlc = 8;
    memcpy(f.data, d, 8);
    return fsd_handle_di_gear(s, &f, now);
}

static bool accel_334(FSDState* s, CanFrame* f, const uint8_t* d, uint32_t now) {
    memset(f, 0, sizeof(*f));
    f->id = CAN_ID_UI_POWERTRAIN; f->dlc = 8;
    memcpy(f->data, d, 8);
    return fsd_handle_accel_mode(s, f, now);
}

static uint8_t pedal_map(const CanFrame* f) { return (f->data[0] >> 5) & 0x03; }

// Same frame with another car-side pedal map, checksum valid (touchscreen change).
static void accel_with_map(uint8_t out[8], const uint8_t in[8], uint8_t map) {
    memcpy(out, in, 8);
    out[0] = (uint8_t)((out[0] & 0x9F) | (map << 5));
    out[7] = tesla_additive_checksum(0x334u, out, 7);
}

static void test_accel_real_frames(void) {
    const uint8_t* real[] = {k_334_hw3, k_334_hw4, k_334_hw4p};
    for (int i = 0; i < 3; i++) {
        CHECK(tesla_additive_checksum(0x334u, real[i], 7) == real[i][7],
              "real 0x334 #%d: additive checksum matches byte7", i);
        CHECK(((real[i][0] >> 5) & 0x03) == 1, "real 0x334 #%d: UI_pedalMap = SPORT", i);
    }
    // Real 0x118 frames: checksum byte0 over bytes 1..7, DI_gear bits 21-23.
    FSDState s;
    memset(&s, 0, sizeof(s));
    CHECK(accel_gear(&s, k_118_p, 10) && s.di_gear == 1 && s.last_gear_tick_ms == 10,
          "real 0x118 P parsed, gear %u", s.di_gear);
    CHECK(accel_gear(&s, k_118_d, 20) && s.di_gear == 4, "real 0x118 D parsed, gear %u",
          s.di_gear);
    uint8_t bad[8];
    memcpy(bad, k_118_p, 8);
    bad[0] ^= 0x01;
    CHECK(!accel_gear(&s, bad, 30) && s.di_gear == 4 && s.last_gear_tick_ms == 20,
          "0x118 bad checksum ignored");
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.dlc = 7;
    memcpy(f.data, k_118_p, 8);
    CHECK(!fsd_handle_di_gear(&s, &f, 40), "0x118 dlc7 ignored");
    // Signed raw speed is kept: reverse decodes negative, the clamped kph is 0.
    accel_speed(&s, k_257_rev, 50);
    CHECK(s.di_speed_raw == 456 && s.vehicle_speed_kph == 0.0f, "0x257 reverse raw %u",
          s.di_speed_raw);
}

static void test_accel_encode(void) {
    // Expected rewrites of the real HW3 frame: only byte0 bits 5-6 and byte7 move.
    static const uint8_t exp_chill[8] = {0x1F, 0x3F, 0x14, 0x80, 0xFC, 0x07, 0xE0, 0x0C};
    static const uint8_t exp_perf[8]  = {0x5F, 0x3F, 0x14, 0x80, 0xFC, 0x07, 0xE0, 0x4C};
    FSDState s;
    CanFrame f;
    accel_state(&s, ACCEL_MODE_CHILL);
    accel_speed(&s, k_257_stop, 1000);
    CHECK(accel_334(&s, &f, k_334_hw3, 1000), "CHILL at standstill -> modified");
    CHECK(memcmp(f.data, exp_chill, 8) == 0 && pedal_map(&f) == 0, "CHILL encode");
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    accel_speed(&s, k_257_stop, 1000);
    CHECK(accel_334(&s, &f, k_334_hw3, 1000), "PERFORMANCE at standstill -> modified");
    CHECK(memcmp(f.data, exp_perf, 8) == 0 && pedal_map(&f) == 2, "PERFORMANCE encode");
    CHECK(s.accel_mode_applied == ACCEL_MODE_PERFORMANCE && !s.accel_mode_pending &&
              s.accel_frames_modified == 1,
          "PERFORMANCE latched");
    // SPORT on a car already in SPORT: latched, but the frame needs no rewrite.
    accel_state(&s, ACCEL_MODE_SPORT);
    accel_speed(&s, k_257_stop, 1000);
    CHECK(!accel_334(&s, &f, k_334_hw4, 1000) && memcmp(f.data, k_334_hw4, 8) == 0,
          "SPORT on a SPORT car -> untouched, not re-sent");
    CHECK(s.accel_mode_applied == ACCEL_MODE_SPORT && s.accel_car_map == 1, "SPORT latched");
    // Every mode on every real frame: map set, other bits / counter kept, checksum valid.
    const uint8_t* real[] = {k_334_hw3, k_334_hw4, k_334_hw4p, k_334_bit7};
    for (int i = 0; i < 4; i++) {
        for (uint8_t mode = ACCEL_MODE_CHILL; mode <= ACCEL_MODE_PERFORMANCE; mode++) {
            uint8_t car_map = (real[i][0] >> 5) & 0x03;
            accel_state(&s, mode);
            accel_speed(&s, k_257_stop, 1000);
            bool mod = accel_334(&s, &f, real[i], 1000);
            CHECK(mod == (mode - 1u != car_map), "frame %d mode %u: modified iff map differs", i,
                  mode);
            CHECK(pedal_map(&f) == mode - 1u, "frame %d mode %u: pedal map", i, mode);
            CHECK((f.data[0] & 0x9F) == (real[i][0] & 0x9F) &&
                      memcmp(&f.data[1], &real[i][1], 6) == 0,
                  "frame %d mode %u: other bits + counter kept", i, mode);
            CHECK(f.data[7] == tesla_additive_checksum(0x334u, f.data, 7),
                  "frame %d mode %u: checksum recomputed", i, mode);
        }
    }
    // tesla-can-boost README: Standard 0xBF -> Performance 0xDF, Chill 0x9F.
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    accel_speed(&s, k_257_stop, 1000);
    accel_334(&s, &f, k_334_bit7, 1000);
    CHECK(f.data[0] == 0xDF && f.data[7] == 0xCC, "bit7 frame -> PERFORMANCE 0xDF");
    accel_state(&s, ACCEL_MODE_CHILL);
    accel_speed(&s, k_257_stop, 1000);
    accel_334(&s, &f, k_334_bit7, 1000);
    CHECK(f.data[0] == 0x9F && f.data[7] == 0x8C, "bit7 frame -> CHILL 0x9F");
}

static void test_accel_off_passthrough(void) {
    FSDState s;
    CanFrame f;
    accel_state(&s, ACCEL_MODE_OFF);
    accel_speed(&s, k_257_stop, 1000);
    for (uint32_t t = 1000; t < 3000; t += 500) {
        CHECK(!accel_334(&s, &f, k_334_hw3, t), "Off -> not re-sent");
        CHECK(memcmp(f.data, k_334_hw3, 8) == 0, "Off -> frame untouched");
    }
    CHECK(s.accel_mode_applied == ACCEL_MODE_OFF && !s.accel_mode_pending &&
              s.accel_frames_modified == 0 && s.accel_car_seen && s.accel_car_map == 1,
          "Off: pass-through, car map still read");
    CHECK(fsd_accel_sent_map(&s) == 1, "Off: sent map = car map");
    // Out-of-range setting (corrupt NVS) behaves as Off.
    s.accel_mode = 7;
    CHECK(!accel_334(&s, &f, k_334_hw3, 3000) && memcmp(f.data, k_334_hw3, 8) == 0,
          "setting 7 -> pass-through");
}

static void test_accel_standstill_latch(void) {
    FSDState s;
    CanFrame f;
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    CHECK(fsd_accel_sent_map(&s) == -1, "no 0x334 yet -> sent map -1");
    // Moving: pending, nothing sent.
    accel_speed(&s, k_257_31, 1000);
    CHECK(!accel_334(&s, &f, k_334_hw3, 1000) && memcmp(f.data, k_334_hw3, 8) == 0,
          "moving -> untouched");
    CHECK(s.accel_mode_applied == ACCEL_MODE_OFF && s.accel_mode_pending,
          "moving -> pending, not applied");
    CHECK(fsd_accel_sent_map(&s) == 1, "pending -> sent map = car map");
    // Stop: applied.
    accel_speed(&s, k_257_stop, 1500);
    CHECK(accel_334(&s, &f, k_334_hw3, 1500) && pedal_map(&f) == 2, "stopped -> PERFORMANCE");
    CHECK(s.accel_mode_applied == ACCEL_MODE_PERFORMANCE && !s.accel_mode_pending,
          "stopped -> applied");
    CHECK(fsd_accel_sent_map(&s) == 2, "applied -> sent map 2");
    // Drive off: stays latched.
    accel_speed(&s, k_257_31, 2000);
    CHECK(accel_334(&s, &f, k_334_hw3, 2000) && pedal_map(&f) == 2, "moving -> still latched");
    // New setting while moving: old value keeps going out until the next stop.
    s.accel_mode = ACCEL_MODE_CHILL;
    CHECK(accel_334(&s, &f, k_334_hw3, 2000) && pedal_map(&f) == 2,
          "change while moving -> old value kept");
    CHECK(s.accel_mode_pending, "change while moving -> pending");
    accel_speed(&s, k_257_0p72, 2500);
    CHECK(accel_334(&s, &f, k_334_hw3, 2500) && pedal_map(&f) == 0, "0.72 km/h -> CHILL applied");
    CHECK(!s.accel_mode_pending, "applied -> not pending");
}

static void test_accel_off_immediate(void) {
    FSDState s;
    CanFrame f;
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    accel_speed(&s, k_257_stop, 1000);
    CHECK(accel_334(&s, &f, k_334_hw4, 1000), "latched at standstill");
    accel_speed(&s, k_257_31, 1500);
    s.accel_mode = ACCEL_MODE_OFF;
    CHECK(!accel_334(&s, &f, k_334_hw4, 1500) && memcmp(f.data, k_334_hw4, 8) == 0,
          "Off while moving -> pass-through at once");
    CHECK(s.accel_mode_applied == ACCEL_MODE_OFF && !s.accel_mode_pending, "Off clears latch");
    // Back on while still moving: waits for the next stop.
    s.accel_mode = ACCEL_MODE_PERFORMANCE;
    CHECK(!accel_334(&s, &f, k_334_hw4, 1600) && s.accel_mode_pending,
          "re-enabled while moving -> pending");
}

static void test_accel_speed_gate(void) {
    FSDState s;
    CanFrame f;
    // No speed, no gear: never engages.
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    CHECK(!accel_334(&s, &f, k_334_hw3, 5000) && s.accel_mode_pending, "no speed -> no engage");
    // SNA speed (fresh) is unknown, not stopped.
    accel_speed(&s, k_257_sna, 5000);
    CHECK(!fsd_accel_standstill(&s, 5000) && !accel_334(&s, &f, k_334_hw3, 5000),
          "SNA speed -> no engage");
    // Stale standstill reading: no engage.
    accel_speed(&s, k_257_stop, 5000);
    CHECK(fsd_accel_standstill(&s, 6000), "stop 1000 ms old -> still fresh");
    CHECK(!fsd_accel_standstill(&s, 6001) && !accel_334(&s, &f, k_334_hw3, 6001),
          "stop 1001 ms old -> stale, no engage");
    // Reversing at 3.5 km/h is not standstill (the clamped kph would read 0).
    accel_speed(&s, k_257_rev, 7000);
    CHECK(!fsd_accel_standstill(&s, 7000) && !accel_334(&s, &f, k_334_hw3, 7000),
          "reverse -3.52 km/h -> no engage");
    accel_speed(&s, k_257_1p60, 7500);
    CHECK(!fsd_accel_standstill(&s, 7500), "1.60 km/h -> not standstill");
    accel_speed(&s, k_257_0p72, 8000);
    CHECK(fsd_accel_standstill(&s, 8000), "0.72 km/h -> standstill");
}

static void test_accel_gear_gate(void) {
    FSDState s;
    CanFrame f;
    // No 0x257 on the tap: a fresh DI_gear = P counts.
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    accel_gear(&s, k_118_d, 1000);
    CHECK(!fsd_accel_standstill(&s, 1000) && !accel_334(&s, &f, k_334_hw3, 1000),
          "gear D, no speed -> no engage");
    accel_gear(&s, k_118_p, 2000);
    CHECK(accel_334(&s, &f, k_334_hw3, 2000) && pedal_map(&f) == 2, "gear P, no speed -> engage");
    CHECK(!fsd_accel_standstill(&s, 3001), "gear P 1001 ms old -> stale");
    // Fresh valid speed wins over gear: P with 30 km/h is not stopped.
    accel_speed(&s, k_257_31, 3000);
    accel_gear(&s, k_118_p, 3000);
    CHECK(!fsd_accel_standstill(&s, 3000), "moving speed overrides gear P");
    // SNA speed falls back to the gear.
    accel_speed(&s, k_257_sna, 3100);
    CHECK(fsd_accel_standstill(&s, 3100), "SNA speed + fresh P -> standstill");
}

static void test_accel_bad_frames(void) {
    FSDState s;
    CanFrame f;
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    accel_speed(&s, k_257_stop, 1000);
    // Wrong DLC.
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_UI_POWERTRAIN; f.dlc = 7;
    memcpy(f.data, k_334_hw3, 8);
    CHECK(!fsd_handle_accel_mode(&s, &f, 1000) && memcmp(f.data, k_334_hw3, 8) == 0,
          "dlc7 -> untouched");
    // Bad checksum.
    uint8_t bad[8];
    memcpy(bad, k_334_hw3, 8);
    bad[7] ^= 0x10;
    CHECK(!accel_334(&s, &f, bad, 1000) && memcmp(f.data, bad, 8) == 0,
          "bad checksum -> untouched");
    // Pedal map 3 is undefined.
    uint8_t m3[8];
    accel_with_map(m3, k_334_hw3, 3);
    CHECK(!accel_334(&s, &f, m3, 1000) && memcmp(f.data, m3, 8) == 0, "pedal map 3 -> untouched");
    CHECK(s.accel_bad_frames == 3 && !s.accel_car_seen && s.accel_mode_applied == ACCEL_MODE_OFF,
          "rejected frames counted, state untouched (bad=%u)", s.accel_bad_frames);
}

static void test_accel_tx_gate(void) {
    FSDState s;
    CanFrame f;
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    s.op_mode = OpMode_ListenOnly;
    accel_speed(&s, k_257_stop, 1000);
    CHECK(!accel_334(&s, &f, k_334_hw3, 1000) && memcmp(f.data, k_334_hw3, 8) == 0,
          "Listen-Only -> untouched");
    CHECK(s.accel_mode_applied == ACCEL_MODE_OFF && !s.accel_mode_pending,
          "Listen-Only -> no latch");
    // Activated while moving: must not start rewriting until the next stop.
    s.op_mode = OpMode_Active;
    accel_speed(&s, k_257_31, 1500);
    CHECK(!accel_334(&s, &f, k_334_hw3, 1500) && s.accel_mode_pending,
          "activated while moving -> pending");
    accel_speed(&s, k_257_stop, 2000);
    CHECK(accel_334(&s, &f, k_334_hw3, 2000), "stop -> applied");
    // Autopark / OTA pause drop the latch.
    s.autopark_tx_block = true;
    CHECK(fsd_accel_sent_map(&s) == 1, "Autopark block -> sent map = car map");
    CHECK(!accel_334(&s, &f, k_334_hw3, 2500) && s.accel_mode_applied == ACCEL_MODE_OFF,
          "Autopark block -> pass-through");
    s.autopark_tx_block = false;
    s.tesla_ota_in_progress = true;
    accel_speed(&s, k_257_stop, 3000);
    CHECK(!accel_334(&s, &f, k_334_hw3, 3000), "OTA -> pass-through");
}

static void test_accel_touchscreen_change(void) {
    FSDState s;
    CanFrame f;
    uint8_t chill[8];
    accel_with_map(chill, k_334_hw3, 0);
    accel_state(&s, ACCEL_MODE_PERFORMANCE);
    accel_speed(&s, k_257_stop, 1000);
    CHECK(accel_334(&s, &f, k_334_hw3, 1000), "latched PERFORMANCE");
    accel_speed(&s, k_257_31, 1500);
    CHECK(accel_334(&s, &f, k_334_hw3, 1500), "moving, car unchanged -> still rewritten");
    // Driver picks Chill on the touchscreen while moving: their choice goes through.
    CHECK(!accel_334(&s, &f, chill, 2000) && memcmp(f.data, chill, 8) == 0,
          "touchscreen change while moving -> pass-through");
    CHECK(s.accel_mode_applied == ACCEL_MODE_OFF && s.accel_mode_pending && s.accel_car_map == 0,
          "touchscreen change -> latch dropped, pending");
    accel_speed(&s, k_257_31, 2500);
    CHECK(!accel_334(&s, &f, chill, 2500), "still moving -> pass-through");
    accel_speed(&s, k_257_stop, 3000);
    CHECK(accel_334(&s, &f, chill, 3000) && pedal_map(&f) == 2, "next stop -> re-applied");
}

// ── Performance read-out (fsd_perf.h) ──────────────────────────────────────────

// Acceleration-timer state machine: arm leaving standstill, lock at the target,
// keep the best across runs, reset at the next standstill, and never lock on a
// run that doesn't reach the target.
static void test_perf_accel_timer(void) {
    FSDPerf p;
    fsd_perf_reset(&p);
    // Standstill baseline, then launch at t=100ms (clock starts at the standstill
    // sample's timestamp).
    fsd_perf_update(&p, 0.0f, 0);
    fsd_perf_update(&p, 0.5f, 100);
    fsd_perf_update(&p, 5.0f, 200);      // leaves standstill -> armed, start=100
    CHECK(p.accel_armed, "accel armed after launch");
    CHECK(!p.lock_0_50 && p.t_0_50_ms == 0, "no 0-50 lock below target");
    fsd_perf_update(&p, 55.0f, 4000);    // >= 50 km/h -> lock, 4000-100=3900ms
    CHECK(p.lock_0_50 && p.t_0_50_ms == 3900, "0-50 locked at 3900ms got %lu",
          (unsigned long)p.t_0_50_ms);
    CHECK(p.best_0_50_ms == 3900, "best 0-50 seeded");
    CHECK(!p.lock_0_60mph, "0-60mph not yet (55<96.56)");
    fsd_perf_update(&p, 100.0f, 7000);   // >= 96.56 and >= 100 -> both lock, 6900ms
    CHECK(p.lock_0_60mph && p.t_0_60mph_ms == 6900, "0-60mph locked 6900ms got %lu",
          (unsigned long)p.t_0_60mph_ms);
    CHECK(p.lock_0_100 && p.t_0_100_ms == 6900, "0-100 locked 6900ms");

    // Reset at the next standstill, then a quicker run improves the best.
    fsd_perf_update(&p, 0.0f, 20000);
    CHECK(!p.accel_armed, "disarmed at standstill");
    fsd_perf_update(&p, 60.0f, 21000);   // relaunch start=20000, 60>=50 -> 1000ms
    CHECK(p.t_0_50_ms == 1000 && p.best_0_50_ms == 1000,
          "0-50 improved to 1000ms (best %lu)", (unsigned long)p.best_0_50_ms);

    // A run that never reaches a target leaves the locks clear.
    FSDPerf q;
    fsd_perf_reset(&q);
    fsd_perf_update(&q, 0.0f, 0);
    fsd_perf_update(&q, 30.0f, 1000);
    fsd_perf_update(&q, 0.0f, 2000);
    CHECK(!q.lock_0_50 && q.best_0_50_ms == 0, "no lock/best when target never reached");
}

// Braking 100->0 run times from crossing down through 100 km/h to standstill.
static void test_perf_brake_timer(void) {
    FSDPerf p;
    fsd_perf_reset(&p);
    fsd_perf_update(&p, 110.0f, 0);       // above 100
    fsd_perf_update(&p, 95.0f, 500);      // crossed down through 100 -> brake start=500
    CHECK(p.brake_armed, "brake armed crossing 100");
    fsd_perf_update(&p, 40.0f, 2000);
    CHECK(!p.lock_100_0, "brake not locked until standstill");
    fsd_perf_update(&p, 0.0f, 3500);      // standstill -> lock 3500-500=3000ms
    CHECK(p.lock_100_0 && p.t_100_0_ms == 3000 && p.best_100_0_ms == 3000,
          "100-0 locked 3000ms got %lu", (unsigned long)p.t_100_0_ms);
}

// Estimated longitudinal G: sign and magnitude on a synthetic ramp, and the
// dt guard rejects implausibly short sample gaps.
static void test_perf_g_estimate(void) {
    FSDPerf p;
    fsd_perf_reset(&p);
    fsd_perf_update(&p, 0.0f, 0);
    fsd_perf_update(&p, 36.0f, 1000);     // +10 m/s in 1s = +1.0197 g
    CHECK(p.g_est > 1.0f && p.g_est < 1.04f, "accel g ~1.02 got %.3f", p.g_est);
    CHECK(p.g_peak > 1.0f && p.g_peak < 1.04f, "g peak captured");
    fsd_perf_update(&p, 0.0f, 2000);      // -10 m/s in 1s = -1.0197 g
    CHECK(p.g_est < -1.0f && p.g_est > -1.04f, "brake g ~-1.02 got %.3f", p.g_est);
    CHECK(p.g_peak_brake < -1.0f, "brake peak captured");
    float before = p.g_est;
    fsd_perf_update(&p, 20.0f, 2005);     // dt=5ms < PERF_DT_MIN_MS -> ignored
    CHECK(p.g_est == before, "sub-20ms sample gap ignored for G");
}

static void test_perf_wheel_slip(void) {
    float out = -1.0f;
    CHECK(fsd_perf_wheel_slip_pct(50.0f, 50.0f, 55.0f, 55.0f, &out) &&
          out > 9.99f && out < 10.01f, "slip 10%% got %.3f", out);
    CHECK(fsd_perf_wheel_slip_pct(50.0f, 50.0f, 45.0f, 45.0f, &out) &&
          out < -9.99f && out > -10.01f, "front faster -> negative slip got %.3f", out);
    out = 123.0f;
    CHECK(!fsd_perf_wheel_slip_pct(1.0f, 1.0f, 1.0f, 1.0f, &out) && out == 123.0f,
          "slip undefined near standstill (out untouched)");
}

static void test_perf_temp_band(void) {
    CHECK(fsd_perf_temp_band(-5, PERF_BATT_COLD_C, PERF_BATT_WARM_C, PERF_BATT_HOT_C) == PERF_TEMP_COLD, "cold");
    CHECK(fsd_perf_temp_band(5, PERF_BATT_COLD_C, PERF_BATT_WARM_C, PERF_BATT_HOT_C) == PERF_TEMP_COLD, "cold boundary");
    CHECK(fsd_perf_temp_band(20, PERF_BATT_COLD_C, PERF_BATT_WARM_C, PERF_BATT_HOT_C) == PERF_TEMP_NORMAL, "normal");
    CHECK(fsd_perf_temp_band(40, PERF_BATT_COLD_C, PERF_BATT_WARM_C, PERF_BATT_HOT_C) == PERF_TEMP_WARM, "warm");
    CHECK(fsd_perf_temp_band(45, PERF_BATT_COLD_C, PERF_BATT_WARM_C, PERF_BATT_HOT_C) == PERF_TEMP_HOT, "hot boundary");
    CHECK(fsd_perf_temp_band(60, PERF_BATT_COLD_C, PERF_BATT_WARM_C, PERF_BATT_HOT_C) == PERF_TEMP_HOT, "hot");
}

// 0x175 ESP_wheelSpeeds parser: four 13-bit LE fields, factor 0.04 km/h.
static void test_perf_wheel_speeds_parse(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_ESP_WHEELSPD;
    f.dlc = 8;
    uint16_t fl = 1250, fr = 1250, rl = 1375, rr = 1375;  // 50,50,55,55 km/h
    uint64_t w = (uint64_t)fl | ((uint64_t)fr << 13) |
                 ((uint64_t)rl << 26) | ((uint64_t)rr << 39);
    for (int i = 0; i < 8; i++) f.data[i] = (uint8_t)((w >> (8 * i)) & 0xFF);
    fsd_handle_wheel_speeds(&s, &f);
    CHECK(s.wheel_speed_seen, "wheel speeds seen");
    CHECK(s.wheel_speed_fl_kph > 49.9f && s.wheel_speed_fl_kph < 50.1f,
          "FL 50 got %.2f", s.wheel_speed_fl_kph);
    CHECK(s.wheel_speed_rr_kph > 54.9f && s.wheel_speed_rr_kph < 55.1f,
          "RR 55 got %.2f", s.wheel_speed_rr_kph);
    float slip = 0.0f;
    CHECK(fsd_perf_wheel_slip_pct(s.wheel_speed_fl_kph, s.wheel_speed_fr_kph,
                                  s.wheel_speed_rl_kph, s.wheel_speed_rr_kph, &slip) &&
          slip > 9.9f && slip < 10.1f, "state slip ~10%% got %.2f", slip);
    // SNA/full-scale field is rejected (plausibility gate, no checksum modeled).
    FSDState s2;
    memset(&s2, 0, sizeof(s2));
    uint64_t bad = (uint64_t)0x1FFF | ((uint64_t)fr << 13) |
                   ((uint64_t)rl << 26) | ((uint64_t)rr << 39);
    for (int i = 0; i < 8; i++) f.data[i] = (uint8_t)((bad >> (8 * i)) & 0xFF);
    fsd_handle_wheel_speeds(&s2, &f);
    CHECK(!s2.wheel_speed_seen, "full-scale SNA field rejected");
}

// ── Charge-port type (0x25D CP_status, #228) ─────────────────────────────────
// Real idle-US frames from the #211 Fremont dump and the discussion example:
// CP_type US_TESLA(0), cable not present, cable state NOT_CONNECTED -> unplugged.
static const uint8_t k_25d_us_unplugged[8]  = {0x34,0x41,0x01,0x00,0x04,0xF0,0xD4,0x03};
static const uint8_t k_25d_us_unplugged2[8] = {0x34,0x41,0x01,0x16,0x44,0xF0,0xCF,0x03};

static void cp_set(FSDState* s, bool on) {
    memset(s, 0, sizeof(*s));
    s->op_mode = OpMode_Active;
    s->cp_type_eu_rewrite = on;
}
static CanFrame cp_frame(const uint8_t* d, uint8_t dlc) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_CP_STATUS;
    f.dlc = dlc;
    memcpy(f.data, d, dlc <= 8 ? dlc : 8);
    return f;
}

static void test_cp_type_decode(void) {
    // Decode is read-only and runs even in Listen-Only, toggle off.
    FSDState s; cp_set(&s, false); s.op_mode = OpMode_ListenOnly;
    CanFrame f = cp_frame(k_25d_us_unplugged, 8);
    CHECK(!fsd_handle_cp_type_rewrite(&s, &f, 1000u), "decode-only: no rewrite, toggle off");
    CHECK(s.cp_status_seen && s.cp_type == FSD_CP_TYPE_US_TESLA &&
          s.cp_plug_state == FSD_CP_PLUG_UNPLUGGED,
          "idle US decoded: seen=%d type=%u plug=%u", s.cp_status_seen, s.cp_type, s.cp_plug_state);
    CHECK(memcmp(f.data, k_25d_us_unplugged, 8) == 0, "decode does not touch the frame");

    // The 6-byte Party-CAN 0x25D is not CP_status: decodes to nothing.
    FSDState s6; cp_set(&s6, true);
    CanFrame f6 = cp_frame(k_25d_us_unplugged, 6);
    CHECK(!fsd_handle_cp_type_rewrite(&s6, &f6, 1000u) && !s6.cp_status_seen, "6-byte 0x25D ignored");

    // Plug-state table straight from the shared decoder.
    uint8_t d[8]; memcpy(d, k_25d_us_unplugged, 8);
    CHECK(fsd_cp_plug_state(d) == FSD_CP_PLUG_UNPLUGGED, "NOT_CONNECTED -> unplugged");
    d[0] |= 0x08u;  // CP_chargeCablePresent
    CHECK(fsd_cp_plug_state(d) == FSD_CP_PLUG_PLUGGED, "cable present -> plugged");
    memcpy(d, k_25d_us_unplugged, 8); d[1] |= 0x04u;  // CP_chargeDoorOpen (bit10)
    CHECK(fsd_cp_plug_state(d) == FSD_CP_PLUG_PLUGGED, "charge door open -> plugged");
    memcpy(d, k_25d_us_unplugged, 8); d[7] |= 0x20u;  // CP_hvInletExposed (bit61)
    CHECK(fsd_cp_plug_state(d) == FSD_CP_PLUG_PLUGGED, "HV inlet exposed -> plugged");
    memcpy(d, k_25d_us_unplugged, 8);
    d[0] = (uint8_t)((d[0] & ~0x60u) | (2u << 5));     // CP_latchState = ENGAGED
    CHECK(fsd_cp_plug_state(d) == FSD_CP_PLUG_PLUGGED, "latch engaged -> plugged");
    memcpy(d, k_25d_us_unplugged, 8);
    d[1] = (uint8_t)((d[1] & ~0xC0u) | (2u << 6));     // CP_chargeCableState = CONNECTED
    CHECK(fsd_cp_plug_state(d) == FSD_CP_PLUG_PLUGGED, "cable state CONNECTED -> plugged");
    memcpy(d, k_25d_us_unplugged, 8);
    d[1] = (uint8_t)(d[1] & ~0xC0u);                   // CP_chargeCableState = SNA, nothing else set
    CHECK(fsd_cp_plug_state(d) == FSD_CP_PLUG_UNKNOWN, "cable state SNA -> unknown (fail closed)");
}

static void test_cp_type_stale(void) {
    // Each 8-byte CP_status stamps when it arrived; the read-out goes stale
    // FSD_CP_STATUS_STALE_MS after the last one, never before the first.
    FSDState s; cp_set(&s, false);
    CHECK(!fsd_cp_status_stale(s.cp_status_seen, s.cp_status_ms, 100000u), "never seen -> not stale");
    CanFrame f = cp_frame(k_25d_us_unplugged, 8);
    fsd_handle_cp_type_rewrite(&s, &f, 5000u);
    CHECK(s.cp_status_ms == 5000u, "arrival stamped (%u)", s.cp_status_ms);
    CHECK(!fsd_cp_status_stale(s.cp_status_seen, s.cp_status_ms, 5000u + FSD_CP_STATUS_STALE_MS),
          "fresh up to the limit");
    CHECK(fsd_cp_status_stale(s.cp_status_seen, s.cp_status_ms, 5001u + FSD_CP_STATUS_STALE_MS),
          "stale past it");
    f = cp_frame(k_25d_us_unplugged2, 8);
    fsd_handle_cp_type_rewrite(&s, &f, 9000u);
    CHECK(!fsd_cp_status_stale(s.cp_status_seen, s.cp_status_ms, 9100u), "a new frame makes it fresh again");

    // A 6-byte 0x25D (Party CAN DAS_road) is not CP_status and keeps nothing alive.
    CanFrame f6 = cp_frame(k_25d_us_unplugged, 6);
    fsd_handle_cp_type_rewrite(&s, &f6, 20000u);
    CHECK(s.cp_status_ms == 9000u && fsd_cp_status_stale(s.cp_status_seen, s.cp_status_ms, 20000u),
          "6-byte 0x25D does not refresh the stamp (%u)", s.cp_status_ms);

    // millis() wraps after ~49 days.
    FSDState w; cp_set(&w, false);
    f = cp_frame(k_25d_us_unplugged, 8);
    fsd_handle_cp_type_rewrite(&w, &f, 0xFFFFFF00u);
    CHECK(!fsd_cp_status_stale(w.cp_status_seen, w.cp_status_ms, 0x00000100u), "fresh across the wrap");
    CHECK(fsd_cp_status_stale(w.cp_status_seen, w.cp_status_ms, 0x00000100u + FSD_CP_STATUS_STALE_MS),
          "stale across the wrap");
}

static void test_cp_type_rewrite(void) {
    // US + unplugged + Active + toggle on -> one bit-preserving IEC_CCS copy.
    FSDState s; cp_set(&s, true);
    CanFrame f = cp_frame(k_25d_us_unplugged, 8);
    CHECK(fsd_handle_cp_type_rewrite(&s, &f, 1000u), "US unplugged -> rewritten");
    CHECK(f.data[0] == 0x37 && memcmp(f.data + 1, k_25d_us_unplugged + 1, 7) == 0,
          "only CP_type changed: byte0=0x%02X", f.data[0]);
    // The copy is counted by the caller once the CAN driver has taken it, so a
    // send refused by a full TX queue never shows as sent.
    CHECK(fsd_cp_type_value(f.data[0]) == FSD_CP_TYPE_IEC_CCS && s.cp_type_tx_count == 0,
          "copy is IEC_CCS, not counted before the send (%u)", s.cp_type_tx_count);
    CanFrame f2 = cp_frame(k_25d_us_unplugged2, 8);
    CHECK(fsd_handle_cp_type_rewrite(&s, &f2, 1000u) && f2.data[0] == 0x37 && s.cp_type_tx_count == 0,
          "second US frame rewritten (tx=%u)", s.cp_type_tx_count);

    // Already IEC_CCS -> never echoed (no self-trigger loop).
    FSDState s3; cp_set(&s3, true);
    uint8_t ccs[8]; memcpy(ccs, k_25d_us_unplugged, 8); ccs[0] = fsd_cp_type_make_iec_ccs(ccs[0]);
    CanFrame f3 = cp_frame(ccs, 8);
    CHECK(!fsd_handle_cp_type_rewrite(&s3, &f3, 1000u) && s3.cp_type == FSD_CP_TYPE_IEC_CCS,
          "already IEC_CCS -> not rewritten");

    // CP_type 1 (EURO_IEC) and 2 (GB): not US, left alone.
    for (uint8_t t = 1; t <= 2; t++) {
        FSDState st; cp_set(&st, true);
        uint8_t d[8]; memcpy(d, k_25d_us_unplugged, 8);
        d[0] = (uint8_t)((d[0] & 0xFCu) | t);
        CanFrame ft = cp_frame(d, 8);
        CHECK(!fsd_handle_cp_type_rewrite(&st, &ft, 1000u) && ft.data[0] == d[0],
              "CP_type %u -> untouched", t);
    }

    // Plug guard: cable present -> held, even though US + Active + toggle on.
    FSDState sp; cp_set(&sp, true);
    uint8_t plugged[8]; memcpy(plugged, k_25d_us_unplugged, 8); plugged[0] |= 0x08u;
    CanFrame fp = cp_frame(plugged, 8);
    CHECK(!fsd_handle_cp_type_rewrite(&sp, &fp, 1000u) && memcmp(fp.data, plugged, 8) == 0,
          "cable present -> held (plug guard)");
    CHECK(sp.cp_status_seen && sp.cp_plug_state == FSD_CP_PLUG_PLUGGED && sp.cp_type_tx_count == 0,
          "plug guard: decoded but no copy");

    // Unknown plug state (SNA) -> fail closed.
    FSDState su; cp_set(&su, true);
    uint8_t sna[8]; memcpy(sna, k_25d_us_unplugged, 8); sna[1] = (uint8_t)(sna[1] & ~0xC0u);
    CanFrame fu = cp_frame(sna, 8);
    CHECK(!fsd_handle_cp_type_rewrite(&su, &fu, 1000u) && su.cp_plug_state == FSD_CP_PLUG_UNKNOWN,
          "plug state unknown -> held (fail closed)");

    // Wrong DLC -> no rewrite, no decode.
    FSDState sd; cp_set(&sd, true);
    CanFrame fd = cp_frame(k_25d_us_unplugged, 7);
    CHECK(!fsd_handle_cp_type_rewrite(&sd, &fd, 1000u) && !sd.cp_status_seen, "dlc7 -> untouched");

    // Transmit gate closed: decode still runs, no copy.
    FSDState sg; cp_set(&sg, true); sg.op_mode = OpMode_ListenOnly;
    CanFrame fg = cp_frame(k_25d_us_unplugged, 8);
    CHECK(!fsd_handle_cp_type_rewrite(&sg, &fg, 1000u) && sg.cp_status_seen && sg.cp_type_tx_count == 0,
          "Listen-Only -> decoded, no copy");
    sg.op_mode = OpMode_Active; sg.tesla_ota_in_progress = true;
    fg = cp_frame(k_25d_us_unplugged, 8);
    CHECK(!fsd_handle_cp_type_rewrite(&sg, &fg, 1000u), "OTA in progress -> no copy");
    sg.tesla_ota_in_progress = false; sg.autopark_tx_block = true;
    fg = cp_frame(k_25d_us_unplugged, 8);
    CHECK(!fsd_handle_cp_type_rewrite(&sg, &fg, 1000u), "Autopark pause -> no copy");

    // The rewrite depends only on frame bytes + state, never on a bus id, so it
    // behaves the same whichever bus delivers 0x25D (main.cpp re-sends on the
    // arrival bus). Same frame, two independent states -> identical result.
    FSDState ba, bb; cp_set(&ba, true); cp_set(&bb, true);
    CanFrame fa = cp_frame(k_25d_us_unplugged, 8), fb = cp_frame(k_25d_us_unplugged, 8);
    CHECK(fsd_handle_cp_type_rewrite(&ba, &fa, 1000u) == fsd_handle_cp_type_rewrite(&bb, &fb, 1000u) &&
          memcmp(fa.data, fb.data, 8) == 0, "rewrite is bus-agnostic (pure in the frame)");
}

// ── Black-box save in slices ─────────────────────────────────────────────────
// The window the save used to find with a linear scan of the whole ring.
static BBWindow bb_window_linear(const uint32_t* ts, uint32_t n, uint32_t lo, uint32_t hi) {
    BBWindow w = {0, 0};
    bool started = false;
    for (uint32_t k = 0; k < n; k++) {
        if (ts[k] < lo) continue;
        if (ts[k] > hi) break;
        if (!started) { w.first = k; started = true; }
        w.count++;
    }
    return w;
}

static void test_blackbox_window(void) {
    uint32_t ts[64];
    uint32_t seed = 0x1234567u;
    int bad = 0;
    for (int trial = 0; trial < 3000; trial++) {
        uint32_t n = xorshift32(&seed) % 64u;
        uint32_t t = 1000u + xorshift32(&seed) % 50u;
        for (uint32_t k = 0; k < n; k++) {
            ts[k] = t;
            t += xorshift32(&seed) % 4u == 0 ? 0u : xorshift32(&seed) % 30u;  // with repeats
        }
        uint32_t lo = 990u + xorshift32(&seed) % (t - 990u + 20u);
        uint32_t hi = lo + xorshift32(&seed) % 400u;
        BBWindow got = bb_find_window(n, lo, hi, [&](uint32_t k) { return ts[k]; });
        BBWindow want = bb_window_linear(ts, n, lo, hi);
        if (got.count != want.count || (want.count && got.first != want.first)) bad++;
    }
    CHECK(bad == 0, "binary-search window matches the linear scan (%d mismatches)", bad);

    uint32_t two[3] = {100u, 200u, 200u};
    auto at = [&](uint32_t k) { return two[k]; };
    BBWindow w = bb_find_window(0, 0u, 1000u, at);
    CHECK(w.count == 0, "empty ring: empty window");
    w = bb_find_window(3, 300u, 400u, at);
    CHECK(w.count == 0 && w.first == 3, "everything before the window");
    w = bb_find_window(3, 0u, 50u, at);
    CHECK(w.count == 0 && w.first == 0, "everything after the window");
    w = bb_find_window(3, 200u, 200u, at);
    CHECK(w.first == 1 && w.count == 2, "lo == hi on a repeated timestamp: both frames");
    w = bb_find_window(3, 100u, 199u, at);
    CHECK(w.first == 0 && w.count == 1, "hi just before a frame excludes it");
}

static void test_blackbox_ring_push(void) {
    // Without a save, a full ring evicts its oldest frame.
    const uint32_t cap = 8u;
    uint32_t vals[cap] = {};
    uint32_t head = 0, tail = 0, slot = 0;
    bool pushed = true;
    for (uint32_t v = 0; v < 20u; v++) {
        pushed &= bb_ring_push(cap, head, tail, cap, &slot);
        vals[slot] = v;
    }
    CHECK(pushed, "without a save every push lands");
    CHECK(bb_ring_count(cap, head, tail) == cap - 1u, "full ring holds cap - 1 frames");
    bool order = true;
    for (uint32_t k = 0; k < cap - 1u; k++) order &= vals[(tail + k) % cap] == 13u + k;
    CHECK(order, "the newest seven frames, oldest first");
}

static void test_blackbox_save_slices(void) {
    // A full ring of frames 0..14; the event window is 3..11. The save writes
    // four frames a pass while eight new frames arrive between passes, more
    // than the save frees, so the ring fills up to the frames still to write.
    const uint32_t cap = 16u;
    uint32_t vals[cap] = {};
    uint32_t head = 0, tail = 0, slot = 0;
    for (uint32_t v = 0; v < 15u; v++) {
        bb_ring_push(cap, head, tail, cap, &slot);
        vals[slot] = v;
    }
    uint32_t t0 = tail;
    BBWindow w = bb_find_window(bb_ring_count(cap, head, tail), 3u, 11u,
                                [&](uint32_t k) { return vals[(t0 + k) % cap]; });
    CHECK(w.first == 3u && w.count == 9u, "window 3..11 (first %u, count %u)",
          (unsigned)w.first, (unsigned)w.count);
    uint32_t next = (tail + w.first) % cap, left = w.count;
    uint32_t written[16], nw = 0, passes = 0, dropped = 0, v = 100u;
    while (left) {
        for (uint32_t n = bb_slice(left, 4u); n > 0; n--) {
            written[nw++] = vals[next];
            next = bb_ring_next(cap, next);
            left--;
        }
        passes++;
        for (int i = 0; i < 8; i++, v++) {
            if (bb_ring_push(cap, head, tail, left ? next : cap, &slot)) vals[slot] = v;
            else dropped++;
        }
    }
    bool intact = nw == 9u;
    for (uint32_t k = 0; intact && k < nw; k++) intact = written[k] == 3u + k;
    CHECK(intact, "every window frame written once, in order, none overwritten");
    CHECK(passes == 3u, "9 frames at 4 a pass: 3 passes (%u)", (unsigned)passes);
    CHECK(dropped > 0u, "new frames dropped while the ring was full up to the save (%u)",
          (unsigned)dropped);
    CHECK(bb_ring_count(cap, head, tail) == cap - 1u, "ring still full after the save");
    CHECK(bb_ring_push(cap, head, tail, cap, &slot), "after the save the oldest frame goes again");

    // Slices add up to the window, never more than the per-pass cap.
    bool ok = true;
    for (uint32_t count = 0; count < 100u; count++) {
        for (uint32_t per = 1; per < 40u; per++) {
            uint32_t left2 = count, sum = 0, n2 = 0;
            while (left2) {
                uint32_t s2 = bb_slice(left2, per);
                ok &= s2 >= 1u && s2 <= per;
                sum += s2; left2 -= s2; n2++;
            }
            ok &= sum == count && n2 == (count + per - 1u) / per;
        }
    }
    CHECK(ok, "slices cover the window in ceil(count / per_pass) passes");
}

// Slices cut by the time budget (BLACKBOX_FLUSH_BUDGET_US) and the gap after them.
static void test_blackbox_slice_budget(void) {
    CHECK(!bb_slice_over(1u, 32u, 0u, 600u), "one line, time left: go on");
    CHECK(bb_slice_over(1u, 32u, 600u, 600u), "budget gone after one line: stop");
    CHECK(bb_slice_over(32u, 32u, 10u, 600u) && bb_slice_over(5u, 5u, 0u, 600u),
          "all the slice's lines written: stop");
    CHECK(bb_slice_gap_ms(32u, 32u, 50u) == 50u && bb_slice_gap_ms(16u, 32u, 50u) == 25u &&
              bb_slice_gap_ms(15u, 32u, 50u) == 24u && bb_slice_gap_ms(1u, 32u, 50u) == 2u &&
              bb_slice_gap_ms(0u, 32u, 50u) == 1u,
          "gap: full after 32 lines, the share rounded up after fewer");

    // Whatever the slices, lines go out no faster than 32 per 50 ms.
    uint32_t seed = 0xB1ACB0Fu;
    bool pace = true;
    for (int trial = 0; trial < 500; trial++) {
        uint32_t lines = 0, ms = 0;
        for (int k = 0; k < 200; k++) {
            uint32_t n = 1u + xorshift32(&seed) % 32u;
            lines += n;
            ms += bb_slice_gap_ms(n, 32u, 50u);
        }
        pace &= (uint64_t)ms * 32u >= (uint64_t)lines * 50u;
    }
    CHECK(pace, "budget-cut slices never outpace 32 lines per 50 ms");

    // A 1501-line save with lines of 40 us: 15 lines a slice instead of 32, each
    // ending at the budget, the save about as long as with 32-line slices.
    const uint32_t LINE_US = 40u, BUDGET = 600u, TOTAL = 1501u;
    uint32_t left = TOTAL, slices = 0, ms = 0, longest = 0;
    while (left) {
        uint32_t n = bb_slice(left, 32u), lines = 0, us = 0;
        do { lines++; us += LINE_US; left--; } while (!bb_slice_over(lines, n, us, BUDGET));
        if (us > longest) longest = us;
        slices++;
        ms += bb_slice_gap_ms(lines, 32u, 50u);
    }
    const uint32_t fixed_ms = ((TOTAL + 31u) / 32u) * 50u;    // 47 slices of 32
    CHECK(slices == 101u && longest == 600u, "15-line slices of 600 us (%u slices, %u us)",
          (unsigned)slices, (unsigned)longest);
    CHECK(ms >= fixed_ms && ms <= fixed_ms + fixed_ms / 10u,
          "save takes %u ms against %u ms with 32-line slices", (unsigned)ms, (unsigned)fixed_ms);
}

// A trigger while an event is armed (blackbox_arm()).
static void test_blackbox_arm_action(void) {
    const uint32_t POST = 5000u;
    CHECK(bb_arm_action(false, 0u, false, 0u, POST, 123u) == BB_ARM_NOW, "nothing armed: arm");
    CHECK(bb_arm_action(true, 1000u, false, 0u, POST, 6000u) == BB_ARM_IN_WINDOW,
          "at the end of the post-roll: inside the armed window");
    CHECK(bb_arm_action(true, 1000u, false, 0u, POST, 6001u) == BB_ARM_QUEUE,
          "after it, during the save: waits");
    CHECK(bb_arm_action(true, 1000u, true, 7000u, POST, 12000u) == BB_ARM_IN_WINDOW,
          "a second one in the waiting event's post-roll: part of it");
    CHECK(bb_arm_action(true, 1000u, true, 7000u, POST, 12001u) == BB_ARM_MISSED,
          "a second one after it: missed");
    CHECK(bb_arm_action(true, 0xFFFFF000u, false, 0u, POST, 0x00000100u) == BB_ARM_IN_WINDOW,
          "post-roll across the millis() wrap");
    CHECK(bb_arm_action(true, 0xFFFFF000u, false, 0u, POST, 0xFFFFF000u + POST + 1u) ==
              BB_ARM_QUEUE, "past the post-roll across the wrap");
}

// The save and the queue together, in the order blackbox.cpp runs them: `rate`
// key frames every 10 ms, a save step of 32 frames every 50 ms, the trigger
// handling of blackbox_arm() and blackbox_tick(). Returns the saved events.
struct BBSimEvent {
    uint32_t trig_ms, first_ts, last_ts, count;
};
static int bb_sim(uint32_t cap, uint32_t rate, const uint32_t* trig, int ntrig,
                  BBSimEvent* out, uint32_t* missed, uint32_t* dropped) {
    const uint32_t PRE = 10000u, POST = 5000u, TICK = 10u, GAP = 50u, PER = 32u;
    static uint32_t ts[16384];
    uint32_t head = 0, tail = 0, slot = 0;
    bool armed = false, queued = false, saving = false;
    uint32_t trig_ms = 0, q_ms = 0, flush_at = 0, next = 0, left = 0;
    int nev = 0, ti = 0;
    *missed = *dropped = 0;
    for (uint32_t now = 100u; now < 120000u; now += TICK) {
        for (uint32_t r = 0; r < rate; r++) {
            if (bb_ring_push(cap, head, tail, left ? next : cap, &slot)) ts[slot] = now;
            else (*dropped)++;
        }
        while (ti < ntrig && trig[ti] <= now) {
            switch (bb_arm_action(armed, trig_ms, queued, q_ms, POST, now)) {
            case BB_ARM_NOW: armed = true; trig_ms = now; flush_at = now + POST; break;
            case BB_ARM_QUEUE: queued = true; q_ms = now; break;
            case BB_ARM_MISSED: (*missed)++; break;
            default: break;
            }
            ti++;
        }
        if (!armed || (int32_t)(now - flush_at) < 0) continue;
        if (!saving) {
            uint32_t t0 = tail;
            BBWindow w = bb_find_window(bb_ring_count(cap, head, tail), trig_ms - PRE,
                                        trig_ms + POST,
                                        [&](uint32_t k) { return ts[(t0 + k) % cap]; });
            next = (tail + w.first) % cap;
            left = w.count;
            out[nev] = {trig_ms, w.count ? ts[next] : 0u, 0u, w.count};
            saving = true;
        }
        for (uint32_t n = bb_slice(left, PER); n > 0; n--) {
            out[nev].last_ts = ts[next];
            next = bb_ring_next(cap, next);
            left--;
        }
        flush_at = now + GAP;
        if (left) continue;
        saving = false;
        armed = false;
        nev++;
        if (queued) {
            queued = false;
            armed = true;
            trig_ms = q_ms;
            flush_at = q_ms + POST;
        }
    }
    return nev;
}

static void test_blackbox_trigger_during_save(void) {
    // 100 frames/s: 1501 frames a window, saved at 32 per 50 ms in about 2.3 s.
    BBSimEvent ev[8];
    uint32_t missed = 0, dropped = 0;
    // 22000: inside the first window. 25500: during its save (25000..), waits.
    // 26000: inside the waiting window. 30800: during the second save, waits in
    // turn. 31000: inside that one.
    const uint32_t t1[] = {20000u, 22000u, 25500u, 26000u, 30800u, 31000u};
    int n = bb_sim(8192u, 1u, t1, 6, ev, &missed, &dropped);
    CHECK(n == 3, "three events saved (%d)", n);
    CHECK(n >= 2 && ev[1].trig_ms == 25500u && ev[1].first_ts == 15500u &&
              ev[1].last_ts == 30500u && ev[1].count == 1501u,
          "the trigger during the save gets its own full window (%u..%u, %u frames)",
          n >= 2 ? (unsigned)ev[1].first_ts : 0u, n >= 2 ? (unsigned)ev[1].last_ts : 0u,
          n >= 2 ? (unsigned)ev[1].count : 0u);
    CHECK(n == 3 && ev[2].trig_ms == 30800u && ev[2].count == 1501u,
          "and the one during its save gets the next");
    CHECK(missed == 0 && dropped == 0, "big ring: nothing missed or dropped");

    // 500 frames/s: 7501 frames, about 11.7 s of saving. 25100 waits; 31000 is
    // past its post-roll (..30100) with the first save still running: missed.
    // 36000 too (the first save ends at about 36750).
    const uint32_t t2[] = {20000u, 25100u, 31000u, 36000u};
    n = bb_sim(16384u, 5u, t2, 4, ev, &missed, &dropped);
    CHECK(n == 2 && ev[1].trig_ms == 25100u && missed == 2,
          "long save: one waits, the later two are missed (%d events, %u missed)", n,
          (unsigned)missed);
    CHECK(n == 2 && ev[1].first_ts == 15100u && ev[1].last_ts == 30100u,
          "the waiting event still has its full window (%u..%u)",
          n == 2 ? (unsigned)ev[1].first_ts : 0u, n == 2 ? (unsigned)ev[1].last_ts : 0u);

    // Small ring (999 frames, 10 s at 100 frames/s): the waiting event's pre-roll
    // is partly gone by the time it is saved; the trigger and its post-roll are not.
    const uint32_t t3[] = {20000u, 25100u};
    n = bb_sim(1000u, 1u, t3, 2, ev, &missed, &dropped);
    CHECK(n == 2 && ev[1].trig_ms == 25100u && ev[1].first_ts > 15100u &&
              ev[1].first_ts < 25100u && ev[1].last_ts == 30100u,
          "small ring: pre-roll partly kept (%u..%u)", n == 2 ? (unsigned)ev[1].first_ts : 0u,
          n == 2 ? (unsigned)ev[1].last_ts : 0u);
}

// Continuous AP (#218): the 0x229 press may only go out on a bus where the
// car's own 0x229 was stored recently, never on a configured bus by default.
static void test_gear_tx_route(void) {
    const uint32_t AGE = 500u;  // GEAR_LEVER_CACHED_COUNTER_MAX_AGE_MS
    GearCounterCache one[1] = {};
    CHECK(gear_tx_bus_index(one, 1, 0, 1000u, AGE) == -1, "single bus, nothing seen: no press");
    one[0] = {7, true, 1000u};
    CHECK(gear_tx_bus_index(one, 1, 0, 1500u, AGE) == 0, "single bus, 500 ms old: can0");
    CHECK(gear_tx_bus_index(one, 1, 0, 1501u, AGE) == -1, "single bus, 501 ms old: no press");
    CHECK(gear_tx_bus_index(one, 1, 1, 1200u, AGE) == 0, "single bus ignores an out-of-range preference");
    one[0] = {7, true, 0u};
    CHECK(gear_tx_bus_index(one, 1, 0, 100u, AGE) == -1, "ms 0 never counts as seen");
    one[0] = {7, false, 1000u};
    CHECK(gear_tx_bus_index(one, 1, 0, 1100u, AGE) == -1, "invalid counter: no press");
    one[0] = {7, true, 0xFFFFFF00u};
    CHECK(gear_tx_bus_index(one, 1, 0, 0x40u, AGE) == 0, "fresh across the millis() wrap");

    // T-2CAN: can1 preferred (GEAR_LEVER_TX_BUS_INDEX 1).
    GearCounterCache two[2] = {};
    two[0] = {3, true, 2000u};
    CHECK(gear_tx_bus_index(two, 2, 1, 2100u, AGE) == 0,
          "0x229 only on can0: press on can0, not the configured can1");
    two[1] = {9, true, 2050u};
    CHECK(gear_tx_bus_index(two, 2, 1, 2100u, AGE) == 1, "0x229 on both: configured can1");
    two[1].ms = 1500u;
    CHECK(gear_tx_bus_index(two, 2, 1, 2100u, AGE) == 0, "can1 gone stale, can0 live: can0");
    two[0] = {};
    CHECK(gear_tx_bus_index(two, 2, 1, 2100u, AGE) == -1, "stale on can1, never on can0: no press");
    two[1].ms = 2090u;
    CHECK(gear_tx_bus_index(two, 2, 1, 2100u, AGE) == 1, "0x229 only on can1: can1");
    CHECK(gear_tx_bus_index(two, 2, 0, 2100u, AGE) == 1, "can0 preferred but silent: can1");
    CHECK(gear_tx_bus_index(two, 2, 1, 2591u, AGE) == -1, "everything stale: no press");
}

// ── Black-box event names ────────────────────────────────────────────────────
// The name that retention drops first among `names`, as bb_drop_oldest() picks it.
static const char* bb_oldest(const char* const* names, int n) {
    const char* pick = nullptr;
    uint64_t oldest = UINT64_MAX;
    for (int i = 0; i < n; i++) {
        uint64_t o = bb_event_order(names[i]);
        if (o < oldest) { oldest = o; pick = names[i]; }
    }
    return pick;
}

static void test_blackbox_names(void) {
    char nm[40];
    bb_event_name(nm, sizeof(nm), 7u, "abort");
    CHECK(strcmp(nm, "ev000007_abort") == 0, "name: %s", nm);
    CHECK(bb_event_seq(nm) == 7u, "number read back (%u)", bb_event_seq(nm));
    bb_event_name(nm, sizeof(nm), 1234567u, "busoff");
    CHECK(strcmp(nm, "ev1234567_busoff") == 0 && bb_event_seq(nm) == 1234567u,
          "past six digits: %s", nm);
    bb_event_name(nm, sizeof(nm), 0xFFFFFFFFu, "manual");
    CHECK(bb_event_seq(nm) == 0xFFFFFFFFu && strlen(nm) < 40, "largest number fits: %s", nm);

    // Names without a number: the old evt_<millis>_ form and anything else.
    CHECK(bb_event_seq("evt_123456_abort") == 0, "old name has no number");
    CHECK(bb_event_seq("ev_12_abort") == 0 && bb_event_seq("ev12") == 0 &&
          bb_event_seq("ev12x_abort") == 0 && bb_event_seq("ev99999999999_abort") == 0 &&
          bb_event_seq("notes") == 0 && bb_event_seq("") == 0 && bb_event_seq(nullptr) == 0,
          "malformed names have no number");

    // Old names come before every numbered one, by <millis> among themselves.
    CHECK(bb_event_order("evt_4000000000_abort") < bb_event_order("ev000001_abort"),
          "a long-uptime old name is still older than event 1");
    CHECK(bb_event_order("evt_100_abort") < bb_event_order("evt_200_abort"), "old names by millis");
    CHECK(bb_event_order("ev000009_abort") < bb_event_order("ev000010_manual"), "numbers in order");
    CHECK(bb_event_order("notes") == 0, "a stray file sorts first");

    // The reported bug: five saved from a long session (evt_<big millis>), the
    // device reboots, and an event early in the new uptime came out smallest
    // and was deleted right after it was written. Numbered, it is the newest.
    const char* disk[] = {"evt_3600000_abort", "evt_3700000_abort", "evt_3800000_busoff",
                          "evt_3900000_abort", "evt_4000000_manual", "ev000001_abort"};
    CHECK(strcmp(bb_oldest(disk, 6), "evt_3600000_abort") == 0,
          "retention drops the oldest old name, not the new event (%s)", bb_oldest(disk, 6));
    const char* later[] = {"ev000002_abort", "ev000006_abort", "ev000003_busoff",
                           "ev000005_abort", "ev000004_manual", "ev000007_manual"};
    CHECK(strcmp(bb_oldest(later, 6), "ev000002_abort") == 0, "then the lowest number (%s)",
          bb_oldest(later, 6));

    // Numbering carries on from the highest on disk, however the files are listed.
    uint32_t last = 0;
    for (const char* n : later) if (bb_event_seq(n) > last) last = bb_event_seq(n);
    bb_event_name(nm, sizeof(nm), last + 1u, "abort");
    CHECK(strcmp(nm, "ev000008_abort") == 0, "next after a reboot: %s", nm);
}

// A heap with a hard byte cap, to prove the RAM black-box save (blackbox_ram.h)
// never needs two windows of memory at once (#191). Each block carries its own
// size ahead of the pointer so free() can return it to the budget, and the peak
// high-water mark lets a test assert how much was outstanding at once.
static size_t g_cap_bytes = 0;
static size_t g_outstanding = 0;
static size_t g_cap_peak = 0;
static int    g_live_allocs = 0;

static void* cap_alloc(size_t n) {
    if (g_outstanding + n > g_cap_bytes) return NULL;
    void* raw = malloc(n + sizeof(size_t));
    if (!raw) return NULL;
    *(size_t*)raw = n;
    g_outstanding += n;
    if (g_outstanding > g_cap_peak) g_cap_peak = g_outstanding;
    g_live_allocs++;
    return (char*)raw + sizeof(size_t);
}
static void cap_free(void* p) {
    if (!p) return;
    void* raw = (char*)p - sizeof(size_t);
    g_outstanding -= *(size_t*)raw;
    g_live_allocs--;
    free(raw);
}
static void cap_reset(size_t cap) {
    g_cap_bytes = cap; g_outstanding = 0; g_cap_peak = 0; g_live_allocs = 0;
}

static void test_blackbox_ram_save_mem(void) {
    // A classic-ESP32 internal-RAM window: 3000 frames at 19 B each (~57 KB),
    // the safety cap in blackbox.cpp. The no-PSRAM board #191 was filed on has
    // room for one window but not two.
    const size_t WINDOW = 3000u * 19u;
    const size_t TIGHT  = WINDOW + WINDOW / 2u;   // 1.5 windows free

    // The OLD two-copy path under the tight cap: a scratch copy of the window,
    // then the slot copy allocated while the scratch is still held. The second
    // allocation needs a second window and fails — the event is dropped (#191).
    cap_reset(TIGHT);
    void* scratch = cap_alloc(WINDOW);
    CHECK(scratch != NULL, "two-copy path: the scratch window allocates");
    void* slot_old = cap_alloc(WINDOW);   // still holding scratch
    CHECK(slot_old == NULL, "two-copy path: the second window can't be had (the #191 drop)");
    cap_free(scratch);

    // The FIX: bb_ram_reserve() frees the previous event first, then takes one
    // window, and the caller copies the ring straight in. It succeeds under the
    // same tight cap the two-copy path failed under, and the peak never passes
    // one window.
    cap_reset(TIGHT);
    BBRamSlot slot = {NULL, 0};
    void* b1 = bb_ram_reserve(&slot, WINDOW, cap_alloc, cap_free);
    CHECK(b1 != NULL && slot.buf == b1 && slot.bytes == WINDOW,
          "reserve: one window succeeds where two copies failed");
    CHECK(g_cap_peak <= WINDOW, "reserve: peak stays at one window (%zu <= %zu)",
          g_cap_peak, WINDOW);

    // A second event reuses the budget: the old buffer is freed before the new
    // one, so even back-to-back saves never need two windows.
    void* b2 = bb_ram_reserve(&slot, WINDOW, cap_alloc, cap_free);
    CHECK(b2 != NULL && slot.bytes == WINDOW,
          "reserve: a second event fits in the same budget");
    CHECK(g_live_allocs == 1, "reserve: exactly one window is live at a time (%d)",
          g_live_allocs);
    CHECK(g_cap_peak <= WINDOW, "reserve: peak still one window across saves (%zu)", g_cap_peak);
    cap_free(slot.buf);

    // A real OOM: not even one window fits. Seed a previous event first, then
    // shrink the budget below a window. The reserve reports OOM and leaves the
    // slot empty — the dashboard shows a save failure, not a stale event — and
    // the previous event is gone too (the price of a one-window peak).
    cap_reset(WINDOW);
    BBRamSlot tiny = {NULL, 0};
    void* seed = bb_ram_reserve(&tiny, WINDOW, cap_alloc, cap_free);
    CHECK(seed != NULL, "seed: a first event is stored");
    g_cap_bytes = WINDOW - 1u;   // now a window no longer fits
    void* oom = bb_ram_reserve(&tiny, WINDOW, cap_alloc, cap_free);
    CHECK(oom == NULL, "reserve: a window too large for the heap reports OOM");
    CHECK(tiny.buf == NULL && tiny.bytes == 0,
          "reserve: the slot is empty after an OOM (no stale event)");
    CHECK(g_live_allocs == 0, "reserve: the OOM freed the previous event too (nothing live)");

    // A zero-frame window still owns a valid 1-byte buffer (empty but real),
    // so the backend's frame pointer is never NULL for a kept event.
    cap_reset(64u);
    BBRamSlot empty = {NULL, 0};
    void* z = bb_ram_reserve(&empty, 0u, cap_alloc, cap_free);
    CHECK(z != NULL && empty.bytes == 1u, "reserve: an empty window still owns a buffer");
    cap_free(empty.buf);
}

// The adaptive internal-RAM ring sizing (#191). bb_ram_ring_frames() picks the
// largest ring that still leaves the WiFi/web margin free, clamped to the floor
// and the per-target save cap, or 0 to refuse when even the floor won't fit.
static void test_blackbox_ram_ring_sizing(void) {
    const size_t   FSZ    = 19u;          // sizeof(BBFrame) @ 19 B/frame
    const size_t   MARGIN = 60u * 1024u;  // BLACKBOX_HEAP_MARGIN
    const uint32_t MINF   = 800u;         // BB_MIN_INTERNAL_FRAMES
    const uint32_t MAXF   = 3000u;        // BLACKBOX_FRAMES_INTERNAL (classic)

    // Plenty of heap (e.g. an S3 or a freshly booted board): clamp to the cap,
    // never larger, so the one-window save still fits inside the margin.
    CHECK(bb_ram_ring_frames(300u * 1024u, MARGIN, FSZ, MINF, MAXF) == MAXF,
          "sizing: ample heap clamps to the per-target cap");

    // The #191 board: ~86 KB free after WiFi + web. The OLD fixed-3000 ring
    // needed 3000*19 + 60 KB = ~118 KB and so always failed; the adaptive ring
    // is sized DOWN to a non-zero count strictly between the floor and the cap.
    const size_t HEAP_191   = 86u * 1024u;
    const size_t FIXED_NEED = (size_t)MAXF * FSZ + MARGIN;
    CHECK(HEAP_191 < FIXED_NEED, "sizing: the fixed 3000 ring never fit the #191 heap");
    uint32_t f191 = bb_ram_ring_frames(HEAP_191, MARGIN, FSZ, MINF, MAXF);
    CHECK(f191 > MINF && f191 < MAXF,
          "sizing: #191 heap gives a shortened but usable ring (%lu frames)",
          (unsigned long)f191);
    CHECK((size_t)f191 * FSZ + MARGIN <= HEAP_191,
          "sizing: the chosen ring still leaves the full margin free");

    // Just above the floor: the computed count lands at the minimum (~4 s).
    CHECK(bb_ram_ring_frames(MARGIN + (size_t)MINF * FSZ + 10u, MARGIN, FSZ, MINF, MAXF) == MINF,
          "sizing: just above the floor yields the minimum ring");

    // At and below the floor: margin + floor can't both be had → refuse (0),
    // exactly as the old code refused — the board is genuinely too tight.
    CHECK(bb_ram_ring_frames(MARGIN + (size_t)MINF * FSZ, MARGIN, FSZ, MINF, MAXF) == 0u,
          "sizing: margin + floor exactly won't fit, refuse");
    CHECK(bb_ram_ring_frames(MARGIN, MARGIN, FSZ, MINF, MAXF) == 0u,
          "sizing: only the margin free, refuse");
    CHECK(bb_ram_ring_frames(0u, MARGIN, FSZ, MINF, MAXF) == 0u,
          "sizing: no heap, refuse");
}

// ── #176: stale-frame TX gate ────────────────────────────────────────────────
// A drain pass that empties a receive backlog hands process_frame() frames up to
// ~0.5 s old; a read-modify-retransmit path must emit a modified copy only from a
// fresh frame. This drives the same decision main.cpp makes for the 0x370 nag
// echo: fsd_handle_epas_status() runs for every frame (receive-side state), the
// echo is gated on fsd_tx_fresh(frame.rx_ms, now).
static void test_fresh_tx_gate(void) {
    const uint32_t now = 100000u;
    // The gate itself.
    CHECK(fsd_tx_fresh(now - 2u, now), "2 ms old is fresh");
    CHECK(fsd_tx_fresh(now - FSD_FRESH_TX_MAX_AGE_MS, now), "exactly the threshold is fresh");
    CHECK(!fsd_tx_fresh(now - (FSD_FRESH_TX_MAX_AGE_MS + 1u), now),
          "one ms past the threshold is stale");
    CHECK(!fsd_tx_fresh(now - 200u, now), "200 ms backlog is stale");
    CHECK(fsd_tx_fresh(0u, now), "unstamped (0) treated as fresh");

    FSDState s;
    memset(&s, 0, sizeof(s));
    s.nag_killer = true;
    s.das_hands_on_state = 0xFF;   // no DAS frame seen -> conservative echo

    // Eight stale 0x370 frames (counters running ahead), then one fresh frame —
    // the shape of the #176 record. Drive the nag decision per frame.
    const int kStale = 8;
    int echoes = 0, processed = 0;
    uint8_t last_cnt_out = 0xFF;
    for (int i = 0; i <= kStale; i++) {
        bool is_fresh_frame = (i == kStale);
        CanFrame f;
        memset(&f, 0, sizeof(f));
        f.id = CAN_ID_EPAS_STATUS;
        f.dlc = 8;
        f.data[2] = 0x08; f.data[3] = 0x02;   // torsionBarTorque raw 2050 = 0 Nm
        f.data[4] = 0x00;                       // handsOnLevel = 0 (nag imminent)
        f.data[6] = (uint8_t)(i & 0x0F);        // counter runs ahead across the backlog
        f.rx_ms = is_fresh_frame ? (now - 2u) : (now - 200u);

        // Receive-side state updates for every frame, stale or not.
        fsd_handle_epas_status(&s, &f);
        processed++;

        // main.cpp: build + send the echo only when the input is fresh.
        bool fresh = fsd_tx_fresh(f.rx_ms, now);
        CanFrame out;
        memset(&out, 0, sizeof(out));
        bool fired = fresh && fsd_handle_nag_killer(&s, &f, &out, now);
        if (fired) { echoes++; last_cnt_out = (uint8_t)(out.data[6] & 0x0F); }
    }
    CHECK(echoes == 1, "exactly one echo from a 9-frame backlog, got %d", echoes);
    CHECK(processed == kStale + 1, "receive-side state updated for every frame (%d)", processed);
    CHECK(s.torsion_bar_torque_seen, "epas torque parsed for every frame");
    CHECK(last_cnt_out == (uint8_t)((kStale + 1) & 0x0F),
          "the one echo carries the fresh frame's counter+1, got %u", last_cnt_out);

    // Control: with no gate (every frame treated fresh) the same backlog emits
    // nine echoes — the burst #176 reported with counters ahead of the car's.
    int ungated = 0;
    for (int i = 0; i <= kStale; i++) {
        CanFrame f; memset(&f, 0, sizeof(f));
        f.id = CAN_ID_EPAS_STATUS; f.dlc = 8;
        f.data[2] = 0x08; f.data[3] = 0x02; f.data[4] = 0x00; f.data[6] = (uint8_t)(i & 0x0F);
        CanFrame out; memset(&out, 0, sizeof(out));
        if (fsd_handle_nag_killer(&s, &f, &out, now)) ungated++;
    }
    CHECK(ungated == kStale + 1, "ungated backlog would echo every frame (%d)", ungated);

    // A lone fresh frame still echoes.
    CanFrame lone; memset(&lone, 0, sizeof(lone));
    lone.id = CAN_ID_EPAS_STATUS; lone.dlc = 8;
    lone.data[2] = 0x08; lone.data[3] = 0x02; lone.data[4] = 0x00; lone.data[6] = 0x05;
    lone.rx_ms = now - 1u;
    CanFrame lout; memset(&lout, 0, sizeof(lout));
    CHECK(fsd_tx_fresh(lone.rx_ms, now) && fsd_handle_nag_killer(&s, &lone, &lout, now),
          "a lone fresh frame still echoes");
}

// ── #176 secondary: nag echo during AP fault / driver override ────────────────
static void test_nag_fault_override(void) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = CAN_ID_EPAS_STATUS;
    f.dlc = 8;
    f.data[4] = 0x00;   // handsOnLevel = 0 (nag imminent)
    f.data[6] = 0x05;

    FSDState base;
    memset(&base, 0, sizeof(base));
    base.nag_killer = true;
    base.das_hands_on_state = 0xFF;   // conservative echo

    CanFrame out;
    FSDState s;

    // Baseline: engaged, hands off, ~0 torque -> normal nag echo fires.
    s = base; s.das_ap_state = 3; s.torsion_bar_torque_nm = 0.3f;
    memset(&out, 0, sizeof(out));
    CHECK(fsd_handle_nag_killer(&s, &f, &out, 1000u),
          "normal nag still echoes (engaged, hands-off, low torque)");

    // AP aborting / aborted -> suppressed.
    s = base; s.das_ap_state = 8; s.torsion_bar_torque_nm = 0.0f;
    CHECK(!fsd_handle_nag_killer(&s, &f, &out, 1000u), "no echo while AP ABORTING (8)");
    s = base; s.das_ap_state = 9;
    CHECK(!fsd_handle_nag_killer(&s, &f, &out, 1000u), "no echo while AP ABORTED (9)");

    // Driver clearly steering -> suppressed, either sign.
    s = base; s.das_ap_state = 3; s.torsion_bar_torque_nm = 3.2f;
    CHECK(!fsd_handle_nag_killer(&s, &f, &out, 1000u), "no echo while driver applies +3.2 Nm");
    s = base; s.das_ap_state = 3; s.torsion_bar_torque_nm = -3.2f;
    CHECK(!fsd_handle_nag_killer(&s, &f, &out, 1000u), "no echo while driver applies -3.2 Nm");

    // Boundary at FSD_NAG_DRIVER_TORQUE_NM.
    s = base; s.das_ap_state = 3; s.torsion_bar_torque_nm = 2.5f;
    CHECK(!fsd_handle_nag_killer(&s, &f, &out, 1000u), "2.5 Nm suppresses (>=)");
    s = base; s.das_ap_state = 3; s.torsion_bar_torque_nm = 2.4f;
    CHECK(fsd_handle_nag_killer(&s, &f, &out, 1000u), "2.4 Nm still echoes");

    // The shared guard predicate directly.
    CHECK(fsd_nag_suppress_fault(8, 0.0f) && fsd_nag_suppress_fault(9, 0.0f),
          "guard: abort states suppress");
    CHECK(!fsd_nag_suppress_fault(3, 0.5f), "guard: engaged + light torque allowed");
    CHECK(fsd_nag_suppress_fault(3, -2.6f), "guard: heavy torque suppresses");
}

int main() {
    printf("test_esp32_core: ESP32 firmware handler host tests\n");
    test_fresh_tx_gate();
    test_nag_fault_override();
    test_gear_tx_route();
    test_perf_accel_timer();
    test_perf_brake_timer();
    test_perf_g_estimate();
    test_perf_wheel_slip();
    test_perf_temp_band();
    test_perf_wheel_speeds_parse();
    test_accel_real_frames();
    test_accel_encode();
    test_accel_off_passthrough();
    test_accel_standstill_latch();
    test_accel_off_immediate();
    test_accel_speed_gate();
    test_accel_gear_gate();
    test_accel_bad_frames();
    test_accel_tx_gate();
    test_accel_touchscreen_change();
    test_hw4_mux2_profile_layout();
    test_hw3_speed_passthrough();
    test_hw4_ignores_hw3_override();
    test_gtw_car_state();
    test_ota_parity();
    test_ota_tx_gate();
    test_das_decode();
    test_engaged();
    test_autopark();
    test_autopark_176();
    test_signal_map();
    test_steering_angle();
    test_bms_thermal();
    test_ui_soc();
    test_di_speed();
    test_perf_speed_sna();
    test_cp_type_decode();
    test_cp_type_rewrite();
    test_cp_type_stale();
    test_state_init();
    test_can_frame_filter();
    test_blackbox_window();
    test_blackbox_ring_push();
    test_blackbox_save_slices();
    test_blackbox_slice_budget();
    test_blackbox_arm_action();
    test_blackbox_trigger_during_save();
    test_blackbox_ram_save_mem();
    test_blackbox_ram_ring_sizing();
    test_blackbox_names();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
