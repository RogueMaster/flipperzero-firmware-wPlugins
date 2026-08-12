// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#pragma once

/**
 * GPS over the Unleashed RPC location service ("phone GPS").
 *
 * The Flipper has no GNSS hardware. Unleashed's `gps` service is a pure message
 * bridge: it asks whatever RPC client is attached -- in practice the qUnleashed
 * companion app on a phone, over BLE or USB -- for a location, and hands back
 * what that client answers with. So this source is the PHONE's position, not the
 * Flipper's, and it exists only while the phone is paired and the app is running.
 *
 * FIRMWARE-GATED. The service landed in Unleashed only (upstream PR #1013, API
 * 88.2); official firmware and Momentum have neither the header nor the symbols,
 * and CI builds all three from this one tree. The whole implementation is behind
 * `__has_include(<gps/gps.h>)`, so on those two targets every entry point below
 * compiles to a no-op and gps_rpc_supported() returns false. Nothing here needs a
 * build-matrix change; the gate follows the SDK that is actually in use.
 *
 * OPSEC: this is the one GPS source that requires a second radio-connected
 * device. See the note in README.md -- a paired phone is a far larger and more
 * attributable signature than a passive UART GPS module, which is why the
 * default source is, and stays, the Flipper's own UART.
 */

#include <stdbool.h>

typedef struct GpsRpc GpsRpc;

/**
 * True when this build's SDK actually has the location service.
 *
 * Compile-time constant, not a runtime probe: it reports whether the header was
 * present when the .fap was built. The UI uses it to say "needs Unleashed"
 * outright rather than leaving a user on official firmware watching a badge that
 * can never fill -- the exact failure mode issue #5 spent four rounds inside.
 */
bool gps_rpc_supported(void);

/** @param app  ReconApp* (passed as void* to avoid a header cycle). */
GpsRpc* gps_rpc_alloc(void* app);
void gps_rpc_free(GpsRpc* rpc);

/** Open the location record, subscribe, and ask the phone to start streaming. */
void gps_rpc_start(GpsRpc* rpc);
/** Stop the stream and unsubscribe. Safe to call when never started. */
void gps_rpc_stop(GpsRpc* rpc);

/**
 * Periodic re-request while the stream is not yet delivering.
 *
 * Called from the GUI tick. A stream request fails outright when no RPC client
 * is attached, and the ordinary case is that the operator opens a scan screen
 * first and connects the phone afterwards -- without this, that session would
 * never get a fix even though the phone is now sitting there connected. Mirrors
 * the ESP relay's gps_cfg_resend for the same reason.
 */
void gps_rpc_tick(GpsRpc* rpc);
