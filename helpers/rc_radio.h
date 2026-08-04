/**
 * RollCall - Sub-GHz capture worker.
 *
 * Brings up the internal CC1101 in an OOK/FM receive mode and runs the full
 * Flipper Sub-GHz decoder stack (environment + receiver + worker) against the
 * air. Every time your remote is pressed, the matching protocol decoder fires;
 * we record ONE capture per distinct press: which protocol it is, whether that
 * protocol is a rolling (dynamic) or fixed (static) code by design, and a
 * fingerprint of the decoded parcel so we can prove whether the code actually
 * changes between presses.
 *
 * Two extra listening modes ride on the same radio:
 *   - live diagnostics (RSSI + raw demodulator edge rate) so you can tell
 *     "wrong frequency" apart from "protocol not supported";
 *   - a band hunt that sweeps every candidate frequency measuring RSSI, so you
 *     can find where your fob actually transmits instead of guessing.
 *
 * Strictly listen-only. RollCall never transmits, never replays, never clones.
 */
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/view_dispatcher.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC_MAX_CAPTURES 8

/** What the decoded protocol is, by design. Reported by the SDK decoder. */
typedef enum {
    RcCodeUnknown = 0, /* protocol not recognised                */
    RcCodeStatic, /* fixed OOK code - replayable            */
    RcCodeDynamic, /* rolling code - counter/hopping parcel  */
} RcCodeClass;

/** One registered button press. Plain data only (no pointers) so it copies. */
typedef struct {
    char protocol[28]; /* "KeeLoq", "Princeton", "CAME", ...      */
    RcCodeClass cls; /* static / dynamic / unknown             */
    uint16_t bits; /* decoded bit length                     */
    uint64_t fingerprint; /* hash of the decoded parcel (Key+Cnt+..) */
    uint32_t tick; /* furi tick when captured                */
    int8_t rssi; /* dBm at the moment of the decode        */
} RcCapture;

/** Radio band presented in Settings and swept by the band hunt. */
typedef struct {
    uint32_t frequency; /* Hz          */
    const char* label; /* "433.92"    */
} RcBand;

#define RC_BAND_COUNT 14
extern const RcBand rc_bands[RC_BAND_COUNT];

/** Index of 433.92 MHz in rc_bands - the default and the safe fallback. */
#define RC_BAND_DEFAULT 9

/** Modulation preset presented in Settings (index -> FuriHalSubGhzPreset). */
typedef struct {
    const char* label; /* "AM650"                 */
    uint8_t preset; /* FuriHalSubGhzPreset enum */
} RcMod;

#define RC_MOD_COUNT 4
extern const RcMod rc_mods[RC_MOD_COUNT];

/** One band's result from the hunt sweep. */
typedef struct {
    int8_t floor_dbm; /* quietest reading seen on this band    */
    int8_t peak_dbm; /* loudest reading seen on this band     */
    int8_t last_dbm; /* most recent reading (for the live bar) */
    bool seen; /* has this band been sampled at all?    */
} RcHuntBand;

/** A band must beat its own noise floor by this much to count as a hit. */
#define RC_HUNT_MIN_DELTA_DB 10

typedef struct RcRadio RcRadio;

/**
 * @param view_dispatcher  where to post progress from the worker thread
 * @param capture_event    custom-event id to post when a new press registers.
 *                         The app owns this id; the radio never invents one.
 */
RcRadio* rc_radio_alloc(ViewDispatcher* view_dispatcher, uint32_t capture_event);
void rc_radio_free(RcRadio* radio);

/** Tune before starting. freq in Hz, preset is a FuriHalSubGhzPreset value. */
void rc_radio_configure(RcRadio* radio, uint32_t frequency, uint8_t preset);

/** Collapse window: presses closer together than this are treated as one. */
void rc_radio_set_press_gap(RcRadio* radio, uint32_t gap_ms);

/** Begin / end decoding. start() clears the capture log. */
void rc_radio_start(RcRadio* radio);
void rc_radio_stop(RcRadio* radio);
bool rc_radio_is_running(RcRadio* radio);

/** How many distinct presses have been registered so far. */
uint8_t rc_radio_count(RcRadio* radio);

/** Copy the capture log out for analysis / drawing. Returns the count. */
uint8_t rc_radio_snapshot(RcRadio* radio, RcCapture* out, uint8_t max);

/* ------------------------------------------------------- diagnostics ----- */

/** Current carrier strength in dBm. Valid while decoding or hunting. */
float rc_radio_rssi(RcRadio* radio);

/**
 * Raw level transitions the demodulator has produced since start(). Rising
 * fast means the radio IS hearing something on this band even if no protocol
 * decoder claimed it - which is exactly how you tell "wrong frequency" from
 * "unsupported protocol".
 */
uint32_t rc_radio_edges(RcRadio* radio);

/* ---------------------------------------------------------- band hunt ---- */

/**
 * Sweep every band in rc_bands on a worker thread, sampling RSSI. Hold your
 * remote down while this runs: the band it transmits on climbs far above its
 * own noise floor. Mutually exclusive with rc_radio_start().
 */
void rc_radio_hunt_start(RcRadio* radio);
void rc_radio_hunt_stop(RcRadio* radio);
bool rc_radio_hunt_is_running(RcRadio* radio);

/** Copy the per-band results out for drawing. Returns how many were written. */
uint8_t rc_radio_hunt_snapshot(RcRadio* radio, RcHuntBand* out, uint8_t max);

/** Full sweeps completed so far - drives the "keep holding" progress hint. */
uint32_t rc_radio_hunt_sweeps(RcRadio* radio);

/**
 * Index into rc_bands of the band with the biggest peak-over-floor delta, or
 * -1 if nothing beat RC_HUNT_MIN_DELTA_DB. Never guesses: no signal, no answer.
 */
int8_t rc_radio_hunt_best(RcRadio* radio);

#ifdef __cplusplus
}
#endif
