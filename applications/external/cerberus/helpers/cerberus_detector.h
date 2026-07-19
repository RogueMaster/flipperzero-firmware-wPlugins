/**
 * Cerberus - threat detection engine.
 *
 * Pure, radio-agnostic logic: it is fed a stream of RSSI samples (dBm) for a
 * single band together with a monotonic millisecond clock, and decides whether
 * that band is currently under one of three kinds of attack:
 *
 *   - JAMMING : the channel is held continuously busy (energy never drops back
 *               to the noise floor) for a sustained period.
 *   - FLOOD   : an abnormal number of short transmissions (bursts) per second.
 *   - REPLAY  : the same burst "fingerprint" (duration + amplitude) repeating
 *               far more often than legitimate traffic would - the signature of
 *               a captured-and-replayed or brute-forced remote.
 *
 * The engine keeps an adaptive noise-floor estimate per band so it works in any
 * RF environment without manual calibration. See README.md for the rationale
 * and the (honest) limitations of these heuristics.
 */
#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CerberusThreatNone = 0,
    CerberusThreatJam,
    CerberusThreatFlood,
    CerberusThreatReplay,
} CerberusThreat;

/** Human-readable threat name, e.g. "JAMMING". */
const char* cerberus_threat_name(CerberusThreat threat);
/** Short threat tag for the cramped status bar, e.g. "JAM". */
const char* cerberus_threat_short(CerberusThreat threat);

#define CERBERUS_BURST_RING 48
#define CERBERUS_FP_RING    24

/** Per-band detector state. Owned and mutated only by the radio worker thread. */
typedef struct {
    float floor; // adaptive noise floor (dBm)
    float peak; // decaying peak hold (dBm)
    float occupancy; // EMA of "channel busy" in [0..1]

    bool busy; // currently above the detection threshold
    uint32_t burst_start; // tick when the current burst began
    float burst_peak; // peak RSSI within the current burst
    uint32_t high_occ_since; // tick occupancy first crossed the jam line (0 = clear)

    uint32_t burst_times[CERBERUS_BURST_RING]; // ring of recent burst start ticks
    uint8_t burst_head;

    uint16_t fp_ring[CERBERUS_FP_RING]; // ring of recent burst fingerprints
    uint8_t fp_head;

    CerberusThreat threat; // threat currently shown for this band
    uint32_t threat_until; // tick at which the shown threat decays
    uint32_t cooldown_until; // tick before which no new alert may be raised

    uint16_t bursts_per_sec; // rolling burst rate (for the UI)
} CerberusDetector;

/** Tunables derived from the user's sensitivity setting. */
typedef struct {
    float margin_db; // how far above the floor counts as "busy"
    uint16_t flood_rate; // bursts/sec that trips the flood alert
    uint32_t jam_ms; // sustained busy time that trips the jam alert
    uint8_t replay_repeats; // identical fingerprints that trip the replay alert
    uint32_t cooldown_ms; // minimum gap between alerts on one band
    uint32_t display_ms; // how long a threat stays lit after it clears
} CerberusDetectorParams;

void cerberus_detector_init(CerberusDetector* d);

/** Recalibrate: forget the learned noise floor and history for this band. */
void cerberus_detector_reset_floor(CerberusDetector* d);

/** Fill @p with the parameters for sensitivity 0 (low), 1 (medium) or 2 (high). */
void cerberus_detector_set_sensitivity(CerberusDetectorParams* p, int sensitivity);

/**
 * Feed one RSSI sample. Returns the threat that was *newly raised* by this
 * sample (edge-triggered, after cooldown) so the caller can fire an alert, or
 * CerberusThreatNone if nothing new happened. d->threat / d->bursts_per_sec are
 * updated for display regardless of the return value.
 */
CerberusThreat cerberus_detector_feed(
    CerberusDetector* d,
    const CerberusDetectorParams* p,
    float rssi,
    uint32_t now,
    bool detect_jam,
    bool detect_flood,
    bool detect_replay);

#ifdef __cplusplus
}
#endif
