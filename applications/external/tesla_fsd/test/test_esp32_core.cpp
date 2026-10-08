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

#include "fsd_handler.h" // esp32/.firmware/fsd_handler.h (first on the include path)
#include "fsd_ota.h" // shared reference: fsd_ota_update()
#include "fsd_can_ops.h" // tesla_can_rx_accept / tesla_can_tx_valid (driver RX/TX filter)

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
    for(int i = 0; i < n; i++) {
        bool was = esp.tesla_ota_in_progress;
        esp32_feed(&esp, b6[i]);
        fsd_ota_update(&ref, b6[i]);
        if(diverged_at < 0 && (esp.tesla_ota_in_progress != ref.tesla_ota_in_progress ||
                               esp.ota_raw_state != ref.ota_raw_state ||
                               esp.ota_assert_count != ref.ota_assert_count ||
                               esp.ota_clear_count != ref.ota_clear_count))
            diverged_at = i;
        if(!was && esp.tesla_ota_in_progress) latches++;
    }
    CHECK(
        diverged_at < 0, "%s: ESP32 diverges from fsd_ota_update at frame %d", name, diverged_at);
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
    for(int i = 0; i < 10; i++)
        esp32_feed(&s, 0x41);
    CHECK(!s.tesla_ota_in_progress, "ESP32 OTA stable raw 1 never latches");

    // Stable raw-2 flag: latches on the 3rd repeat (frame 4), not the 2nd.
    memset(&s, 0, sizeof(s));
    for(int i = 0; i < 3; i++)
        esp32_feed(&s, 0x42);
    CHECK(!s.tesla_ota_in_progress, "ESP32 OTA 2 repeats -> not yet");
    esp32_feed(&s, 0x42);
    CHECK(s.tesla_ota_in_progress, "ESP32 OTA 3rd repeat -> latched");
    CHECK(s.ota_raw_state == 2, "ESP32 OTA raw_state 2 got %u", s.ota_raw_state);

    // Released by exactly 6 non-asserting frames.
    for(int i = 0; i < 5; i++)
        esp32_feed(&s, 0x41);
    CHECK(s.tesla_ota_in_progress, "ESP32 OTA 5 clear frames -> still latched");
    esp32_feed(&s, 0x41);
    CHECK(!s.tesla_ota_in_progress, "ESP32 OTA 6th clear frame -> released");
}

