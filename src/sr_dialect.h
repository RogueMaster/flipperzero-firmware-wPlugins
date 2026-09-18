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

/*
 * Dash may send `info` only to SigRoam firmware. Stock Marauder info is not
 * read-only: CommandLine INFO_CMD assigns currentScanMode = SHOW_INFO (101),
 * scanning() is then true (WIFI_SCAN_OFF is 0), and wardrive lives inside
 * if (!scanning()). Empty Version (pre-Probe) also skips, so a first Dash
 * open cannot swallow wardrive. Probe still sends info.
 */
static inline bool sr_dialect_dash_may_send_info(const SrFirmwareInfo* fw) {
    return sr_dialect_is_sigroam(fw);
}

/*
 * Stock Marauder INFO_CMD leaves currentScanMode=SHOW_INFO, so the next
 * wardrive is swallowed. Generic path only: Probe queues one stopscan after
 * handshake Ok; Dash queues one more only if Probe did not. SigRoam info is
 * read-only — never send this pair there (it would stop a live scan).
 *
 * Worker is a single slot: Probe and Dash must not both have stopscan in
 * flight. Confirm on wifi_stop_rev moving (Stopping WiFi always prints,
 * even when session is already Stopped), not on session_rev.
 */
typedef enum {
    SrShowInfoClearNone = 0, /* Not generic: send wardrive now */
    SrShowInfoClearSendStop, /* Generic, Probe did not queue: Dash sends stopscan */
    SrShowInfoClearWait /* Generic, Probe already queued: wait for wifi_stop_rev */
} SrShowInfoClearAct;

static inline bool sr_dialect_probe_should_clear_show_info(const SrFirmwareInfo* fw) {
    return sr_dialect_is_generic_marauder(fw);
}

static inline SrShowInfoClearAct sr_dialect_show_info_clear_on_start(
    const SrFirmwareInfo* fw,
    bool probe_stop_sent) {
    if(!sr_dialect_is_generic_marauder(fw)) {
        return SrShowInfoClearNone;
    }
    return probe_stop_sent ? SrShowInfoClearWait : SrShowInfoClearSendStop;
}

/* Wrapping counter: != , never >. */
static inline bool sr_dialect_show_info_clear_done(
    uint32_t wifi_stop_rev_now,
    uint32_t wifi_stop_rev_at_send) {
    return wifi_stop_rev_now != wifi_stop_rev_at_send;
}
