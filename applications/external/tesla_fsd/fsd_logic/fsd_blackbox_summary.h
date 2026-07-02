#pragma once
/*
 * fsd_blackbox_summary.h — pure JSON-summary formatter for the black-box
 * incident recorder (#124).
 *
 * The ESP32 black-box writes two files per event: a pure candump .log (via the
 * shared tesla_format_candump_line) and a decoded .json summary. The decode is
 * the hand-analysis that used to be done on testers' logs by eye — trigger +
 * state transition, HW, firmware-era guess, buses seen, active toggles and a
 * das_ap_state mini-timeline. Keeping it here, header-only and pure (formats
 * into the caller's buffer, no I/O), lets the host tests exercise the exact
 * bytes the firmware emits and keeps the firmware itself thin.
 *
 * Returns the number of bytes written (excluding the trailing NUL). Output is
 * always NUL-terminated.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* trigger; // "ABORT" / "BUSOFF" / "MANUAL"
    uint8_t from_state; // das_ap_state at the transition source (evt_last_from)
    uint8_t to_state; // das_ap_state after the transition (evt_last_to)
    uint32_t trigger_rel_ms; // trigger time relative to the capture window start
    uint32_t window_pre_ms; // pre-roll window (ms)
    uint32_t window_post_ms; // post-roll window (ms)
    uint32_t frame_count; // frames written to the .log
    int hw_version; // TeslaHWVersion (0 unknown, 1 legacy, 2 HW3, 3 HW4)
    bool hw4_das_status_seen; // 0x39B 8-byte DAS_status seen (fw-era hint)
    bool dual_can; // device tapped two CAN controllers
    uint32_t bus0_frames; // frames seen on can0 in the window
    uint32_t bus1_frames; // frames seen on can1 in the window
    bool nag; // active toggles at capture time
    bool ap_first;
    bool abort_guard;
    bool signal_map; // configurable signal mapping in use (cfg_das_id != 0)
    bool nag_burst;
    const uint32_t* tl_ts; // das_ap_state timeline: rel-ms per entry
    const uint8_t* tl_state; // das_ap_state per entry
    int tl_count; // timeline entry count
} FSDBlackboxSummary;

static inline const char* fsd_blackbox_hw_name(int hw) {
    switch(hw) {
    case 1:
        return "Legacy";
    case 2:
        return "HW3";
    case 3:
        return "HW4";
    default:
        return "Unknown";
    }
}

// Best-effort firmware-era guess from HW + DAS_status shape. Not authoritative
// (no firmware-version signal exists on CAN) — a hint for triage, marked guess.
static inline const char* fsd_blackbox_fw_era(int hw, bool hw4_das_status_seen) {
    if(hw == 3) return hw4_das_status_seen ? "HW4 / post-14.x guess" : "HW4";
    if(hw == 2) return "HW3 / pre-14.x typical";
    if(hw == 1) return "Legacy";
    return "unknown";
}

static inline int fsd_blackbox_format_json(char* out, int out_sz, const FSDBlackboxSummary* s) {
    if(out_sz <= 0) return 0;
    out[0] = '\0';
    int pos = 0;

#define BB_APPEND(...)                                                         \
    do {                                                                       \
        if(pos < out_sz - 1) {                                                 \
            int _n = snprintf(out + pos, (size_t)(out_sz - pos), __VA_ARGS__); \
            if(_n < 0) {                                                       \
                out[pos] = '\0';                                               \
                return pos;                                                    \
            }                                                                  \
            pos += _n;                                                         \
            if(pos > out_sz - 1) pos = out_sz - 1;                             \
        }                                                                      \
    } while(0)

    const char* trig = s->trigger ? s->trigger : "?";
    uint32_t sec = s->trigger_rel_ms / 1000u;
    uint32_t ms3 = s->trigger_rel_ms % 1000u;

    BB_APPEND("{\"trigger\":\"%s\",", trig);
    BB_APPEND("\"transition\":\"%u->%u\",", s->from_state, s->to_state);
    BB_APPEND(
        "\"detail\":\"%s %u->%u @ t=%lu.%03lus\",",
        trig,
        s->from_state,
        s->to_state,
        (unsigned long)sec,
        (unsigned long)ms3);
    BB_APPEND("\"trigger_ms\":%lu,", (unsigned long)s->trigger_rel_ms);
    BB_APPEND(
        "\"window\":{\"pre_ms\":%lu,\"post_ms\":%lu},",
        (unsigned long)s->window_pre_ms,
        (unsigned long)s->window_post_ms);
    BB_APPEND("\"frames\":%lu,", (unsigned long)s->frame_count);
    BB_APPEND("\"hw\":\"%s\",", fsd_blackbox_hw_name(s->hw_version));
    BB_APPEND("\"fw_era\":\"%s\",", fsd_blackbox_fw_era(s->hw_version, s->hw4_das_status_seen));
    BB_APPEND(
        "\"buses\":{\"dual_can\":%s,\"can0\":%lu,\"can1\":%lu},",
        s->dual_can ? "true" : "false",
        (unsigned long)s->bus0_frames,
        (unsigned long)s->bus1_frames);
    BB_APPEND(
        "\"toggles\":{\"nag\":%s,\"ap_first\":%s,\"abort_guard\":%s,"
        "\"signal_map\":%s,\"nag_burst\":%s},",
        s->nag ? "true" : "false",
        s->ap_first ? "true" : "false",
        s->abort_guard ? "true" : "false",
        s->signal_map ? "true" : "false",
        s->nag_burst ? "true" : "false");
    BB_APPEND("\"ap_timeline\":[");
    for(int i = 0; i < s->tl_count; i++) {
        BB_APPEND(
            "%s{\"t\":%lu,\"s\":%u}", i ? "," : "", (unsigned long)s->tl_ts[i], s->tl_state[i]);
    }
    BB_APPEND("]}");

#undef BB_APPEND
    return pos;
}
