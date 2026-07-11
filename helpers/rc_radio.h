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
} RcCapture;

/** Radio band presented in Settings. */
typedef struct {
    uint32_t frequency; /* Hz          */
    const char* label; /* "433.92"    */
} RcBand;

#define RC_BAND_COUNT 6
extern const RcBand rc_bands[RC_BAND_COUNT];

/** Modulation preset presented in Settings (index -> FuriHalSubGhzPreset). */
typedef struct {
    const char* label; /* "AM650"                 */
    uint8_t preset; /* FuriHalSubGhzPreset enum */
} RcMod;

#define RC_MOD_COUNT 4
extern const RcMod rc_mods[RC_MOD_COUNT];

/** Event id posted to the ViewDispatcher when a new press is registered. */
#define RC_EVENT_CAPTURE 0x2001U

typedef struct RcRadio RcRadio;

RcRadio* rc_radio_alloc(ViewDispatcher* view_dispatcher);
void rc_radio_free(RcRadio* radio);

/** Tune before starting. freq in Hz, preset is a FuriHalSubGhzPreset value. */
void rc_radio_configure(RcRadio* radio, uint32_t frequency, uint8_t preset);

/** Begin / end listening. start() clears the capture log. */
void rc_radio_start(RcRadio* radio);
void rc_radio_stop(RcRadio* radio);
bool rc_radio_is_running(RcRadio* radio);

/** How many distinct presses have been registered so far. */
uint8_t rc_radio_count(RcRadio* radio);

/** Copy the capture log out for analysis / drawing. Returns the count. */
uint8_t rc_radio_snapshot(RcRadio* radio, RcCapture* out, uint8_t max);

#ifdef __cplusplus
}
#endif