// ── ESP32 vs shared reference, frame by frame ─────────────────────────────────
static void test_ota_parity(void) {
    int run = 0, max_run = 0;
    for(size_t i = 0; i < sizeof(k_palladium_318_b6); i++) {
        run = ((k_palladium_318_b6[i] & 0x03u) == 1u) ? run + 1 : 0;
        if(run > max_run) max_run = run;
    }
    CHECK(max_run >= 3, "Palladium fixture raw-1 run %d (old ESP32 3-frame trigger)", max_run);
    CHECK(
        parity_run("Palladium", k_palladium_318_b6, (int)sizeof(k_palladium_318_b6)) == 0,
        "ESP32 OTA Palladium capture never latches");

    FSDState s;
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    bool tx_ok = true;
    for(size_t i = 0; i < sizeof(k_palladium_318_b6); i++) {
        esp32_feed(&s, k_palladium_318_b6[i]);
        if(!fsd_can_transmit(&s)) tx_ok = false;
    }
    CHECK(tx_ok, "ESP32 TX never paused by the Palladium counter");

    char name[48];
    uint8_t seq[512];

    // +2 counter 0x21..0x3F, keeping every k-th frame (2: raw pinned at 1,
    // 16: aliased to a constant 0x21).
    static const int keep_every[] = {1, 2, 3, 16};
    for(size_t k = 0; k < sizeof(keep_every) / sizeof(keep_every[0]); k++) {
        for(int i = 0; i < 128; i++)
            seq[i] = (uint8_t)(0x21 + 2 * ((i * keep_every[k]) % 16));
        snprintf(name, sizeof(name), "+2 counter keep 1/%d", keep_every[k]);
        CHECK(parity_run(name, seq, 128) == 0, "ESP32 OTA %s never latches", name);
    }

    // +2 counter under pseudo-random RX drops (fixed seed).
    for(int keep_q = 1; keep_q <= 3; keep_q++) {
        uint32_t rng = 0x31800u + (uint32_t)keep_q;
        int n = 0;
        for(int i = 0; n < 256; i++)
            if((int)(xorshift32(&rng) & 3u) < keep_q) seq[n++] = (uint8_t)(0x21 + 2 * (i % 16));
        snprintf(name, sizeof(name), "+2 counter random drops keep %d/4", keep_q);
        CHECK(parity_run(name, seq, n) == 0, "ESP32 OTA %s never latches", name);
    }

    // +1 counter kept every 4th frame: phase 2 is a constant raw 2.
    for(int phase = 0; phase < 4; phase++) {
        for(int i = 0; i < 128; i++)
            seq[i] = (uint8_t)(phase + 4 * i);
        snprintf(name, sizeof(name), "+1 counter every 4th phase %d", phase);
        CHECK(parity_run(name, seq, 128) == 0, "ESP32 OTA %s never latches", name);
    }

    // Flag latch, release, then a changed raw-2 byte restarting the count.
    static const uint8_t flag[] = {
        0x42, 0x42, 0x42, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x42, 0x42, 0x42, 0x46, 0x46, 0x46, 0x46, 0x02, 0x06, 0x0A,
    };
    CHECK(
        parity_run("flag", flag, (int)sizeof(flag)) == 2, "ESP32 OTA flag sequence latches twice");

    // Random byte6 runs (1..8 frames, biased to raw 2): every transition type.
    uint32_t rng = 0x318u;
    int n = 0;
    while(n < (int)sizeof(seq)) {
        uint8_t v = (uint8_t)xorshift32(&rng);
        if(xorshift32(&rng) & 1u) v = (uint8_t)((v & 0xFCu) | 0x02u);
        int len = 1 + (int)(xorshift32(&rng) % 8u);
        for(int j = 0; j < len && n < (int)sizeof(seq); j++)
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
    for(int i = 0; i < 4; i++)
        esp32_feed(&s, 0x42);
    CHECK(s.tesla_ota_in_progress, "stable raw-2 flag latched");
    CHECK(!fsd_can_transmit(&s), "OTA latched -> TX blocked");
    s.ignore_ota = true;
    CHECK(fsd_can_transmit(&s), "OTA latched + ignore_ota -> TX allowed");
    s.op_mode = OpMode_ListenOnly;
    CHECK(!fsd_can_transmit(&s), "listen-only blocks TX even with ignore_ota");
    s.ignore_ota = false;
    for(int i = 0; i < 6; i++)
        esp32_feed(&s, 0x41);
    CHECK(!s.tesla_ota_in_progress, "OTA released");
    CHECK(!fsd_can_transmit(&s), "listen-only blocks TX with no OTA");
    s.op_mode = OpMode_Active;
    CHECK(fsd_can_transmit(&s), "Active after release -> TX allowed");
}

// ── HW4/HW3 DAS decode: byte0 low nibble (#177) + autopark bits (#180) ────────
static void esp32_das(
    FSDState* s,
    uint8_t b0,
    uint8_t b1,
    uint8_t b3,
    void (*fn)(FSDState*, const CanFrame*)) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x39B;
    f.dlc = 8;
    f.data[0] = b0;
    f.data[1] = b1;
    f.data[3] = b3;
    f.data[5] = (uint8_t)(2u << 2); // hands-on 2 (valid, keeps das_seen meaningful)
    fn(s, &f);
}

