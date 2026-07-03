#pragma once
/*
 * fsd_profile_db.h — built-in variant profiles + auto-suggest matcher (#126).
 *
 * Extends the Signal Map (#122): a small table of known 0x39B/0x399 DAS layouts
 * so a hand-typed signal map isn't the only way onto an odd variant. The ESP32
 * runs the matcher ONLY when the standard parser is failing (AP engaged but
 * das_ap_state stuck/unreadable, #125's "0x39B present but AP-state unreadable"),
 * and on a single confident match surfaces a dashboard suggestion. The user taps
 * "apply" -> the existing sig_cfg path pre-fills the Signal Map. NEVER silent
 * auto-apply: a false profile is worse than no profile, so the bar is exactly one
 * qualifying profile or nothing.
 *
 * The real gap this earns its keep on: ssw0209's byte0 HI-nibble (0x39B, shift4).
 * The standard fsd_handle_das_status_hw4 parser already auto-adapts to Highland's
 * byte0 LOW-nibble via its das_hw4_use_byte0 latch (#116) — that variant is NOT a
 * gap and its profile is present only as a disambiguation candidate. A match
 * against a std / auto-handled profile means the parser is fine: no suggestion.
 *
 * Pure / header-only (static inline), mirroring fsd_capability.h — the match
 * logic lives in ONE place the host tests exercise directly.
 */

#include <stdbool.h>
#include <stdint.h>

// A DAS field position: value = (data[byte] >> shift) & mask.
typedef struct {
    uint8_t byte;
    uint8_t shift;
    uint8_t mask;
} FSDProfileField;

// A built-in variant profile. handson is byte5/shift2/mask0xF in every known
// variant; kept explicit so a future odd trim can override it.
typedef struct {
    const char* name;
    uint16_t das_id;
    FSDProfileField apstate;
    FSDProfileField handson;
    // false only for the genuinely-unhandled variant the standard parser can't
    // read (ssw0209). true for layouts the std parser / byte0 latch already
    // handle — those rows exist purely to disambiguate the matcher: a match
    // against one of them proves the parser is fine, so no suggestion is made.
    bool needs_override;
} FSDProfile;

// Seed table (verified against fsd_handle_das_status_hw3 / hw4). Order is stable;
// the matcher reports an index into this table.
static const FSDProfile FSD_PROFILE_DB[] = {
    // name                     das_id  apstate{b,s,m}   handson{b,s,m}   needs_override
    {"Standard HW3/Legacy", 0x399, {0, 0, 0x0F}, {5, 2, 0x0F}, false},
    {"Standard HW4", 0x39B, {1, 4, 0x0F}, {5, 2, 0x0F}, false},
    {"ssw0209 (byte0 hi)", 0x39B, {0, 4, 0x0F}, {5, 2, 0x0F}, true},
    {"Highland (byte0 lo)", 0x39B, {0, 0, 0x0F}, {5, 2, 0x0F}, false},
};

#define FSD_PROFILE_DB_COUNT ((int)(sizeof(FSD_PROFILE_DB) / sizeof(FSD_PROFILE_DB[0])))

// Tunables (conservative — a false single match is the failure we most fear).
#define FSD_PROFILE_MIN_FRAMES 3 // need at least this many DAS frames to decide
#define FSD_PROFILE_STATE_MAX  9 // valid DAS_autopilotState range is 0..9
#define FSD_PROFILE_ACTIVE_MIN 2 // >=2 == ACTIVE_NOMINAL (AP engaged)
#define FSD_PROFILE_MAX_FRAMES 16 // ring size the ESP32 layer feeds in

// One captured DAS frame's payload.
typedef struct {
    uint8_t data[8];
    uint8_t len;
} FSDProfileFrame;

typedef enum {
    FSD_MATCH_NONE = 0, // zero profiles qualify -> fall back to manual Signal Map
    FSD_MATCH_ONE, // exactly one qualifies -> a candidate to suggest
    FSD_MATCH_AMBIGUOUS, // >=2 qualify -> ambiguous, fall back to manual
} FSDMatchStatus;

