#pragma once
/*
 * fsd_blackbox_filter.h — capture ID filter for the black-box recorder (#124).
 *
 * The recorder's RAM ring is small: 6000 frames on a no-PSRAM S3. A busy Tesla
 * full bus runs ~3300 frames/s (measured on a Legacy Model X), which fills the
 * ring in ~1.8 s — truncating the intended 10 s pre / 5 s post window and
 * losing the exact steer-jerk / abort lead-up the capture exists for.
 *
 * Recording only the key diagnostic IDs drops the stored rate ~15x (the key set
 * is ~a few hundred f/s), so 6000 frames covers ~15-30 s even on a busy bus —
 * the whole window survives.
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
};

#define FSD_BLACKBOX_KEY_ID_COUNT (sizeof(FSD_BLACKBOX_KEY_IDS) / sizeof(FSD_BLACKBOX_KEY_IDS[0]))

// True when `id` should enter the black-box ring. Runs on every RX frame, so it
// stays cheap: a linear scan of ~12 constants (O(1)-ish, no allocation). Define
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
