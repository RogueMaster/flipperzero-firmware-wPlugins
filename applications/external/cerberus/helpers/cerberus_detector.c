#include "cerberus_detector.h"

#define FLOOR_MIN   (-120.0f)
#define FLOOR_MAX   (-50.0f)
#define JAM_OCC     (0.82f) // occupancy above this == channel "held"
#define FLOOR_DROP  (0.30f) // how fast the floor follows a lower reading
#define FLOOR_RISE  (0.001f) // how slowly the floor creeps back up
#define OCC_ALPHA   (0.05f) // occupancy EMA smoothing

const char* cerberus_threat_name(CerberusThreat threat) {
    switch(threat) {
    case CerberusThreatJam:
        return "JAMMING";
    case CerberusThreatFlood:
        return "FLOOD";
    case CerberusThreatReplay:
        return "REPLAY";
    default:
        return "CLEAR";
    }
}

const char* cerberus_threat_short(CerberusThreat threat) {
    switch(threat) {
    case CerberusThreatJam:
        return "JAM";
    case CerberusThreatFlood:
        return "FLOOD";
    case CerberusThreatReplay:
        return "RPLY";
    default:
        return "OK";
    }
}

void cerberus_detector_init(CerberusDetector* d) {
    memset(d, 0, sizeof(CerberusDetector));
    d->floor = -96.0f;
    d->peak = -120.0f;
    d->occupancy = 0.0f;
}

void cerberus_detector_reset_floor(CerberusDetector* d) {
    // Keep nothing - a clean recalibration of the environment.
    cerberus_detector_init(d);
}

void cerberus_detector_set_sensitivity(CerberusDetectorParams* p, int sensitivity) {
    // High sensitivity => smaller margin (reacts to weaker / fewer signals).
    switch(sensitivity) {
    case 2: // High
        p->margin_db = 10.0f;
        p->flood_rate = 9;
        p->jam_ms = 1000;
        p->replay_repeats = 3;
        break;
    case 0: // Low
        p->margin_db = 24.0f;
        p->flood_rate = 20;
        p->jam_ms = 1800;
        p->replay_repeats = 5;
        break;
    default: // Medium
        p->margin_db = 16.0f;
        p->flood_rate = 14;
        p->jam_ms = 1300;
        p->replay_repeats = 4;
        break;
    }
    p->cooldown_ms = 4000;
    p->display_ms = 3000;
}

// Collapse a burst into a coarse fingerprint: duration bucket + amplitude bucket.
// Two presses of the same remote land in the same bucket; a replayed capture
// repeats that bucket far more often than a human would.
static uint16_t cerberus_fingerprint(uint32_t duration, float peak, float floor) {
    uint16_t db = (duration > 500) ? 9 : (uint16_t)(duration / 50); // 0..9 (50ms steps)
    int rel = (int)(peak - floor);
    if(rel < 0) rel = 0;
    if(rel > 60) rel = 60;
    uint16_t pb = (uint16_t)(rel / 6); // 0..10
    return (uint16_t)((db << 4) | pb) | 0x100; // 0x100 marks a valid (non-empty) slot
}

static uint16_t cerberus_count_recent(const uint32_t* ring, uint32_t now, uint32_t window) {
    uint16_t count = 0;
    for(size_t i = 0; i < CERBERUS_BURST_RING; i++) {
        uint32_t t = ring[i];
        if(t != 0 && (now - t) <= window) count++;
    }
    return count;
}

static uint8_t cerberus_count_fp(const uint16_t* ring, uint16_t fp) {
    uint8_t count = 0;
    for(size_t i = 0; i < CERBERUS_FP_RING; i++) {
        if(ring[i] == fp) count++;
    }
    return count;
}

CerberusThreat cerberus_detector_feed(
    CerberusDetector* d,
    const CerberusDetectorParams* p,
    float rssi,
    uint32_t now,
    bool detect_jam,
    bool detect_flood,
    bool detect_replay) {
    CerberusThreat raised = CerberusThreatNone;

    // --- Adaptive noise floor: drop fast toward quiet, creep up slowly ---
    if(rssi < d->floor) {
        d->floor = d->floor * (1.0f - FLOOR_DROP) + rssi * FLOOR_DROP;
    } else {
        d->floor = d->floor * (1.0f - FLOOR_RISE) + rssi * FLOOR_RISE;
    }
    if(d->floor < FLOOR_MIN) d->floor = FLOOR_MIN;
    if(d->floor > FLOOR_MAX) d->floor = FLOOR_MAX;

    // --- Peak hold with slow decay toward the floor ---
    if(rssi > d->peak) {
        d->peak = rssi;
    } else {
        d->peak += (d->floor - d->peak) * 0.02f;
    }

    const float threshold = d->floor + p->margin_db;
    const bool busy = rssi > threshold;

    // --- Occupancy EMA (drives jamming detection) ---
    d->occupancy += ((busy ? 1.0f : 0.0f) - d->occupancy) * OCC_ALPHA;

    // --- Burst edge detection ---
    if(busy && !d->busy) {
        // rising edge: a new burst started
        d->busy = true;
        d->burst_start = now;
        d->burst_peak = rssi;
        d->burst_times[d->burst_head] = now;
        d->burst_head = (uint8_t)((d->burst_head + 1) % CERBERUS_BURST_RING);
    } else if(busy) {
        if(rssi > d->burst_peak) d->burst_peak = rssi;
    } else if(!busy && d->busy) {
        // falling edge: burst ended -> fingerprint it
        d->busy = false;
        uint32_t duration = now - d->burst_start;
        uint16_t fp = cerberus_fingerprint(duration, d->burst_peak, d->floor);
        d->fp_ring[d->fp_head] = fp;
        d->fp_head = (uint8_t)((d->fp_head + 1) % CERBERUS_FP_RING);
    }

    d->bursts_per_sec = cerberus_count_recent(d->burst_times, now, 1000);

    // --- Candidate evaluation (priority: jam > flood > replay) ---
    CerberusThreat candidate = CerberusThreatNone;

    if(d->occupancy > JAM_OCC) {
        if(d->high_occ_since == 0) d->high_occ_since = now;
    } else {
        d->high_occ_since = 0;
    }

    if(detect_jam && d->high_occ_since != 0 && (now - d->high_occ_since) >= p->jam_ms) {
        candidate = CerberusThreatJam;
    }

    if(candidate == CerberusThreatNone && detect_flood && d->bursts_per_sec >= p->flood_rate) {
        candidate = CerberusThreatFlood;
    }

    if(candidate == CerberusThreatNone && detect_replay && !d->busy) {
        // check the fingerprint we most recently stored
        uint8_t prev = (uint8_t)((d->fp_head + CERBERUS_FP_RING - 1) % CERBERUS_FP_RING);
        uint16_t fp = d->fp_ring[prev];
        if(fp != 0 && cerberus_count_fp(d->fp_ring, fp) >= p->replay_repeats) {
            candidate = CerberusThreatReplay;
        }
    }

    // --- Raise (edge-triggered, rate-limited) and drive the display state ---
    if(candidate != CerberusThreatNone) {
        d->threat = candidate;
        d->threat_until = now + p->display_ms;
        if(now >= d->cooldown_until) {
            raised = candidate;
            d->cooldown_until = now + p->cooldown_ms;
        }
    } else if(d->threat != CerberusThreatNone && now > d->threat_until) {
        d->threat = CerberusThreatNone;
    }

    return raised;
}
