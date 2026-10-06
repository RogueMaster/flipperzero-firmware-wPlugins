// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Time allowed for a companion to prove the UART, protocol, signature table,
 * self-test, and receive path before a field session is called unhealthy. */
#define RECON_PREFLIGHT_GRACE_MS 8000u

typedef enum {
    ReconPreflightWaiting = 0,
    ReconPreflightReady,
    ReconPreflightLimited,
    ReconPreflightFailed,
} ReconPreflightState;

/** Pure snapshot consumed by recon_preflight_evaluate(). */
typedef struct {
    bool companion_backend;
    bool link_running;
    bool link_busy;
    bool connected;
    bool protocol_seen;
    bool protocol_match;
    bool signature_seen;
    bool signature_match;
    bool sigtest_seen;
    bool sigtest_pass;
    uint32_t frames;
    uint32_t elapsed_ms;
} ReconPreflightInput;

/** Classify one health snapshot without touching firmware or UI state. */
ReconPreflightState recon_preflight_evaluate(const ReconPreflightInput* input);

/** Short stable label suitable for the 128x64 health screen. */
const char* recon_preflight_state_label(ReconPreflightState state);

/**
 * True if the companion's advertised build is the half this app was paired with.
 *
 * FLOCK_COMPANION_VERSION in the companion sketch MUST equal FAP_VERSION, and
 * tools/check_oui_parity.py fails CI when they drift -- so a difference here
 * means the board is left over from a different release. The sketch's own
 * comment calls the app-side check "that rule's missing half"; this is it.
 *
 * DELIBERATELY NOT A FAULT ON ITS OWN. The companion version is bumped every
 * release whether or not anything on the board changed, so demoting a healthy
 * board to LIMITED on a version string alone would order a reflash on most
 * releases for no gain -- exactly what discussion #26 asked us not to do. The
 * actionable signal is the SIGNATURE REVISION (FDF_SIGNATURE_REVISION vs the
 * companion's SIGREV): that only moves when the detection data really changed.
 * So this answers "which build am I running" and sig mismatch answers "do I
 * have to reflash".
 *
 * @param esp_build    the companion's advertised build, "" for pre-v0.88 FW.
 * @param app_version  RECON_VERSION. A leading 'v' on either side is ignored.
 * @return true when they pair, or when app_version is the un-stamped "v?.??"
 *         developer fallback (nothing useful to compare against).
 */
bool recon_companion_build_matches(const char* esp_build, const char* app_version);

#ifdef __cplusplus
}
#endif
