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

#ifdef __cplusplus
}
#endif