static void test_das_decode(void) {
    FSDState s;

    // Real anoblekman Highland HW4 frames: ap_state from byte0, byte1 = 0x0A noise.
    memset(&s, 0, sizeof(s));
    esp32_das(&s, 0x01, 0x0A, 0xE0, fsd_handle_das_status_hw4); // parked, no bits
    CHECK(s.das_ap_state == 1, "hw4 parked byte0 ap_state=1 got %u", s.das_ap_state);
    CHECK(!s.ap_active, "hw4 state 1 -> ap_active false");
    CHECK(!s.autopark_ready && !s.autopark_waiting_brake, "byte3 0xE0 -> no autopark bits");

    esp32_das(&s, 0x06, 0x0A, 0xE5, fsd_handle_das_status_hw4); // autopark, byte3 0xE5
    CHECK(s.das_ap_state == 6, "hw4 autopark byte0 ap_state=6 got %u", s.das_ap_state);
    CHECK(s.ap_active, "hw4 state 6 -> ap_active true (engaged 3..6)");
    CHECK(
        s.autopark_ready && s.autopark_waiting_brake && !s.autopark_parked,
        "byte3 0xE5 -> ready+waitingForBrake");

    // #116 fixtures decode to their byte0 states; byte1 noise never changes it.
    memset(&s, 0, sizeof(s));
    esp32_das(&s, 0x02, 0x10, 0x00, fsd_handle_das_status_hw4);
    CHECK(s.das_ap_state == 2, "#116 READY byte0=2 got %u", s.das_ap_state);
    static const uint8_t noise[] = {0x6A, 0x10, 0x0A, 0xF0};
    for(unsigned i = 0; i < sizeof(noise); i++) {
        esp32_das(&s, 0x03, noise[i], 0x00, fsd_handle_das_status_hw4);
        CHECK(
            s.das_ap_state == 3,
            "byte1=0x%02X noise keeps ap_state=3 got %u",
            noise[i],
            s.das_ap_state);
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
    for(uint8_t v = 0; v <= 15; v++) {
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
    f.id = 0x257u;
    f.dlc = 8;
    f.data[1] = b1;
    f.data[2] = b2;
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
    s.autopark_ready = true;
    s.autopark_waiting_brake = true;
    ap_update(&s, 6, 1200);
    CHECK(s.autopark_episode && s.autopark_tx_block, "state 6 autopark: blocked");
    CHECK(!fsd_can_transmit(&s), "autopark blocks fsd_can_transmit");
    s.autopark_ready = false;
    s.autopark_waiting_brake = false;
    ap_update(&s, 1, 22000);
    CHECK(!s.autopark_episode && fsd_can_transmit(&s), "6->1: released");

    // FSD 2->3->6 with bits clear never blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 2, 100);
    ap_update(&s, 3, 200);
    ap_update(&s, 6, 300);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "FSD 2->3->6: no block");

    // 2->6 (missed 3) at 0 km/h blocks; stale keeps block; fresh speed >20 ENDS
    // the episode, so slowing back down in the same 6 does not re-block (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.speed_seen = true;
    s.last_speed_tick_ms = 100;
    s.vehicle_speed_kph = 0.0f;
    ap_update(&s, 2, 100);
    ap_update(&s, 6, 200);
    CHECK(s.autopark_tx_block, "2->6 missed-3 at 0 km/h: blocked");
    ap_update(&s, 6, 1500); // last speed 1.4 s ago -> stale
    CHECK(s.autopark_tx_block, "stale speed keeps block (fail safe)");
    s.vehicle_speed_kph = 25.0f;
    s.last_speed_tick_ms = 1900;
    ap_update(&s, 6, 2000);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "fresh speed >20 ends the episode");
    s.vehicle_speed_kph = 5.0f;
    s.last_speed_tick_ms = 2900;
    ap_update(&s, 6, 3000);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "5 km/h after release, same 6: no block");
    ap_update(&s, 6, 5000); // stale again
    CHECK(!s.autopark_tx_block, "stale speed after release: episode stays ended");

    // SNA speed must NOT release: raw 0xFFF (4095) decodes to 287.6 kph. A valid
    // release ends the episode, so a later SNA reading cannot re-open it (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.autopark_ready = true;
    ap_update(&s, 1, 100);
    ap_update(&s, 6, 200);
    CHECK(s.autopark_tx_block, "SNA test: episode blocked before any speed");
    ap_speed(&s, 0xF0, 0xFF, 300); // raw 0xFFF = SNA, fresh
    CHECK(
        s.vehicle_speed_kph > 287.5f && s.vehicle_speed_kph < 287.7f,
        "SNA raw 0xFFF decodes to %.2f kph (287.6)",
        s.vehicle_speed_kph);
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

    // Maneuver bit rising mid-6 at low speed blocks; autoparkReady alone does not.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 3, 100);
    ap_update(&s, 6, 200);
    CHECK(!s.autopark_tx_block, "FSD 3->6 no bits: no block");
    s.autopark_ready = true;
    ap_update(&s, 6, 250);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "autoparkReady alone mid-6: no block");
    s.autopark_parked = true;
    ap_update(&s, 6, 300);
    CHECK(s.autopark_tx_block, "maneuver bit rising mid-6: blocks");

    // ignore_ota does NOT override; listen-only still blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.ignore_ota = true;
    s.autopark_ready = true;
    ap_update(&s, 6, 200);
    CHECK(s.autopark_tx_block && !fsd_can_transmit(&s), "ignore_ota does not override autopark");
    s.op_mode = OpMode_ListenOnly;
    CHECK(!fsd_can_transmit(&s), "listen-only blocks TX");
}

