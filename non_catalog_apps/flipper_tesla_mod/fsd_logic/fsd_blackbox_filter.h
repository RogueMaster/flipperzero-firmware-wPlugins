#pragma once
/*
 * fsd_blackbox_filter.h — capture ID filter for the black-box recorder (#124).
 *
 * The recorder's RAM ring is small: 6000 frames on a no-PSRAM S3. A busy Tesla
 * full bus runs ~3300 frames/s (measured on a Legacy Model X), which fills the
 * ring in ~1.8 s — truncating the intended 10 s pre / 5 s post window and
 * losing the exact steer-jerk / abort lead-up the capture exists for.
 *
 * Recording only the key diagnostic IDs drops the stored rate ~15x. Measured
 * key-set rates: ~200 f/s on a single-bus HW4 Model Y (#211; 6000 frames
 * ≈ 30 s, the whole window survives) and ~620 f/s on a dual-CAN HW3 Model 3
 * (#176; 6000 frames ≈ 9.7 s, so after the 5 s post-roll ~4.7 s of pre-roll is
 * left). The PSRAM ring (40000 frames) holds the full window on both.
 *
 * Header-only + dependency-free (only <stdint.h>/<stdbool.h>) so the host test
 * can exercise the predicate without the ESP32 backends.
 *
 * Escape hatch: define BLACKBOX_CAPTURE_ALL at build time to keep EVERY frame
 * instead — full fidelity, but on a busy bus the window truncates to ~1.8 s as
 * above. Default off.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Curated key diagnostic IDs — the frames the abort / steer-jerk / nag analysis
// actually reads. Edit this list to add or drop a capture ID. IDs mirror the
// CAN_ID_* defines in fsd_handler.h / esp32/.firmware/config.h.
static const uint32_t FSD_BLACKBOX_KEY_IDS[] = {
    0x370u, // EPAS3P_sysStatus     — nag killer / EPAS
    0x399u, // DAS_status (HW3/Legacy), also ISA speed on HW4
    0x39Bu, // DAS_status (HW4)
    0x488u, // DAS_steeringControl  — DAS steering request/angle
    0x129u, // SCCM_steeringAngleSensor — steering angle (#108 soft-engage gate)
    0x3EEu, // DAS_autopilot        — AP legacy
    0x3FDu, // DAS_autopilotControl — AP control (HW3/HW4 core)
    0x145u, // ESP_status           — driver brake pedal
    0x238u, // UI_driverAssistMapData — map speed limit
    0x389u, // DAS_status2          — ACC speed limit
    0x2B9u, // DAS_control          — cruise set speed / ACC state
    0x118u, // DI_systemStatus      — vehicle state
    // Speed / Autopark / Acceleration Mode / battery questions (#176, #211, #251).
    // Low rate except 0x257 (~50 Hz per bus), which fsd_blackbox_thin_keep()
    // thins in the internal-RAM ring.
    0x257u, // DI_speed             — vehicle speed (Autopark release, standstill latch)
    0x334u, // UI_powertrainControl — pedal map (Acceleration Mode, #211)
    0x312u, // BMS_thermalStatus    — pack temperature (layout unconfirmed)
    0x292u, // BMS_socStatus        — pack SOC
    0x33Au, // UI_range             — car-displayed SOC (#251)
};

#define FSD_BLACKBOX_KEY_ID_COUNT (sizeof(FSD_BLACKBOX_KEY_IDS) / sizeof(FSD_BLACKBOX_KEY_IDS[0]))

// True when `id` should enter the black-box ring. Runs on every RX frame, so it
// stays cheap: a linear scan of ~17 constants (O(1)-ish, no allocation). Define
// BLACKBOX_CAPTURE_ALL to bypass the filter and keep every frame.
static inline bool fsd_blackbox_should_record(uint32_t id) {
#ifdef BLACKBOX_CAPTURE_ALL
    (void)id;
    return true;
#else
    for(size_t i = 0; i < FSD_BLACKBOX_KEY_ID_COUNT; i++) {
        if(FSD_BLACKBOX_KEY_IDS[i] == id) return true;
    }
    return false;
#endif
}

// 0x257 DI_speed runs at ~50 Hz on each bus that carries it. At full rate it
// adds up to ~100 f/s on a dual-CAN tap, which on the #176 car would take the
// 6000-frame ring's pre-roll from ~4.7 s to ~3 s. One frame per
// FSD_BLACKBOX_THIN_MS per bus (10 Hz) still answers the speed questions
// (Autopark release at 12/20 km/h, standstill latches, 0-50 / 0-100 runs) for
// ~20 f/s. The caller applies it to the internal-RAM ring only.
#define FSD_BLACKBOX_THIN_ID 0x257u
#define FSD_BLACKBOX_THIN_MS 100u

typedef struct {
    bool seen; // a FSD_BLACKBOX_THIN_ID frame has been stored
    uint32_t last_ms; // when
} FSDBlackboxThin;

// True when a frame that passed fsd_blackbox_should_record() is stored now.
// Only FSD_BLACKBOX_THIN_ID is thinned; *t is its state for the frame's bus.
static inline bool fsd_blackbox_thin_keep(uint32_t id, uint32_t now_ms, FSDBlackboxThin* t) {
#ifdef BLACKBOX_CAPTURE_ALL
    (void)id;
    (void)now_ms;
    (void)t;
    return true;
#else
    if(id != FSD_BLACKBOX_THIN_ID) return true;
    if(t->seen && (uint32_t)(now_ms - t->last_ms) < FSD_BLACKBOX_THIN_MS) return false;
    t->seen = true;
    t->last_ms = now_ms;
    return true;
#endif
}
