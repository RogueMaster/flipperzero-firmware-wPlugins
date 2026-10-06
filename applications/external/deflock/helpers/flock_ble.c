// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "flock_ble.h"
#include <string.h>

/** ASCII upper-case (no locale, safe for embedded). */
static char ascii_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

/** Printable 7-bit ASCII (the serial is plain ASCII inside the mfg blob). */
static bool is_print(uint8_t b) {
    return b >= 0x20 && b < 0x7f;
}

/** Case-insensitive prefix test (needle assumed already upper-case). */
static bool ci_prefix(const char* s, const char* needle_upper) {
    if(!s) return false;
    for(size_t k = 0; needle_upper[k]; k++) {
        if(ascii_upper(s[k]) != needle_upper[k]) return false;
    }
    return true;
}

/** Case-insensitive substring test (needle assumed already upper-case). */
static bool ci_contains(const char* s, const char* needle_upper) {
    if(!s || !needle_upper[0]) return false;
    for(const char* h = s; *h; h++) {
        if(ci_prefix(h, needle_upper)) return true;
    }
    return false;
}

FlockConfidence flock_ble_confidence(uint16_t company, const char* name, bool raven_gatt) {
    // Strong, near-unique tells. Each names Flock specifically rather than the
    // silicon vendor, so each stands on its own:
    //   - 0x09C8 is Flock's own manufacturer id in the advert;
    //   - the Raven GATT services (0x3100-0x3500) are Raven-SPECIFIC;
    //   - "Penguin-*" / "FS Ext *" are Flock's own product naming.
    //
    // 0x09C8 IS FILED UNDER XUNTONG, THE BATTERY VENDOR, NOT FLOCK (see
    // docs/signatures.md and the AXON_BLE_COMPANY_ID comment in flock_ble.h), so
    // in principle it has the same shared-identifier shape as a WiFi OUI. It is
    // NOT gated on corroboration anyway, and that is deliberate:
    //
    //   - there is no corroborator strong enough to gate on. The serial format is
    //     one observed sample ("TN72023022000771"), no documented field offset, no
    //     prefix/length/checksum structure, and it is KNOWN TO HAVE CHANGED (newer
    //     firmware dropped "Penguin-" for bare digits). Any >=6 alphanumeric run
    //     satisfies flock_ble_extract_serial -- its own test proves "LONGEST9"
    //     passes -- so it would rule out empty payloads and nothing else.
    //   - the cost of being wrong is asymmetric and severe. This function returns
    //     only Confirmed(4) or Possible(1); there is no middle rung on the BLE
    //     path. The default alert gate is Likely(2), so a demotion here is 4->1
    //     straight THROUGH the gate: no beep, no vibro, no alert card, and
    //     permanently, because confidence is monotonic (recon_app.c) and nothing
    //     on this path can raise it back. A real camera would go silent.
    //
    // Tried and reverted on 2026-09-06 after a bench "false positive" turned out
    // to be the emitter itself, correctly Confirmed via its Raven GATT identity
    // and merely wearing a stale first-seen name. Do not re-add a serial-shaped
    // gate here without a corroborator that is actually structural.
    // flock_ble_tell() reports WHICH tell fired so a shared-id match is visible
    // to the operator -- that is the precision answer, not a lower rung.
    if(company == FLOCK_BLE_COMPANY_ID) return FlockConfidenceConfirmed;
    // Axon's own SIG-registered company id. Vendor-exclusive like 0x09C8 -- it
    // names Axon, not a silicon vendor -- so it stands on its own for the same
    // reason. What it confirms is an AXON DEVICE, not a Flock camera; the device
    // class carries that distinction, this function only answers "how sure".
    if(company == AXON_BLE_COMPANY_ID) return FlockConfidenceConfirmed;
    if(raven_gatt) return FlockConfidenceConfirmed;
    // Delegates to flock_ble_name_is_flock() rather than repeating the patterns.
    // It used to inline them, and when the field-observed names (Pigvision,
    // RWLS-, FlockCam, FS-XXXXXX) were added to the helper on 2026-09-07 this
    // line did not learn them -- the helper went green while the function that
    // actually scores a detection still returned Possible. Caught only because
    // the new tests assert through flock_ble_confidence(), not through the
    // helper. One definition, one place to extend.
    if(flock_ble_name_is_flock(name)) return FlockConfidenceConfirmed;

    // Nothing Flock-specific left. The only other way the companion classifies a
    // device as Flock is an OUI prefix on the BLE address, and those prefixes are
    // SHARED silicon-vendor ranges (Espressif, Liteon and friends -- see the
    // table comment in flock_db.c). That is exactly the "possible" rung and must
    // never be announced as a confirmed camera.
    //
    // This is a TRUST BOUNDARY, not a redundant check, and it is deliberately a
    // FLOOR rather than an allow-list: a newer companion may classify on a tell
    // this build predates, and the honest answer to "Flock, for a reason I don't
    // recognise" is "possible", not "confirmed". Before this cap existed, any BLE
    // device with a static address in the OUI table was reported as CONFIRMED
    // Flock -- the same over-claim as the v0.46 "Flock-Guest" SSID bug, on the
    // BLE path, which the SSID-side guard in esp_parser.c never covered.
    return FlockConfidencePossible;
}

