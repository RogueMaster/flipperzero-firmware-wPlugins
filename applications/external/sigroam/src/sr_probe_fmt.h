#pragma once

#include "sr_dialect.h"
#include "sr_types.h"
#include "sr_view_fmt.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

/*
 * ★ Probe Ok-page copy. Zero furi (ADR-003).
 *
 * Product identity for SigRoam dialect; UART Version: / ESP-IDF: stay off
 * the screen (ADR-027). Generic Marauder keeps the firmware name, not v1.14.1.
 */

static inline void sr_probe_fmt_board(const SrFirmwareInfo* fw, char* out, size_t cap) {
    if(out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';
    if(fw == NULL) {
        (void)snprintf(out, cap, "?");
        return;
    }
    if(sr_fmt_hw_is_scout_lite(fw->hardware, sizeof(fw->hardware))) {
        (void)snprintf(out, cap, "Scout Lite");
        return;
    }
    if(fw->hardware[0] != '\0') {
        (void)snprintf(out, cap, "%s", fw->hardware);
        return;
    }
    (void)snprintf(out, cap, "?");
}

static inline void sr_probe_fmt_diag(const SrFirmwareInfo* fw, char* out, size_t cap) {
    static const char* const k_states[] = {
        "IDLE", "SCANNING", "STOPPING", "DRAINING", "SEALED", "UPLOADING"};
    static const char* const k_gates[] = {
        "scan_quiet",
        "ble_quiet",
        "obs_conv",
        "prelock_req",
        "prelock_ack",
        "rec_empty",
        "seal_try"};
    const char* st;
    const char* stuck;
    unsigned i;

    if(out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';
    if(fw == NULL || !fw->diag_seen) {
        return;
    }

    st = (fw->diag_state < (uint8_t)(sizeof(k_states) / sizeof(k_states[0]))) ?
             k_states[fw->diag_state] :
             "?";
    stuck = "(all cleared)";
    for(i = 0; i < (unsigned)(sizeof(k_gates) / sizeof(k_gates[0])); i++) {
        if((fw->diag_seal & (1u << i)) == 0u) {
            stuck = k_gates[i];
            break;
        }
    }
    (void)snprintf(
        out,
        cap,
        "\nstate: %s\n"
        "stuck: %s\n"
        "hb %lu/%lu/%lu\n"
        "   %lu/%lu/%lu\n",
        st,
        stuck,
        (unsigned long)fw->diag_hb[0],
        (unsigned long)fw->diag_hb[1],
        (unsigned long)fw->diag_hb[2],
        (unsigned long)fw->diag_hb[3],
        (unsigned long)fw->diag_hb[4],
        (unsigned long)fw->diag_hb[5]);
}

static inline size_t sr_probe_fmt_ok(
    const SrFirmwareInfo* fw,
    const char* scanner_ver,
    const char* brand_line,
    char* out,
    size_t cap) {
    char board[SR_FW_HARDWARE_MAX + 1];
    char diag[128];
    const char* ver;
    const char* brand;
    const char* title;
    int n;

    if(out == NULL || cap == 0u) {
        return 0;
    }
    out[0] = '\0';
    if(fw == NULL) {
        return 0;
    }

    sr_probe_fmt_board(fw, board, sizeof(board));
    sr_probe_fmt_diag(fw, diag, sizeof(diag));
    ver = (scanner_ver != NULL && scanner_ver[0] != '\0') ? scanner_ver : "?";
    brand = (brand_line != NULL && brand_line[0] != '\0') ? brand_line : "";
    title = (fw->firmware[0] != '\0') ? fw->firmware : "?";

    if(sr_dialect_is_sigroam(fw)) {
        n = snprintf(
            out,
            cap,
            "\e#SigRoam\n"
            "v%s\n"
            "%s\n"
            "%s\n"
            "%s",
            ver,
            board,
            brand,
            diag);
    } else {
        n = snprintf(
            out,
            cap,
            "\e#%s\n"
            "%s\n"
            "%s",
            title,
            board,
            diag);
    }
    if(n < 0) {
        out[0] = '\0';
        return 0;
    }
    if((size_t)n >= cap) {
        return cap - 1u;
    }
    return (size_t)n;
}
