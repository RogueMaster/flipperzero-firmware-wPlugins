#pragma once

#include "sr_types.h"

#include <stddef.h>
#include <stdbool.h>

/*
 * ★ Pure-logic dialect switch. Must not include any furi header (ADR-003).
 *
 * Marauder text Mode A is the baseline; SigRoam firmware is an enhanced
 * peer that still speaks that dialect plus Radio:/Qual:/Sess:/Busy:/Diag:.
 * Session confirmation extras for stock Marauder (CSV-as-start, idle WiFi
 * stop) must not run on a SigRoam Version string, and must not run before
 * Probe/info has filled firmware.version (empty version keeps the historical
 * strict rules so existing host_test stays green).
 *
 * Discriminator is the Version: value substring "-sigroam-", copied from
 * firmware kVersion "v1.14.1-sigroam-<n>". Handshake Firmware: Marauder is
 * shared and must not be used as the switch.
 */

#define SR_DIALECT_SIGROAM_MARK "-sigroam-"

static inline bool sr_dialect_version_has_sigroam(const char* ver) {
    static const char k[] = SR_DIALECT_SIGROAM_MARK;
    size_t i;
    size_t j;

    if(ver == NULL) {
        return false;
    }
    for(i = 0; ver[i] != '\0'; i++) {
        for(j = 0; k[j] != '\0'; j++) {
            if(ver[i + j] != k[j]) {
                break;
            }
        }
        if(k[j] == '\0') {
            return true;
        }
    }
    return false;
}

static inline bool sr_dialect_is_sigroam(const SrFirmwareInfo* fw) {
    if(fw == NULL) {
        return false;
    }
    return sr_dialect_version_has_sigroam(fw->version);
}

/* Non-empty Version that is not a SigRoam enhanced build. */
static inline bool sr_dialect_is_generic_marauder(const SrFirmwareInfo* fw) {
    if(fw == NULL) {
        return false;
    }
    if(fw->version[0] == '\0') {
        return false;
    }
    return !sr_dialect_version_has_sigroam(fw->version);
}