/**
 * "FS-" + exactly six hex digits, the whole name -- Flock's post-"Penguin"
 * unit-id GAP name (zmattmanz/plume, "newer naming convention").
 *
 * ANCHORED AND SHAPED, deliberately. A bare "FS-" prefix is two letters and
 * would confirm a camera on any device that happens to start that way; it is
 * the same shape of mistake as the unanchored "flock-" substring that shipped
 * `Flock-Guest` as CONFIRMED in v0.46. Requiring the full six-hex form and
 * nothing after it is what makes this safe to stake a Confirmed on -- the exact
 * rule is_flock_provisioning_ssid() applies to "Flock-XXXXXX" on the Wi-Fi side.
 */
static bool is_fs_unit_name(const char* name) {
    if(!name) return false;
    if(!(ascii_upper(name[0]) == 'F' && ascii_upper(name[1]) == 'S' && name[2] == '-')) {
        return false;
    }
    for(int i = 3; i < 9; i++) {
        char c = name[i]; // '\0' (short name) is not hex -> correctly rejected
        bool hex = (c >= '0' && c <= '9') || (ascii_upper(c) >= 'A' && ascii_upper(c) <= 'F');
        if(!hex) return false;
    }
    return name[9] == '\0';
}

bool flock_ble_name_is_flock(const char* name) {
    // Flock's own product naming. Every entry here is staked on CONFIRMED -- this
    // function is what flock_ble_confidence() consults for the naming rung -- so
    // the bar is "a string no ordinary device would choose", not "contains flock".
    //
    // NOT ADDED, on purpose: a bare "FLOCK" prefix. The BLE path has no Likely
    // rung, so it would promote anything merely flock-ish straight to Confirmed,
    // which is precisely the v0.46 `Flock-Guest` over-claim. The Wi-Fi side gets
    // to score a loose "flock" substring as LIKELY; here there is no such landing
    // spot, so loose patterns are excluded rather than softened.
    // "PENGUIN-", with the dash: every observed unit name is "Penguin-<digits>".
    // The bare word is a product name other people use too (speakers, toys,
    // a Linux mascot), and this predicate stakes CONFIRMED, so the shape that
    // only Flock's firmware produces is the one that counts.
    if(ci_prefix(name, "PENGUIN-") || ci_contains(name, "FS EXT")) return true;
    // Field-observed Flock BLE names, all long and vendor-specific enough to
    // stand alone (zmattmanz/plume, corroborated by the flock-you lineage):
    //   PIGVISION  -- Flock's Pigvision units
    //   FLOCKCAM   -- names the product, not the vendor family
    //   RWLS-      -- observed as "RWLS-38:5B:44:B3:0F:5A", the unit's own MAC
    //                 appended; the prefix alone is four letters plus a dash and
    //                 has no ordinary-device meaning.
    if(ci_prefix(name, "PIGVISION") || ci_prefix(name, "FLOCKCAM")) return true;
    if(ci_prefix(name, "RWLS-")) return true;
    return is_fs_unit_name(name);
}

