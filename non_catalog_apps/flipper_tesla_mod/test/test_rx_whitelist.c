/*
 * test_rx_whitelist.c — host unit tests for fsd_logic/fsd_rx_whitelist.h.
 *
 * This is the safety gate for the opt-in "RX ID whitelist (performance)" mode.
 * The derived whitelist must NEVER drop an id a currently-enabled feature's
 * handler consumes — if it does, that feature breaks silently on a real car.
 *
 * Each feature below enumerates the CAN ids its process_frame() handler
 * reads/writes EXPLICITLY (not copied from the implementation). If a future
 * change teaches a handler a new id without adding it to the whitelist tables,
 * the matching CHECK here turns red.
 *
 * Build + run:  make -C test check   (compiled by the test Makefile)
 */

#include <stdio.h>
#include <string.h>

#include "fsd_rx_whitelist.h"

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

// ── helpers ──────────────────────────────────────────────────────────────────
static bool has(const uint32_t* ids, size_t n, uint32_t id) {
    for(size_t i = 0; i < n; i++) {
        if(ids[i] == (id & 0x7FFu)) return true;
    }
    return false;
}

// Derive with the common, non-fallback arguments (blackbox flag explicit).
static bool derive(const FSDState* s, bool blackbox_on, uint32_t* ids, size_t* n) {
    return fsd_rx_whitelist_needed(
        s,
        blackbox_on,
        /*capture_all*/ false,
        /*capability*/ false,
        /*signalmap_discovery*/ false,
        ids,
        FSD_RXW_MAX_IDS,
        n);
}

// Assert that the set derived for state `s` (blackbox off) CONTAINS every id in
// `want`, so enabling the feature never filters out a frame its handler needs.
static void
    expect_contains(const char* feature, const FSDState* s, const uint32_t* want, size_t want_n) {
    uint32_t ids[FSD_RXW_MAX_IDS];
    size_t n = 0;
    bool accept_all = derive(s, false, ids, &n);
    CHECK(!accept_all, "%s: derivation unexpectedly returned accept-all", feature);
    for(size_t i = 0; i < want_n; i++) {
        CHECK(
            has(ids, n, want[i]),
            "%s: derived set is missing id 0x%03X",
            feature,
            (unsigned)want[i]);
    }
}

// The five hard essentials, per the spec, that MUST be in EVERY derived set.
static const uint32_t SPEC_ESSENTIALS[] = {0x398u, 0x318u, 0x399u, 0x39Bu, 0x370u};
#define SPEC_ESSENTIAL_N (sizeof(SPEC_ESSENTIALS) / sizeof(SPEC_ESSENTIALS[0]))

static void expect_spec_essentials(const char* where, const uint32_t* ids, size_t n) {
    for(size_t i = 0; i < SPEC_ESSENTIAL_N; i++) {
        CHECK(
            has(ids, n, SPEC_ESSENTIALS[i]),
            "%s: essential id 0x%03X missing",
            where,
            (unsigned)SPEC_ESSENTIALS[i]);
    }
}

// ── 1. default / empty state ─────────────────────────────────────────────────
static void test_default_state(void) {
    FSDState s;
    memset(&s, 0, sizeof(s)); // no features; cfg ids 0 (auto)
    uint32_t ids[FSD_RXW_MAX_IDS];
    size_t n = 0;
    bool accept_all = derive(&s, /*blackbox*/ false, ids, &n);
    CHECK(!accept_all, "default state must derive a set, not accept-all");
    CHECK(n > 0, "default state must not yield an empty set");
    expect_spec_essentials("default", ids, n);
    // All read-outs present even with no feature on.
    for(size_t i = 0; i < FSD_RXW_READOUT_COUNT; i++) {
        CHECK(
            has(ids, n, FSD_RXW_READOUT_IDS[i]),
            "default: read-out id 0x%03X missing",
            (unsigned)FSD_RXW_READOUT_IDS[i]);
    }
    // 0x488 is black-box-only: it must NOT appear when the recorder is off.
    CHECK(!has(ids, n, 0x488u), "default (black-box off): 0x488 must not be in the set");
}