// ── #176 parity: Autopark episode vs normal state-6 driving ──────────────────
// Same sequences as test_autopark_176 in test_fsd_core.c, through the ESP32
// 0x39B / 0x257 parsers and fsd_can_transmit().
static const uint8_t k_das_parked[8] = {0x01, 0x0A, 0xDF, 0xE0, 0xB0, 0x08, 0x71, 0x91};
static const uint8_t k_das_autopark[8] = {0x06, 0x0A, 0xDF, 0xE5, 0xB0, 0x08, 0x21, 0x4B};
static void ap_frame(FSDState* s, const uint8_t b[8], uint32_t now) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x39Bu;
    f.dlc = 8;
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
    ap_das(&s, 0x02, 0xE0, 50);
    ap_das(&s, 0x03, 0xE0, 100);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.autopark_tx_block, "#176 AP 3->6 at 30 km/h: no block");
    blocked = 0;
    for(uint32_t i = 0; i < 600; i++) {
        uint32_t t = 300 + i * 100;
        uint32_t ph = i % 60;
        ap_kph(&s, (float)(ph < 30 ? ph : 60 - ph), t);
        ap_das(&s, 0x06, ((i / 7) & 1u) ? 0xE1 : 0xE0, t + 10);
        if(s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(
        blocked == 0,
        "#176 city traffic in 6, autoparkReady toggling: %d blocked frames",
        blocked);

    // 2->6 (missed 3) at 40 km/h: moving at entry -> no episode, ever.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 40.0f, 100);
    ap_das(&s, 0x02, 0xE0, 150);
    ap_das(&s, 0x06, 0xE1, 200);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "2->6 at 40 km/h: no episode");
    ap_kph(&s, 0.0f, 300);
    ap_das(&s, 0x06, 0xE1, 350);
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "2->6 at 40, then stop + ready: no block");

    // maneuver_recent opens an episode entering 6 from an engaged state; a recent
    // autoparkReady does not (AP re-engaging near parked cars, not Autopark) (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_das(&s, 0x03, 0xE0, 100);
    ap_das(&s, 0x03, 0xE4, 1000); // waitingForBrake seen at t=1000
    ap_das(&s, 0x06, 0xE0, 2100); // 3->6, no bit now, recent (1.1 s)
    CHECK(s.autopark_tx_block, "maneuver_recent opens episode on 3->6");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_das(&s, 0x03, 0xE0, 100);
    ap_das(&s, 0x03, 0xE1, 1000); // autoparkReady seen while engaged
    ap_das(&s, 0x06, 0xE0, 2100); // 3->6, ready was recent
    CHECK(!s.autopark_tx_block, "autoparkReady recency on 3->6: no episode");

    // AP re-engaging from a standstill: 2->3->6 at 0..5 km/h with autoparkReady
    // set (byte3 bit0 only, no maneuver bit), never exceeding 20 km/h. Enters 6
    // from an engaged state -> not Autopark, never blocks near parked cars (#176).
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 0.0f, 100);
    ap_das(&s, 0x02, 0xE1, 150);
    ap_das(&s, 0x03, 0xE1, 200);
    ap_das(&s, 0x06, 0xE1, 300);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "2->3->6 at 0 km/h + ready: no episode");
    blocked = 0;
    for(uint32_t i = 0; i < 300; i++) {
        uint32_t t = 400 + i * 100;
        ap_kph(&s, (float)(i % 6), t);
        ap_das(&s, 0x06, ((i / 5) & 1u) ? 0xE1 : 0xE0, t + 10);
        if(s.autopark_tx_block || !fsd_can_transmit(&s)) blocked++;
    }
    CHECK(
        blocked == 0, "re-engage crawl in 6, autoparkReady toggling: %d blocked frames", blocked);

    // Entry boundary: 8.0 km/h still a standstill start, 8.08 is moving.
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

    // 2->6 at 0 km/h: blocked; > 20 ends it; 5 km/h in the same 6 stays released;
    // a maneuver bit re-opens it; leaving 6 resets.
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
    ap_das(&s, 0x06, 0xE5, 600);
    CHECK(
        s.autopark_episode && s.autopark_tx_block, "waitingForBrake mid-6 after release: blocked");
    ap_das(&s, 0x06, 0xE0, 700);
    CHECK(s.autopark_tx_block, "maneuver episode holds after the bit clears (still slow)");
    ap_kph(&s, 25.0f, 800);
    CHECK(!s.autopark_tx_block, "> 20 km/h ends the maneuver episode too");
    ap_kph(&s, 3.0f, 900);
    ap_das(&s, 0x06, 0xE2, 950);
    CHECK(s.autopark_tx_block, "autoParked mid-6 after release: blocked");
    ap_das(&s, 0x02, 0xE0, 1000);
    CHECK(!s.autopark_episode && !s.autopark_tx_block, "leaving 6 ends the episode");
    ap_kph(&s, 0.0f, 1100);
    ap_das(&s, 0x06, 0xE0, 1150);
    CHECK(s.autopark_tx_block, "new 2->6 at a standstill: blocked (fresh state-6 period)");

    // Unknown / stale / SNA speed at a 1->6 entry: fail safe, blocked.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_das(&s, 0x01, 0xE0, 100);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(!s.speed_seen && s.autopark_tx_block, "1->6, speed never seen: blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_kph(&s, 30.0f, 100);
    ap_das(&s, 0x01, 0xE0, 1500);
    ap_das(&s, 0x06, 0xE0, 2000);
    CHECK(s.autopark_tx_block, "1->6, stale 30 km/h: blocked");
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_speed(&s, 0xF0, 0xFF, 100);
    ap_das(&s, 0x01, 0xE0, 150);
    ap_das(&s, 0x06, 0xE0, 200);
    CHECK(s.autopark_tx_block, "1->6, SNA speed: blocked");

    // Real in-car Autopark (#180 trace): parked, autoparkReady rises, ~1.1 s
    // later 1->6 on 06 0A DF E5 B0 08 21 4B at 0..5 km/h held 20 s, then 6->1.
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

