// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
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

    if(name && (ci_prefix(name, "PENGUIN") || ci_prefix(name, "FS EXT"))) {
        return FlockBleModelGeneric;
    }
    if(serial && serial[0]) {
        return FlockBleModelGeneric;
    }
    return FlockBleModelUnknown;
}

FlockBleModel flock_ble_model(const char* serial, const char* name) {
    // Thin wrapper for host tests / older callers that lack the GATT signal.
    return flock_ble_model_ex(serial, name, false);
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
