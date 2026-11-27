#pragma once
/*
 * fsd_steer.h — SCCM_steeringAngleSensor (0x129) decode and the steering-angle
 * helpers its consumers share, header-only for the Flipper and ESP32 builds.
 *
 * Layout (opendbc tesla_model3_party.dbc and tesla_model3_vehicle.dbc, BO_ 297):
 *   SCCM_steeringAngleCrc           0|8
 *   SCCM_steeringAngleCounter       8|4
 *   SCCM_supplierID                12|2
 *   SCCM_steeringAngleSensorStatus 14|2   0 OK 1 INIT 2 ERROR 3 ERROR_INIT
 *   SCCM_steeringAngle             16|14  (0.1, -819.2) deg
 *   SCCM_steeringAngleValidity     30|2   0 INVALID 1 VALID 2 INIT 3 SNA
 *   SCCM_steeringAngleSpeed        32|14  (0.5, -4096) deg/s
 * Bytes 0-1 are the CRC and the counter/supplier/status nibbles. Reading them
 * as an int16 angle gave 819..1228 deg on every 0x129 in the #176 (HW3 Model 3)
 * and #211 (HW4 Model Y) black-box records; this layout gives -6..+41 deg there.
 *
 * Legacy Model S/X carry the angle on 0x003 / 0x00E (tesla_can.dbc, big-endian),
 * not on 0x129, so on those cars no 0x129 arrives and the angle stays unknown.
 *
 * Header-only static inline (the ESP32 build compiles no fsd_logic source files).
 */

#include "fsd_state.h"
#include <stdbool.h>
#include <stdint.h>

#define FSD_STEER_MIN_DLC        4u
#define FSD_STEER_STATUS_OK      0u
#define FSD_STEER_VALIDITY_VALID 1u
// 0x129 runs at ~100 Hz; an angle older than this is unknown.
#define FSD_STEER_FRESH_MS       500u
// EPAS-faithful nag torque (#100) points opposite the wheel. Inside this band the
// last direction holds: the near-centre wander in the #176 records stays within
// about ±3 deg, and each flip jumps the synthetic torque by 2.5-3.6 Nm in one
// frame.
#define FSD_STEER_NAG_HOLD_DEG   3.0f

// Decode one 0x129 payload. True (and *deg set) only for a frame that carries a
// usable angle: validity VALID and sensor status OK.
static inline bool fsd_steer_decode(const uint8_t* d, uint8_t dlc, float* deg) {
    if(dlc < FSD_STEER_MIN_DLC) return false;
    uint8_t status = (uint8_t)(d[1] >> 6);
    uint8_t validity = (uint8_t)(d[3] >> 6);
    if(status != FSD_STEER_STATUS_OK || validity != FSD_STEER_VALIDITY_VALID) return false;
    uint16_t raw = (uint16_t)(d[2] | ((uint16_t)(d[3] & 0x3Fu) << 8));
    *deg = (float)raw * 0.1f - 819.2f;
    return true;
}

// Store a known angle (0x129 or the Signal Map steering source).
static inline void fsd_steer_set(FSDState* s, float deg, uint32_t now_ms) {
    s->steering_angle_deg = deg;
    s->steering_angle_seen = true;
    s->steering_angle_valid = true;
    s->steering_angle_ms = now_ms;
}

// Parse one 0x129. A frame that is not VALID/OK marks the angle unknown; the
// last valid value stays in steering_angle_deg but no consumer reads it then.
static inline void fsd_steer_update(FSDState* s, const uint8_t* d, uint8_t dlc, uint32_t now_ms) {
    if(dlc < FSD_STEER_MIN_DLC) return;
    float deg;
    if(fsd_steer_decode(d, dlc, &deg)) {
        fsd_steer_set(s, deg, now_ms);
    } else {
        s->steering_angle_seen = true;
        s->steering_angle_valid = false;
    }
}

// True while a valid angle is at most FSD_STEER_FRESH_MS old.
static inline bool fsd_steer_known(const FSDState* s, uint32_t now_ms) {
    return s->steering_angle_valid &&
           (uint32_t)(now_ms - s->steering_angle_ms) <= FSD_STEER_FRESH_MS;
}

// EPAS-faithful nag torque direction: -1 for a wheel turned positive, +1 for
// negative. A known angle beyond ±FSD_STEER_NAG_HOLD_DEG sets it; inside the
// band, or with the angle unknown, the last direction holds (+1 before any).
static inline int fsd_steer_nag_dir(FSDState* s, uint32_t now_ms) {
    if(fsd_steer_known(s, now_ms)) {
        if(s->steering_angle_deg > FSD_STEER_NAG_HOLD_DEG)
            s->nag_torque_dir = -1;
        else if(s->steering_angle_deg < -FSD_STEER_NAG_HOLD_DEG)
            s->nag_torque_dir = 1;
    }
    return (s->nag_torque_dir < 0) ? -1 : 1;
}