// ── Signal Map hardening (#100): mask-0 ignored + configured-but-absent flag ──
static void test_signal_map(void) {
    FSDState s;

    // Mask 0 means "not mapped": the field is ignored, not forced to 0.
    memset(&s, 0, sizeof(s));
    s.hw_version = TeslaHW_HW4;
    s.das_ap_state = 5;
    s.das_hands_on_state = 3;
    s.ap_active = true;
    s.cfg_das_id = 0x39B;
    s.cfg_apstate_byte = 0;
    s.cfg_apstate_shift = 0;
    s.cfg_apstate_mask = 0x00;
    s.cfg_handson_byte = 5;
    s.cfg_handson_shift = 2;
    s.cfg_handson_mask = 0x00;
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x39B;
    f.dlc = 8;
    f.data[0] = 0x02;
    f.data[5] = (uint8_t)(1u << 2);
    fsd_apply_signal_config(&s, &f, 5000u);
    CHECK(s.das_ap_state == 5, "mask-0 apstate ignored (kept 5, got %u)", s.das_ap_state);
    CHECK(
        s.das_hands_on_state == 3,
        "mask-0 hands-on ignored (kept 3, got %u)",
        s.das_hands_on_state);
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
}

// ── DI_speed decode parity with the Flipper (0x257) ───────────────────────────
static void test_di_speed(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = 0x257u;
    f.dlc = 8; // DI_speed (the handler ignores the id)
    f.data[1] = 0x10;
    f.data[2] = 0x27;
    f.data[3] = 0x42;
    fsd_handle_di_speed(&s, &f); // raw = (0x27<<4)|(0x10>>4) = 625 -> 625*0.08-40 = 10.0
    CHECK(
        s.vehicle_speed_kph > 9.99f && s.vehicle_speed_kph < 10.01f,
        "di_speed got %.3f exp 10.0",
        s.vehicle_speed_kph);
    CHECK(s.ui_speed == 0x42 && s.speed_seen, "di_speed ui_speed + speed_seen");
}

// ── state init ────────────────────────────────────────────────────────────────
static void test_state_init(void) {
    FSDState s;
    memset(&s, 0xA5, sizeof(s)); // init must not rely on a zeroed struct
    fsd_state_init(&s, TeslaHW_Unknown);
    CHECK(
        !s.tesla_ota_in_progress && !s.ota_last_valid && s.ota_last_byte6 == 0 &&
            s.ota_assert_count == 0 && s.ota_clear_count == 0,
        "init clears OTA detection state");
    CHECK(
        s.op_mode == OpMode_ListenOnly && !fsd_can_transmit(&s), "init: listen-only, TX blocked");
    CHECK(!s.hw3_speed_override, "init: hw3_speed_override default OFF (#209)");
}