/**
 * Stock module / stack names that identify nothing about the device.
 *
 * ANCHORED prefixes only. Deliberately short and boring: every entry must be a
 * name a device ships with rather than one it chose, because anything on this
 * list can be displaced by a later advert. Nothing Flock-adjacent belongs here,
 * and a test asserts none of these collide with a Flock-shaped name.
 */
static const char* const k_generic_names[] = {
    "ESP32",
    "ESP_",
    "ESP-",
    "ARDUINO",
    "NRF",
    "BLUETOOTH",
    "UNKNOWN",
    "NONAME",
};

int flock_ble_name_specificity(const char* name) {
    if(!name || !name[0]) return 0;

    // Self-identifying: Flock's own product naming, or a bare serial. The serial
    // check is the post-2025-03 firmware case, where the GAP name IS the serial
    // and carries no "Penguin-" prefix to recognise it by.
    char probe[24];
    if(flock_ble_name_is_flock(name) ||
       flock_ble_extract_serial(NULL, 0, name, probe, sizeof(probe))) {
        return 3;
    }

    for(size_t i = 0; i < sizeof(k_generic_names) / sizeof(k_generic_names[0]); i++) {
        if(ci_prefix(name, k_generic_names[i])) return 1;
    }
    // Exact-match-only entries: too short to use as prefixes without catching
    // real names ("BT" would swallow "BTLE-Cam-3").
    if(ci_prefix(name, "BT") && !name[2]) return 1;
    if(ci_prefix(name, "BLE") && !name[3]) return 1;

    return 2;
}

bool flock_ble_name_should_replace(const char* current, const char* candidate) {
    return flock_ble_name_specificity(candidate) > flock_ble_name_specificity(current);
}

FlockBleTell
    flock_ble_tell(uint16_t company, const char* name, bool raven_gatt, const uint8_t* addr) {
    // Mirrors flock_ble_confidence()'s precedence exactly, so the tell always
    // explains the rung that function returned rather than describing some other
    // signal that happened to also be present.
    if(company == FLOCK_BLE_COMPANY_ID || company == AXON_BLE_COMPANY_ID) {
        return FlockBleTellMfgId;
    }
    if(raven_gatt) return FlockBleTellRavenGatt;
    if(flock_ble_name_is_flock(name)) return FlockBleTellNaming;

    // POSITIVE identification of the shared-OUI case. Everything above is a
    // Flock-specific tell; reaching here means the companion classified this
    // device for some other reason, and a bare OUI on the BLE address is the one
    // remaining path it has. Checking the address says so as a fact instead of
    // inferring it from absence -- which matters, because "matched a shared
    // silicon-vendor prefix" and "matched something newer than this build
    // understands" are different statements and only one of them is weak.
    if(addr && (flock_oui_match(addr) || soundthinking_oui_match(addr) || axon_oui_match(addr) ||
                vendor_exclusive_oui_match(addr))) {
        return FlockBleTellOuiOnly;
    }
    return FlockBleTellNone;
}

const char* flock_ble_tell_str(FlockBleTell tell) {
    // TERSE: composed into the detail screen's "Method:" row, which leaves about
    // 26 characters next to a scrollbar on a 128 px display.
    switch(tell) {
    case FlockBleTellMfgId:
        return "BLE mfg";
    case FlockBleTellRavenGatt:
        return "BLE GATT";
    case FlockBleTellNaming:
        return "BLE name";
    case FlockBleTellOuiOnly:
        return "BLE OUI";
    default:
        return "BLE";
    }
}