// ── 2. essentials present in EVERY representative state ───────────────────────
static void test_essentials_everywhere(void) {
    FSDState s;
    // A spread of single-feature and combined states.
    for(int variant = 0; variant < 10; variant++) {
        memset(&s, 0, sizeof(s));
        switch(variant) {
        case 0:
            break; // empty
        case 1:
            s.nag_killer = true;
            break;
        case 2:
            s.fsd_unlock = true;
            s.force_fsd = true;
            break;
        case 3:
            s.track_mode_inject = true;
            break;
        case 4:
            s.tlssc_restore = true;
            break;
        case 5:
            s.accel_mode = ACCEL_MODE_PERFORMANCE;
            break;
        case 6:
            s.continuous_ap = true;
            s.soft_engage = true;
            break;
        case 7:
            s.cp_type_eu_rewrite = true;
            break;
        case 8:
            s.cfg_das_id = 0x2F0u;
            s.cfg_steer_id = 0x131u;
            break;
        case 9: // everything at once
            s.nag_killer = s.fsd_unlock = s.force_fsd = true;
            s.track_mode_inject = s.tlssc_restore = true;
            s.accel_mode = ACCEL_MODE_CHILL;
            s.continuous_ap = s.soft_engage = s.summon_unlock = true;
            s.cp_type_eu_rewrite = true;
            break;
        }
        // Both black-box states.
        for(int bb = 0; bb < 2; bb++) {
            uint32_t ids[FSD_RXW_MAX_IDS];
            size_t n = 0;
            bool accept_all = derive(&s, bb != 0, ids, &n);
            CHECK(!accept_all, "variant %d bb=%d: unexpected accept-all", variant, bb);
            expect_spec_essentials("variant", ids, n);
        }
    }
}

// ── 3. per-feature coverage — each handler's consumed ids must be present ──────
static void test_per_feature_coverage(void) {
    FSDState s;

    // nag killer → 0x370 EPAS + DAS status 0x399 / 0x39B.
    memset(&s, 0, sizeof(s));
    s.nag_killer = true;
    {
        const uint32_t w[] = {0x370u, 0x399u, 0x39Bu};
        expect_contains("nag_killer", &s, w, 3);
    }

    // FSD / AP activation → 0x3FD / 0x3EE / 0x399 / 0x39B.
    memset(&s, 0, sizeof(s));
    s.fsd_unlock = true;
    s.force_fsd = true;
    {
        const uint32_t w[] = {0x3FDu, 0x3EEu, 0x399u, 0x39Bu};
        expect_contains("fsd_ap", &s, w, 4);
    }

    // Acceleration Mode → 0x334 pedal map + 0x118 standstill gate.
    memset(&s, 0, sizeof(s));
    s.accel_mode = ACCEL_MODE_PERFORMANCE;
    {
        const uint32_t w[] = {0x334u, 0x118u, 0x257u};
        expect_contains("accel_mode", &s, w, 3);
    }

    // Track Mode inject → 0x313 (+ 0x118 for the read-back state).
    memset(&s, 0, sizeof(s));
    s.track_mode_inject = true;
    {
        const uint32_t w[] = {0x313u, 0x118u};
        expect_contains("track_mode", &s, w, 2);
    }

    // TLSSC Restore → 0x331.
    memset(&s, 0, sizeof(s));
    s.tlssc_restore = true;
    {
        const uint32_t w[] = {0x331u};
        expect_contains("tlssc", &s, w, 1);
    }

    // CP Type EU rewrite → 0x25D.
    memset(&s, 0, sizeof(s));
    s.cp_type_eu_rewrite = true;
    {
        const uint32_t w[] = {0x25Du};
        expect_contains("cp_type", &s, w, 1);
    }

    // Follow distance / driver-assist overrides → 0x3F8.
    memset(&s, 0, sizeof(s));
    s.assist_rhd_override = true;
    s.continue_on_green = true;
    {
        const uint32_t w[] = {0x3F8u};
        expect_contains("follow_assist", &s, w, 1);
    }

    // Continuous AP → gear lever 0x229, brake 0x145, stalk/AP 0x3EE, DAS 0x399.
    memset(&s, 0, sizeof(s));
    s.continuous_ap = true;
    {
        const uint32_t w[] = {0x229u, 0x145u, 0x3EEu, 0x399u};
        expect_contains("continuous_ap", &s, w, 4);
    }

    // Soft Engage → steering angle 0x129.
    memset(&s, 0, sizeof(s));
    s.soft_engage = true;
    {
        const uint32_t w[] = {0x129u};
        expect_contains("soft_engage", &s, w, 1);
    }

    // Suppress speed chime (HW4 ISA) → 0x399.
    memset(&s, 0, sizeof(s));
    s.suppress_speed_chime = true;
    {
        const uint32_t w[] = {0x399u};
        expect_contains("suppress_chime", &s, w, 1);
    }

    // Summon EU Unlock safety auto-disable reads gear lever 0x229 + speed 0x257.
    memset(&s, 0, sizeof(s));
    s.summon_unlock = true;
    {
        const uint32_t w[] = {0x229u, 0x257u};
        expect_contains("summon_unlock", &s, w, 2);
    }

    // BMS / dashboard read-outs.
    memset(&s, 0, sizeof(s));
    {
        const uint32_t w[] = {0x132u, 0x292u, 0x312u, 0x33Au, 0x219u, 0x175u};
        expect_contains("bms_readouts", &s, w, 6);
    }
}

