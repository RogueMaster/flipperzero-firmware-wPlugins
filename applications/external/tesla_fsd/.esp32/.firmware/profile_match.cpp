/*
 * profile_match.cpp — built-in variant-profile auto-suggest (ESP32, #126).
 *
 * Rings recent 0x39B frames and, when the standard parser looks stuck, runs the
 * pure matcher in fsd_logic/fsd_profile_db.h. Read-only — never transmits.
 */

#include "profile_match.h"
#include "config.h"
#include "../../fsd_logic/fsd_profile_db.h"

// Only 0x39B is tracked: it is where the genuinely-unhandled ssw0209 hi-nibble
// variant lives. On HW3/Legacy the standard 0x399 parser is fine, and on HW4
// 0x399 is the ISA chime (no DAS state) — tracking either would only add noise.
#define PM_TRACK_ID   CAN_ID_DAS_STATUS_HW4  // 0x39B
#define PM_WINDOW_MS  4000u   // sustained observation before deciding
#define PM_FRESH_MS   2000u   // a gap longer than this restarts the window
#define PM_RING       FSD_PROFILE_MAX_FRAMES

static portMUX_TYPE  g_pm_mux    = portMUX_INITIALIZER_UNLOCKED;
static FSDState*     g_state     = nullptr;
static portMUX_TYPE* g_state_mux = nullptr;

static FSDProfileFrame g_ring[PM_RING];
static int      g_ring_len  = 0;   // frames stored (caps at PM_RING)
static int      g_ring_head = 0;   // next write slot
static uint32_t g_first_ms  = 0;   // window start
static uint32_t g_last_ms   = 0;   // most recent 0x39B frame
static bool     g_active_seen = false;  // std das_ap_state >= 2 seen this window
static bool     g_nag_seen    = false;  // hands_on >= 1 seen (AP-engaged tell)

void profile_match_init(FSDState* state, portMUX_TYPE* state_mux) {
    g_state     = state;
    g_state_mux = state_mux;
    portENTER_CRITICAL(&g_pm_mux);
    g_ring_len = 0; g_ring_head = 0; g_first_ms = 0; g_last_ms = 0;
    g_active_seen = false; g_nag_seen = false;
    portEXIT_CRITICAL(&g_pm_mux);
}

void profile_match_record(const CanFrame& frame, uint8_t std_ap_state,
                          uint8_t hands_on, uint32_t now_ms) {
    if (frame.id != PM_TRACK_ID) return;
    portENTER_CRITICAL(&g_pm_mux);
    // Fresh start on the first frame or after a gap (car woke / stream resumed).
    if (g_ring_len == 0 || (uint32_t)(now_ms - g_last_ms) > PM_FRESH_MS) {
        g_ring_len = 0; g_ring_head = 0; g_first_ms = now_ms;
        g_active_seen = false; g_nag_seen = false;
    }
    g_last_ms = now_ms;
    FSDProfileFrame f = {};
    uint8_t n = frame.dlc; if (n > 8) n = 8;
    for (uint8_t i = 0; i < n; i++) f.data[i] = frame.data[i];
    f.len = n;
    g_ring[g_ring_head] = f;
    g_ring_head = (g_ring_head + 1) % PM_RING;
    if (g_ring_len < PM_RING) g_ring_len++;
    if (std_ap_state >= FSD_PROFILE_ACTIVE_MIN) g_active_seen = true;
    if (hands_on >= 1) g_nag_seen = true;
    portEXIT_CRITICAL(&g_pm_mux);
}

String profile_match_json() {
    // Snapshot the ring + flags in chronological order under the lock.
    FSDProfileFrame frames[PM_RING];
    int n; uint32_t first, last; bool active_seen, nag_seen;
    portENTER_CRITICAL(&g_pm_mux);
    n = g_ring_len;
    for (int i = 0; i < n; i++) {
        int idx = ((g_ring_head - n + i) % PM_RING + PM_RING) % PM_RING;
        frames[i] = g_ring[idx];
    }
    first = g_first_ms; last = g_last_ms;
    active_seen = g_active_seen; nag_seen = g_nag_seen;
    portEXIT_CRITICAL(&g_pm_mux);

    bool das_cfg = false;
    if (g_state && g_state_mux) {
        portENTER_CRITICAL(g_state_mux);
        das_cfg = (g_state->cfg_das_id != 0);
        portEXIT_CRITICAL(g_state_mux);
    }

    // "Parser is failing" heuristic (conservative — a false suggestion is worse
    // than none):
    //   - no custom signal map is set yet (config owns the mapping once it is),
    //   - >= FSD_PROFILE_MIN_FRAMES 0x39B frames over a >= PM_WINDOW_MS window,
    //     still fresh,
    //   - the standard parser NEVER read AP-state active (>=2) in that window,
    //   - yet the hands-on nag escalated (>=1) -> AP is engaged, so the state
    //     the std parser can't see is real. This is #125's "0x39B present but
    //     AP-state unreadable".
    // Only then run the matcher, and only suggest a single override-needing hit.
    uint32_t now = millis();
    bool suggest = false;
    int  idx = -1;
    if (!das_cfg && n >= FSD_PROFILE_MIN_FRAMES &&
        (uint32_t)(now - last) <= PM_FRESH_MS &&
        (uint32_t)(last - first) >= PM_WINDOW_MS &&
        !active_seen && nag_seen) {
        FSDMatchResult r = fsd_profile_match(PM_TRACK_ID, frames, n);
        if (fsd_profile_should_suggest(r)) { suggest = true; idx = r.index; }
    }

    String j;
    j.reserve(176);
    j  = "{\"suggest\":";
    j += suggest ? "true" : "false";
    if (suggest && idx >= 0) {
        const FSDProfile* p = &FSD_PROFILE_DB[idx];
        j += ",\"name\":\""; j += p->name; j += "\"";
        j += ",\"das_id\":"; j += (int)p->das_id;
        j += ",\"apb\":";    j += p->apstate.byte;
        j += ",\"aps\":";    j += p->apstate.shift;
        j += ",\"apm\":";    j += p->apstate.mask;
        j += ",\"hob\":";    j += p->handson.byte;
        j += ",\"hos\":";    j += p->handson.shift;
        j += ",\"hom\":";    j += p->handson.mask;
    }
    j += "}";
    return j;
}