typedef struct {
    FSDMatchStatus status;
    int index; // index into FSD_PROFILE_DB when status==ONE, else -1
} FSDMatchResult;

// Decode a field from one frame. Returns 0xFF when the byte is out of the frame.
static inline uint8_t fsd_profile_decode(FSDProfileField f, const FSDProfileFrame* fr) {
    if(f.byte >= fr->len) return 0xFF;
    return (uint8_t)((fr->data[f.byte] >> f.shift) & f.mask);
}

/* Does this profile's AP-state field decode as a live, engaged DAS state across
 * the frame history?  A profile qualifies only when, over every frame:
 *   - the field is present and decodes in-range (0..9) — an out-of-range value
 *     means we're reading the wrong nibble, disqualify immediately;
 *   - the value is not stuck at a single constant (>=2 distinct values) — a
 *     pinned nibble is not a live state field (this is what rejects Highland's
 *     byte0-lo / std-HW4's byte1 when they're pinned on a ssw0209 capture);
 *   - it reaches an active value (>=2) at least once — AP was engaged in the
 *     window (the "reaches active while AP is engaged" rule; on a genuinely
 *     parked car no field reaches active, so nothing is suggested).
 */
static inline bool
    fsd_profile_qualifies(const FSDProfile* p, const FSDProfileFrame* frames, int n) {
    if(n < FSD_PROFILE_MIN_FRAMES) return false;
    bool seen[FSD_PROFILE_STATE_MAX + 1] = {false};
    int distinct = 0;
    bool reached_active = false;
    for(int i = 0; i < n; i++) {
        if(p->apstate.byte >= frames[i].len) return false; // field must be present
        uint8_t v =
            (uint8_t)((frames[i].data[p->apstate.byte] >> p->apstate.shift) & p->apstate.mask);
        if(v > FSD_PROFILE_STATE_MAX) return false; // out of range -> wrong nibble
        if(!seen[v]) {
            seen[v] = true;
            distinct++;
        }
        if(v >= FSD_PROFILE_ACTIVE_MIN) reached_active = true;
    }
    return distinct >= 2 && reached_active;
}

/* Match a DAS frame history against the built-in profiles for the DAS id present.
 * Only profiles whose das_id equals `das_id` are considered. Exactly one
 * qualifying profile -> FSD_MATCH_ONE (+ its index); zero -> NONE; two or more ->
 * AMBIGUOUS. NONE and AMBIGUOUS both mean "no suggestion, fall back to manual".
 */
static inline FSDMatchResult
    fsd_profile_match(uint16_t das_id, const FSDProfileFrame* frames, int n) {
    FSDMatchResult r = {FSD_MATCH_NONE, -1};
    if(n < FSD_PROFILE_MIN_FRAMES) return r;
    int qualifying = 0, idx = -1;
    for(int i = 0; i < FSD_PROFILE_DB_COUNT; i++) {
        if(FSD_PROFILE_DB[i].das_id != das_id) continue;
        if(fsd_profile_qualifies(&FSD_PROFILE_DB[i], frames, n)) {
            qualifying++;
            idx = i;
        }
    }
    if(qualifying == 1) {
        r.status = FSD_MATCH_ONE;
        r.index = idx;
    } else if(qualifying >= 2) {
        r.status = FSD_MATCH_AMBIGUOUS;
    }
    return r;
}

/* Should the dashboard raise a suggestion for this match?  Only when exactly one
 * profile qualified AND it is a variant the standard parser can't already read
 * (needs_override). A unique match against a std / auto-handled layout means the
 * parser is fine — surfacing it would just be noise. */
static inline bool fsd_profile_should_suggest(FSDMatchResult r) {
    return r.status == FSD_MATCH_ONE && r.index >= 0 && r.index < FSD_PROFILE_DB_COUNT &&
           FSD_PROFILE_DB[r.index].needs_override;
}
