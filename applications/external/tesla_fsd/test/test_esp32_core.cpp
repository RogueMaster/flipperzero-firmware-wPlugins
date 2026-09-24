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

    // 2->6 (missed 3) at 0 km/h blocks; fresh speed >20 releases; stale keeps block.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    s.speed_seen = true;
    s.last_speed_tick_ms = 100;
    s.vehicle_speed_kph = 0.0f;
    ap_update(&s, 2, 100);
    ap_update(&s, 6, 200);
    CHECK(s.autopark_tx_block, "2->6 missed-3 at 0 km/h: blocked");
    s.vehicle_speed_kph = 25.0f;
    s.last_speed_tick_ms = 900;
    ap_update(&s, 6, 1000);
    CHECK(!s.autopark_tx_block, "fresh speed >20 releases");
    s.last_speed_tick_ms = 1000; // stale relative to now
    ap_update(&s, 6, 3200);
    CHECK(s.autopark_tx_block, "stale speed keeps block (fail safe)");

    // SNA speed must NOT release: raw 0xFFF (4095) decodes to 287.6 kph.
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
    ap_speed(&s, 0xD0, 0x32, 400); // raw 813 = 25.04 kph, fresh
    CHECK(!s.autopark_tx_block && fsd_can_transmit(&s), "fresh valid 25 kph releases");
    ap_speed(&s, 0xF0, 0xFF, 500); // back to SNA
    CHECK(s.autopark_episode && s.autopark_tx_block, "back to SNA re-blocks mid-episode");
    ap_speed(&s, 0xE0, 0xFD, 600); // raw 4062 = 284.96, max valid
    CHECK(!s.autopark_tx_block, "raw 4062 (max valid 284.96 kph) releases");
    ap_speed(&s, 0xF0, 0xFD, 700); // raw 4063 = 285.04, invalid
    CHECK(s.autopark_tx_block, "raw 4063 (above max valid) keeps the block");

    // autopark bit rising mid-episode at low speed blocks.
    memset(&s, 0, sizeof(s));
    s.op_mode = OpMode_Active;
    ap_update(&s, 3, 100);
    ap_update(&s, 6, 200);
    CHECK(!s.autopark_tx_block, "FSD 3->6 no bits: no block");
    s.autopark_parked = true;
    ap_update(&s, 6, 300);
    CHECK(s.autopark_tx_block, "bit rising mid-6: blocks");

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
}

int main() {
    printf("test_esp32_core: ESP32 firmware handler host tests\n");
    test_gtw_car_state();
    test_ota_parity();
    test_ota_tx_gate();
    test_das_decode();
    test_engaged();
    test_autopark();
    test_signal_map();
    test_di_speed();
    test_state_init();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
