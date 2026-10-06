// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "preflight.h"

#include <string.h>

ReconPreflightState recon_preflight_evaluate(const ReconPreflightInput* input) {
    if(!input) return ReconPreflightFailed;

    // These are definitive faults; waiting longer cannot make the already-read
    // answer compatible.
    if(input->link_busy) return ReconPreflightFailed;

    // Generic/Marauder text scraping has no version, signature-revision, or
    // self-test protocol. It can prove UART activity, but it cannot honestly
    // pass the companion-specific gate.
    if(!input->companion_backend) {
        if(input->link_running && input->connected) return ReconPreflightLimited;
        return input->elapsed_ms < RECON_PREFLIGHT_GRACE_MS ? ReconPreflightWaiting :
                                                              ReconPreflightFailed;
    }

    if((input->protocol_seen && !input->protocol_match) ||
       (input->signature_seen && !input->signature_match) ||
       (input->sigtest_seen && !input->sigtest_pass)) {
        return ReconPreflightFailed;
    }

    // READY requires proof from every layer, including an increasing capture
    // counter. A banner alone only proves that serial TX/RX works; it says
    // nothing about the radio actually receiving frames.
    if(input->link_running && input->connected && input->protocol_seen && input->protocol_match &&
       input->signature_seen && input->signature_match && input->sigtest_seen &&
       input->sigtest_pass && input->frames > 0) {
        return ReconPreflightReady;
    }

    if(input->elapsed_ms < RECON_PREFLIGHT_GRACE_MS) return ReconPreflightWaiting;

    // Grace is up and something is unproven. Separate "could not verify" from
    // "verified and wrong": a companion that is plainly capturing, on a matching
    // protocol, but never answers SIGREV or sigtest is simply older than that
    // handshake. Reported by @h00die (discussion #26), who deliberately reflashes
    // about every fifth release -- calling his working board FAILED would be the
    // detector lying about itself, which is the one thing this screen exists to
    // stop. Anything actually contradicted was already caught above, and zero
    // frames is still a hard fault because it is a real capture failure.
    if(input->link_running && input->connected && input->protocol_seen && input->protocol_match &&
       input->frames > 0) {
        return ReconPreflightLimited;
    }

    return ReconPreflightFailed;
}

const char* recon_preflight_state_label(ReconPreflightState state) {
    switch(state) {
    case ReconPreflightReady:
        return "READY";
    case ReconPreflightLimited:
        return "LIMITED";
    case ReconPreflightFailed:
        return "FAILED";
    case ReconPreflightWaiting:
    default:
        return "CHECKING";
    }
}

/* ---- companion build pairing (discussion #26) ---------------------------- */

/** Skip a leading 'v'/'V' so "v0.98" and "0.98" compare equal. */
static const char* pf_ver_digits(const char* s) {
    if(!s) return "";
    if(*s == 'v' || *s == 'V') s++;
    return s;
}

bool recon_companion_build_matches(const char* esp_build, const char* app_version) {
    const char* a = pf_ver_digits(esp_build);
    const char* b = pf_ver_digits(app_version);
    // A build this app cannot name is not a mismatch to report. RECON_VERSION
    // falls back to "v?.??" when the app is built outside application.fam, and
    // screaming "reflash" at a developer's own tree would be noise about the
    // tooling rather than about the board.
    if(!*b || strchr(b, '?')) return true;
    // Empty = a companion older than v0.88, which had no build field at all.
    // That genuinely is not the paired half, so it is a mismatch rather than
    // unknown -- the Firmware screen already names it "pre-0.88".
    if(!*a) return false;
    return strcmp(a, b) == 0;
}
