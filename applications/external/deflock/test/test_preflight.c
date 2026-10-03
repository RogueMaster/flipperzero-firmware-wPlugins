// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "test.h"
#include "../helpers/preflight.h"

#include <string.h>

void suite_preflight(void) {
    printf("[preflight]\n");

    ReconPreflightInput in = {
        .companion_backend = true,
        .link_running = true,
        .connected = true,
        .protocol_seen = true,
        .protocol_match = true,
        .signature_seen = true,
        .signature_match = true,
        .sigtest_seen = true,
        .sigtest_pass = true,
        .frames = 1,
        .elapsed_ms = 1000,
    };
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightReady);
    CHECK_STR_EQ(recon_preflight_state_label(ReconPreflightReady), "READY");

    in.frames = 0;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightWaiting);
    in.elapsed_ms = RECON_PREFLIGHT_GRACE_MS;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);

    in.frames = 99;
    in.protocol_match = false;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);
    in.protocol_match = true;
    in.signature_match = false;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);
    in.signature_match = true;
    in.sigtest_pass = false;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);

    in.sigtest_pass = true;
    in.link_busy = true;
    in.elapsed_ms = 0;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);

    // A companion OLDER than the SIGREV/sigtest handshake: capturing fine, on a
    // matching protocol, but it cannot prove which signature table it holds.
    // That is LIMITED, not FAILED. Discussion #26: @h00die reflashes roughly
    // every fifth release on purpose, and his working board must not be called
    // broken. The distinction under test is "could not verify" vs "verified
    // wrong" -- so flip each unproven field to a WRONG answer below and the
    // verdict must drop to FAILED.
    in = (ReconPreflightInput){
        .companion_backend = true,
        .link_running = true,
        .connected = true,
        .protocol_seen = true,
        .protocol_match = true,
        .signature_seen = false, // predates SIGREV
        .sigtest_seen = false, // predates sigtest
        .frames = 500,
        .elapsed_ms = 1000,
    };
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightWaiting);
    in.elapsed_ms = RECON_PREFLIGHT_GRACE_MS;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightLimited);

    // Silence is forgiven; a wrong answer never is.
    in.signature_seen = true;
    in.signature_match = false;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);
    in.signature_seen = false;
    in.signature_match = false;
    in.sigtest_seen = true;
    in.sigtest_pass = false;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);

    // And an old companion that is not actually capturing is still a hard fault:
    // zero frames is a capture failure, not a missing handshake.
    in.sigtest_seen = false;
    in.sigtest_pass = false;
    in.frames = 0;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);

    in = (ReconPreflightInput){
        .companion_backend = false,
        .link_running = true,
        .connected = true,
        .frames = 100,
        .elapsed_ms = RECON_PREFLIGHT_GRACE_MS * 2,
    };
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightLimited);
    CHECK_STR_EQ(recon_preflight_state_label(ReconPreflightLimited), "LIMITED");

    in.connected = false;
    in.elapsed_ms = 1000;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightWaiting);
    in.elapsed_ms = RECON_PREFLIGHT_GRACE_MS;
    CHECK_INT_EQ(recon_preflight_evaluate(&in), ReconPreflightFailed);

    CHECK_INT_EQ(recon_preflight_evaluate(NULL), ReconPreflightFailed);
}
