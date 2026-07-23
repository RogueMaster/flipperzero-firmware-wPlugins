// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file flock_ble.h
 * Decoder for the Flock Safety external-battery BLE advert (mfg id 0x09C8,
 * XUNTONG). Extracts the always-on ASCII device serial and makes a CONSERVATIVE
 * model guess (Falcon ALPR vs Raven acoustic sensor).
 *
 * Pure logic, no firmware dependencies, so it can be unit-tested on a host.
 *
 * Sourced from open counter-surveillance research: ryanohoro's Falcon teardown
 * (the "TN7..." serial inside the XUNTONG mfg data, and the legacy
 * "Penguin-NNNN" / "FS Ext Battery" GAP name) and colonelpanichacks/flock-you.
 *
 * IMPORTANT: the serial belongs to the *shared* external-battery unit that
 * Flock co-deploys on BOTH the Falcon and the Raven, so the advert ALONE cannot
 * split them. Raven IS now positively derivable from a DIFFERENT signal: the
 * Raven exposes acoustic-sensor-specific GATT services (0x3100-0x3500) that the
 * Falcon does not, surfaced by the companion firmware as a `raven_gatt` flag --
 * see flock_ble_model_ex() in flock_ble.c. Falcon, by contrast, is still NOT
 * derivable: there is no Falcon-specific tell, and absence of the Raven GATT is
 * NOT proof of Falcon (precision rule -- we never assert Falcon by elimination).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Conservative Flock BLE model identification from the 0x09C8 advert + GATT. */
typedef enum {
    FlockBleModelUnknown = 0, /**< Nothing decoded as a Flock battery unit. */
    FlockBleModelGeneric, /**< Flock external battery -- model not determinable. */
    FlockBleModelFalcon, /**< NEEDS VALIDATION: ALPR camera (never emitted -- no tell). */
    FlockBleModelRaven, /**< CONFIRMED via Raven-specific GATT (acoustic sensor). */
} FlockBleModel;

/**
 * Extract the ASCII device serial from the 0x09C8 manufacturer payload, falling
 * back to a bare alphanumeric GAP name (post-2025-03 firmware drops "Penguin-").
 *
 * @param mfg         Raw manufacturer-specific data INCLUDING the 2-byte LE
 *                    company id (may be NULL/empty if only the name is known).
 * @param len         Length of @p mfg in bytes.
 * @param name        GAP device name, or NULL/"" if unknown.
 * @param out_serial  Receives a NUL-terminated serial (cleared on failure).
 * @param serial_cap  Capacity of @p out_serial in bytes.
 * @return true if a plausible serial was extracted.
 */
bool flock_ble_extract_serial(
    const uint8_t* mfg,
    size_t len,
    const char* name,
    char* out_serial,
    size_t serial_cap);

/**
 * Conservative model identification with the Raven GATT signal folded in.
 *
 * @param serial      Decoded 0x09C8 battery serial, or NULL/"" if none.
 * @param name        GAP device name, or NULL/"" if unknown.
 * @param raven_gatt  true iff the companion firmware saw a Raven-specific GATT
 *                    service (0x3100-0x3500) on this device. This is the ONLY
 *                    positive model tell currently known.
 * @return FlockBleModelRaven when @p raven_gatt is set (the acoustic-sensor GATT
 *         is Raven-specific -> a confident identification); otherwise
 *         Generic for a decoded Flock battery, Unknown for everything else.
 *         NEVER returns Falcon: absence of the Raven GATT is not proof of Falcon.
 */
FlockBleModel flock_ble_model_ex(const char* serial, const char* name, bool raven_gatt);

/**
 * Back-compat wrapper: identical to flock_ble_model_ex(serial, name, false), so
 * host tests and older callers that lack the GATT signal still work (they just
 * never see Raven). Returns Generic/Unknown only.
 */
FlockBleModel flock_ble_model(const char* serial, const char* name);

/**
 * Human-readable label. The Raven label is GATT-backed and therefore confident
 * (no "?"); the Falcon label keeps its "?" since Falcon is never asserted.
 */
const char* flock_ble_model_str(FlockBleModel model);

#ifdef __cplusplus
}
#endif