// ── 4. black-box adds every key id (0x488 is the unique one) ──────────────────
static void test_blackbox(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    uint32_t ids[FSD_RXW_MAX_IDS];
    size_t n = 0;
    bool accept_all = derive(&s, /*blackbox*/ true, ids, &n);
    CHECK(!accept_all, "black-box on: unexpected accept-all");
    for(size_t i = 0; i < FSD_BLACKBOX_KEY_ID_COUNT; i++) {
        CHECK(
            has(ids, n, FSD_BLACKBOX_KEY_IDS[i]),
            "black-box key id 0x%03X missing",
            (unsigned)FSD_BLACKBOX_KEY_IDS[i]);
    }
    CHECK(has(ids, n, 0x488u), "black-box on: 0x488 DAS_steeringControl must be present");
}

// ── 5. accept-all fallbacks ───────────────────────────────────────────────────
static void test_fallbacks(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    uint32_t ids[FSD_RXW_MAX_IDS];
    size_t n;

    n = 99;
    CHECK(
        fsd_rx_whitelist_needed(
            &s, false, /*capture_all*/ true, false, false, ids, FSD_RXW_MAX_IDS, &n),
        "capture-all must force accept-all");
    CHECK(n == 0, "accept-all must report 0 ids (capture-all)");

    CHECK(
        fsd_rx_whitelist_needed(
            &s, false, false, /*capability*/ true, false, ids, FSD_RXW_MAX_IDS, &n),
        "capability check must force accept-all");

    CHECK(
        fsd_rx_whitelist_needed(
            &s, false, false, false, /*signalmap*/ true, ids, FSD_RXW_MAX_IDS, &n),
        "signal-map discovery must force accept-all");

    // Set exceeds a small cap -> accept-all (never a partial filter).
    uint32_t small[4];
    n = 99;
    CHECK(
        fsd_rx_whitelist_needed(&s, true, false, false, false, small, 4, &n),
        "a cap smaller than the set must force accept-all");
    CHECK(n == 0, "accept-all must report 0 ids (cap overflow)");

    // A cap exactly at FSD_RXW_MAX_IDS holds the fullest realistic set.
    memset(&s, 0, sizeof(s));
    s.nag_killer = s.fsd_unlock = s.force_fsd = true;
    s.track_mode_inject = s.tlssc_restore = true;
    s.accel_mode = ACCEL_MODE_PERFORMANCE;
    s.cfg_das_id = 0x2F0u;
    s.cfg_steer_id = 0x131u;
    n = 0;
    CHECK(!derive(&s, /*blackbox*/ true, ids, &n), "fullest set must still fit FSD_RXW_MAX_IDS");
    CHECK(n <= FSD_RXW_MAX_IDS, "derived count must not exceed the cap");
}

// ── 6. signal-map configured ids ──────────────────────────────────────────────
static void test_signal_map(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.cfg_das_id = 0x2F0u; // non-standard DAS id
    s.cfg_steer_id = 0x131u; // non-standard steering id
    {
        const uint32_t w[] = {0x2F0u, 0x131u};
        expect_contains("signal_map", &s, w, 2);
    }

    // Auto mode (0) adds nothing unexpected and stays a normal (non-fallback) set.
    memset(&s, 0, sizeof(s));
    uint32_t ids[FSD_RXW_MAX_IDS];
    size_t n = 0;
    CHECK(!derive(&s, false, ids, &n), "auto signal-map must not force accept-all");
}

// ── 7. set invariants: 11-bit, de-duplicated ─────────────────────────────────
static void test_set_wellformed(void) {
    FSDState s;
    memset(&s, 0, sizeof(s));
    s.nag_killer = s.fsd_unlock = s.track_mode_inject = s.tlssc_restore = true;
    s.cfg_das_id = 0x370u; // deliberately collides with an essential -> must dedupe
    uint32_t ids[FSD_RXW_MAX_IDS];
    size_t n = 0;
    CHECK(!derive(&s, true, ids, &n), "combo must derive a set");
    for(size_t i = 0; i < n; i++) {
        CHECK(ids[i] <= 0x7FFu, "id 0x%X is not an 11-bit id", (unsigned)ids[i]);
        for(size_t j = i + 1; j < n; j++) {
            CHECK(ids[i] != ids[j], "duplicate id 0x%03X in derived set", (unsigned)ids[i]);
        }
    }
}

int main(void) {
    test_default_state();
    test_essentials_everywhere();
    test_per_feature_coverage();
    test_blackbox();
    test_fallbacks();
    test_signal_map();
    test_set_wellformed();

    printf("test_rx_whitelist: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