// ── 0x3FD mux2 HW4 speed profile layout (#59), parity with the Flipper core ───
// Real HW4 mux2 payload: byte7 = 0x90 (bit 63 = mux2 valid, bits 60-62 = profile 1).
static void test_hw4_mux2_profile_layout(void) {
    static const uint8_t k_hw4_mux2[8] = {0x02, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90};
    for(int p = 0; p <= 4; p++) {
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
        CHECK(
            ((f.data[7] >> 4) & 0x07) == p,
            "#59 p%d: profile in bits 60-62, got %u",
            p,
            (f.data[7] >> 4) & 0x07);
        CHECK(
            (f.data[7] & 0x80) != 0,
            "#59 p%d: bit63 (mux2 valid) kept, byte7=0x%02X",
            p,
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
    s->nag_killer = true; // mux1 path on, must not leak into mux2
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
    CHECK(
        memcmp(f.data, k_mux0_bit46, 8) == 0,
        "HW3 default: mux0 = car frame + bit46 only (byte6 0x%02X)",
        f.data[6]);
    CHECK(s.speed_offset == 25, "HW3 AP offset still read: %d exp 25", s.speed_offset);
    CHECK(
        s.hw3_profile_seen && s.hw3_car_profile == 1 && s.hw3_sent_profile == 1,
        "HW3 default read-out: profile 1 -> 1");

    ap3fd(&f, k_hw3_mux2);
    CHECK(!fsd_handle_autopilot_frame(&s, &f), "HW3 default: mux2 NOT modified");
    CHECK(memcmp(f.data, k_hw3_mux2, 8) == 0, "HW3 default: mux2 payload untouched");
    CHECK(
        s.hw3_offset_seen && s.hw3_car_offset == 46 && s.hw3_sent_offset == 46,
        "HW3 default read-out: offset 46 -> 46 (got %u -> %u)",
        s.hw3_car_offset,
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
    z.dlc = 8; // old test inputs: zero mux0, profile 2
    CHECK(fsd_handle_autopilot_frame(&s, &z), "HW3 override zero mux0 modified");
    static const uint8_t k_zero_legacy[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x04, 0x00};
    CHECK(memcmp(z.data, k_zero_legacy, 8) == 0, "HW3 override zero mux0 = legacy bytes");

    ap3fd(&f, k_hw3_mux0);
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW3 override mux0 modified");
    static const uint8_t k_mux0_legacy[8] = {0x00, 0x11, 0x22, 0x46, 0x00, 0x40, 0x85, 0x80};
    CHECK(
        memcmp(f.data, k_mux0_legacy, 8) == 0,
        "HW3 override mux0 = legacy bytes (byte6 0x%02X exp 0x85)",
        f.data[6]);
    CHECK(s.hw3_car_profile == 1 && s.hw3_sent_profile == 2, "HW3 override read-out: 1 -> 2");

    ap3fd(&f, k_hw3_mux2);
    CHECK(fsd_handle_autopilot_frame(&s, &f), "HW3 override mux2 modified");
    static const uint8_t k_mux2_legacy[8] = {0x42, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
    CHECK(
        memcmp(f.data, k_mux2_legacy, 8) == 0,
        "HW3 override mux2 = legacy bytes (got %02X %02X)",
        f.data[0],
        f.data[1]);
    CHECK(s.hw3_car_offset == 46 && s.hw3_sent_offset == 25, "HW3 override read-out: 46 -> 25");

    // Follow distance: read-out always, profile write only with the override.
    CanFrame fd;
    memset(&fd, 0, sizeof(fd));
    fd.id = CAN_ID_FOLLOW_DIST;
    fd.dlc = 8;
    fd.data[5] = (uint8_t)(3u << 5); // fd3 -> profile 0
    fsd_handle_follow_distance(&s, &fd);
    CHECK(
        s.follow_distance_seen && s.follow_distance == 3,
        "fd read-out 3 got %u",
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
    for(int ov = 0; ov <= 1; ov++) {
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
        CHECK(
            memcmp(f.data, k_hw4_mux2_out, 8) == 0, "HW4 ov%d mux2 = profile 4 in bits 60-62", ov);
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
    for(uint8_t dlc = 9; dlc <= 15; dlc++)
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

int main() {
    printf("test_esp32_core: ESP32 firmware handler host tests\n");
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
    test_di_speed();
    test_state_init();
    test_can_frame_filter();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