bool flock_ble_extract_serial(
    const uint8_t* mfg,
    size_t len,
    const char* name,
    char* out_serial,
    size_t serial_cap) {
    if(!out_serial || serial_cap == 0) return false;
    out_serial[0] = '\0';

    // The XUNTONG (0x09C8) manufacturer payload carries a plain-ASCII serial,
    // e.g. "TN72023022000771" (ryanohoro Falcon teardown). The first two bytes
    // of `mfg` are the little-endian company id; the serial lives after it. We
    // don't have a documented field offset, so scan for the longest printable
    // run that looks like a serial (letters+digits, >= 6 chars) and take it.
    if(mfg && len > 2) {
        size_t best_start = 0, best_len = 0;
        size_t run_start = 0, run_len = 0;
        for(size_t i = 2; i <= len; i++) {
            bool ok = (i < len) && is_print(mfg[i]) &&
                      ((mfg[i] >= '0' && mfg[i] <= '9') ||
                       (ascii_upper((char)mfg[i]) >= 'A' && ascii_upper((char)mfg[i]) <= 'Z'));
            if(ok) {
                if(run_len == 0) run_start = i;
                run_len++;
            } else {
                if(run_len > best_len) {
                    best_len = run_len;
                    best_start = run_start;
                }
                run_len = 0;
            }
        }
        if(best_len >= 6) {
            size_t n = best_len;
            if(n > serial_cap - 1) n = serial_cap - 1;
            memcpy(out_serial, &mfg[best_start], n);
            out_serial[n] = '\0';
            return true;
        }
    }

    // Fallback: the legacy GAP name on newer firmware *is* the serial (an all-
    // digit "NNNNNNNNNN" string after the 2025-03 firmware dropped "Penguin-").
    if(name && name[0]) {
        const char* p = name;
        if(ci_prefix(p, "PENGUIN-")) p += 8;
        // Only treat it as a serial if it's a bare alphanumeric token (not
        // "FS Ext Battery", which is a model label, not a unit id).
        size_t i = 0;
        bool has_digit = false;
        for(; p[i]; i++) {
            char c = p[i];
            if(c >= '0' && c <= '9')
                has_digit = true;
            else if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
                break;
        }
        if(p[i] == '\0' && i >= 6 && has_digit) {
            size_t n = i;
            if(n > serial_cap - 1) n = serial_cap - 1;
            memcpy(out_serial, p, n);
            out_serial[n] = '\0';
            return true;
        }
    }

    return false;
}

FlockBleModel flock_ble_model_ex(const char* serial, const char* name, bool raven_gatt) {
    // POSITIVE Raven tell. The Raven exposes acoustic-sensor-specific GATT
    // services (0x3100-0x3500) that the companion firmware reports via the
    // raven_gatt flag. Those UUIDs are Raven-SPECIFIC -- the Falcon and the bare
    // external battery do not advertise them -- so a match is a confident,
    // GATT-backed identification of an audio-surveillance unit, not a guess. We
    // assert this regardless of serial/name (the shared battery serial is still
    // ambiguous, but the GATT is not).
    if(raven_gatt) {
        return FlockBleModelRaven;
    }

    // CONSERVATIVE BY DESIGN for everything else. The 0x09C8 serial (e.g.
    // "TN7...") and the "Penguin-NNNN" / "FS Ext Battery" GAP name belong to the
    // *shared* XUNTONG external-battery unit, which Flock co-deploys on BOTH the
    // Falcon (ALPR) and the Raven (acoustic) on the same solar pole. There is NO
    // published, field-validated serial-prefix -> model mapping that separates a
    // Raven from a Falcon via this battery advert (verified against ryanohoro's
    // Falcon teardown and colonelpanichacks/flock-you), and -- critically --
    // absence of the Raven GATT is NOT proof of Falcon (the GATT may simply not
    // have been observed in this scan window). A wrong confident "FALCON" or
    // "AUDIO SURVEILLANCE" label is worse than a generic one, so we deliberately
    // do NOT guess Falcon here: everything that merely decodes as a Flock
    // external battery stays FlockBleModelGeneric.
    (void)serial;

    if(flock_ble_name_is_flock(name)) {
        return FlockBleModelGeneric;
    }
    if(serial && serial[0]) {
        return FlockBleModelGeneric;
    }
    return FlockBleModelUnknown;
}

const char* flock_ble_model_str(FlockBleModel model) {
    switch(model) {
    // Raven is GATT-backed and confident -> no "?". Falcon keeps its "?" because
    // it is never asserted (no Falcon-specific tell -- see flock_ble_model_ex).
    case FlockBleModelFalcon:
        return "Flock Falcon? (ALPR)";
    case FlockBleModelRaven:
        return "Flock Raven (audio)";
    case FlockBleModelGeneric:
        return "Flock device (ext. battery)";
    case FlockBleModelUnknown:
    default:
        return "-";
    }
}
