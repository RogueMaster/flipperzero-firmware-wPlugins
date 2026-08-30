// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
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

#include "flock_db.h" // FlockConfidence

#ifdef __cplusplus
extern "C" {
#endif

/** Flock Safety's manufacturer id (XUNTONG) in the BLE advert. */
#define FLOCK_BLE_COMPANY_ID 0x09C8

/**
 * Axon Enterprise's Bluetooth SIG manufacturer id, registered under their former
 * name TASER International (they renamed in 2017; SIG records keep the original,
 * exactly as 0x09C8 is filed under the battery vendor XUNTONG rather than Flock).
 *
 * Vendor-exclusive, so it is a genuine tell rather than a shared silicon range --
 * but like the Axon OUI it is REGISTRY-VERIFIED AND NEVER FIELD-OBSERVED. See the
 * axon_ouis[] comment in flock_db.c.
 *
 * Re-verified 2026-08-29 against the SIG assigned-numbers list: 0x034D is still
 * filed as "TASER International, Inc.", and there is no separate "Axon" entry.
 */
#define AXON_BLE_COMPANY_ID 0x034D

/**
 * Motorola Solutions' Bluetooth SIG manufacturer id.
 *
 * Verified 2026-08-29 against the SIG company-identifiers list, where 0x04EC
 * reads "Motorola Solutions" exactly.
 *
 * NOT 0x0008, which is plain "Motorola" -- the legacy consumer-handset
 * registration, i.e. the SIG counterpart of the Motorola Mobility OUI trap
 * documented in flock_db.c. Using 0x0008 would classify phones. Use only 0x04EC.
 *
 * WHY IT MATTERS HERE. Motorola's L6Q quick-deploy plate reader is commissioned
 * over Bluetooth from their "LPR Mobile Companion" phone app, which is the same
 * provisioning-radio pattern that makes Flock's own hardware findable. Like the
 * Axon id above it is REGISTRY-VERIFIED AND NEVER FIELD-OBSERVED, and like the
 * Motorola OUIs it names the VENDOR only -- these radios are also in APX and
 * MOTOTRBO hand-helds, so a match must never be reported as a plate reader.
 *
 * DEFINED BUT NOT YET CONSUMED. The BLE classification path runs on the
 * companion (see recon_app_ble_report), so wiring this up means teaching the
 * sketch a new BLE category, not just adding a constant here. Kept in the header
 * next to its sibling so the verified value is not researched a third time.
 */
#define MOTOROLA_BLE_COMPANY_ID 0x04EC

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
 * How sure we are that a BLE device the companion classified as Flock really is
 * one -- the BLE counterpart to the SSID trust boundary in esp_parser.c.
 *
 * WHY THIS EXISTS. The companion sets cat=1 ("Flock") from several signals, and
 * one of them is a bare OUI-prefix match on the BLE address. Those prefixes are
 * SHARED silicon-vendor ranges, so treating every cat=1 as CONFIRMED announced
 * ordinary ESP32-based hardware as a confirmed surveillance camera. This
 * re-derives the rung from the evidence the app can actually see, and it is a
 * FLOOR: anything without a Flock-specific tell lands on "possible".
 *
 * @param company     Manufacturer id from the advert (0 if none).
 * @param name        GAP device name, or NULL/"" if unknown.
 * @param raven_gatt  true iff a Raven-specific GATT service was seen.
 * @return Confirmed for a Flock-specific tell (0x09C8 mfg id, Raven GATT, or
 *         "Penguin-*"/"FS Ext *" naming); FlockConfidencePossible otherwise.
 *         Never returns None -- the caller only asks about devices already
 *         classified as Flock.
 */
FlockConfidence flock_ble_confidence(uint16_t company, const char* name, bool raven_gatt);

/**
 * Human-readable label. The Raven label is GATT-backed and therefore confident
 * (no "?"); the Falcon label keeps its "?" since Falcon is never asserted.
 */
const char* flock_ble_model_str(FlockBleModel model);

#ifdef __cplusplus
}
#endif
