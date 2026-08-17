// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "gps_rpc.h"

// THE FIRMWARE GATE. Unleashed's location service declares sdk_headers=["gps.h"]
// (applications/services/gps/application.fam), so its SDK ships <gps/gps.h> and
// exports the five gps_* symbols; official firmware and Momentum ship neither.
// CI builds this one tree against all three SDKs, so the availability test has to
// follow the SDK in use rather than a flag somebody remembers to set -- which is
// exactly what __has_include does. Everything that touches the service is inside
// this branch; the #else below is a complete, silent no-op implementation.
#if defined(__has_include)
#if __has_include(<gps/gps.h>)
#define RECON_HAS_GPS_RPC 1
#endif
#endif

#ifdef RECON_HAS_GPS_RPC

#include "../recon_app_i.h"
#include "gps_link.h"
#include "gps_rpc_convert.h"

#include <gps/gps.h>

#include <flipper_application/elf/elf_api_interface.h>
#include <loader/firmware_api/firmware_api.h>

#include <stdlib.h>
#include <string.h>

/**
 * Stream rate, Hz. The service accepts 1..10.
 *
 * 1 Hz on purpose. It matches what a UART GPS module emits, it is already faster
 * than detections are recorded, and every sample above it is the PHONE's GNSS
 * radio and BLE link staying awake -- on somebody else's battery, during a scan
 * that may run for hours. Higher rates buy nothing this app can use.
 */
#define GPS_RPC_STREAM_HZ 1

/** How long to wait before re-asking, while the stream is not delivering. */
#define GPS_RPC_RETRY_MS 3000

typedef bool (*GpsRequestStreamFn)(Gps* gps, uint8_t frequency);
typedef bool (*GpsStopStreamFn)(Gps* gps);
typedef void (*GpsSetLocationCallbackFn)(Gps* gps, GpsLocationCallback callback, void* context);

/**
 * Resolve an optional firmware API symbol without putting it in the FAP's
 * mandatory import table.
 *
 * API-compatible firmware variants do not necessarily ship the same optional
 * services. Unleashed exposes the GPS service, while RogueMaster can report
 * the same API version without exporting its symbols. A normal call would make
 * the loader reject the whole app with "Missing Imports" before it can show the
 * GPS feature as unavailable. Calling the firmware resolver directly keeps the
 * dependency optional and lets the ordinary unsupported-state UI handle that
 * variant.
 */
static uint32_t gps_rpc_symbol_hash(const char* name) {
    uint32_t hash = 0x1505;
    for(const unsigned char* p = (const unsigned char*)name; *p != '\0'; p++) {
        hash = (hash << 5) + hash + *p;
    }
    return hash;
}

static bool gps_rpc_resolve(const char* name, Elf32_Addr* address) {
    return firmware_api_interface && firmware_api_interface->resolver_callback &&
           firmware_api_interface->resolver_callback(
               firmware_api_interface, gps_rpc_symbol_hash(name), address);
}

struct GpsRpc {
    ReconApp* app;
    Gps* gps; /**< the furi record, NULL when the service is absent */
    bool subscribed; /**< our callback is installed (so stop() must remove it) */
    bool streaming; /**< a stream request has been accepted at least once */
    uint32_t retry_tick; /**< tick of the last stream request */
    GpsRequestStreamFn request_stream;
    GpsStopStreamFn stop_stream;
    GpsSetLocationCallbackFn set_location_callback;
};

bool gps_rpc_supported(void) {
    return true;
}

static void gps_rpc_set_state(GpsRpc* rpc, ReconGpsPhoneState state) {
    ReconApp* app = rpc->app;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->gps_phone = (uint8_t)state;
    furi_mutex_release(app->mutex);
}

/**
 * RPC location callback. Runs on the RPC service's thread, NOT the GUI thread,
 * which is why every write below goes through a mutex-taking helper.
 */
static void gps_rpc_on_location(GpsStatus status, const GpsLocation* location, void* context) {
    GpsRpc* rpc = context;
    if(!rpc || !rpc->app) return;

    // The phone answered with a refusal. Each one needs a different fix from the
    // operator, so they stay distinct all the way to the badge rather than
    // collapsing into "no fix".
    if(status != GpsStatusOk || !location) {
        ReconGpsPhoneState s;
        switch(status) {
        case GpsStatusNoPermission:
            s = ReconGpsPhoneNoPermission;
            break;
        case GpsStatusDisabled:
            s = ReconGpsPhoneDisabled;
            break;
        case GpsStatusNotSupported:
            s = ReconGpsPhoneNoFix;
            break;
        default:
            s = ReconGpsPhoneError;
            break;
        }
        // CLEAR the fix, do not just relabel it. A revoked permission or a
        // switched-off location service means the last position is now of
        // unknown age, and a stale fix silently geotags detections with a place
        // you have since driven away from.
        gps_publish_fix(rpc->app, 0.0f, 0.0f, -1, false);
        gps_rpc_set_state(rpc, s);
        return;
    }

    GpsRpcFix fix;
    if(!gps_rpc_convert(
           location->latitude,
           location->longitude,
           location->heading,
           location->speed,
           location->accuracy,
           location->satellites,
           &fix)) {
        return;
    }

    gps_publish_fix(rpc->app, fix.lat, fix.lon, fix.sats, fix.valid);

    ReconApp* app = rpc->app;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(fix.has_course) app->gps_course = fix.course;
    // "Answering, but useless" is its own verdict -- see ReconGpsPhoneCoarse.
    app->gps_phone = (uint8_t)(fix.valid ? ReconGpsPhoneStreaming : ReconGpsPhoneCoarse);
    furi_mutex_release(app->mutex);
}

/** Ask for the stream; records whether an RPC client was there to hear it. */
static void gps_rpc_request(GpsRpc* rpc) {
    rpc->retry_tick = furi_get_tick();
    // False means the service has no RPC bridge attached, i.e. nothing is paired
    // -- not that the phone refused. Distinct states, distinct fixes.
    if(rpc->request_stream(rpc->gps, GPS_RPC_STREAM_HZ)) {
        rpc->streaming = true;
        gps_rpc_set_state(rpc, ReconGpsPhoneWaiting);
    } else {
        rpc->streaming = false;
        gps_rpc_set_state(rpc, ReconGpsPhoneNoClient);
    }
}

GpsRpc* gps_rpc_alloc(void* app) {
    GpsRpc* rpc = malloc(sizeof(GpsRpc));
    memset(rpc, 0, sizeof(GpsRpc));
    rpc->app = app;
    return rpc;
}

void gps_rpc_free(GpsRpc* rpc) {
    if(!rpc) return;
    gps_rpc_stop(rpc);
    free(rpc);
}

void gps_rpc_start(GpsRpc* rpc) {
    if(!rpc || rpc->gps) return;

    Elf32_Addr address;
    if(!gps_rpc_resolve("gps_request_stream", &address)) {
        gps_rpc_set_state(rpc, ReconGpsPhoneUnsupported);
        return;
    }
    rpc->request_stream = (GpsRequestStreamFn)(uintptr_t)address;
    if(!gps_rpc_resolve("gps_stop_stream", &address)) {
        gps_rpc_set_state(rpc, ReconGpsPhoneUnsupported);
        return;
    }
    rpc->stop_stream = (GpsStopStreamFn)(uintptr_t)address;
    if(!gps_rpc_resolve("gps_set_location_callback", &address)) {
        gps_rpc_set_state(rpc, ReconGpsPhoneUnsupported);
        return;
    }
    rpc->set_location_callback = (GpsSetLocationCallbackFn)(uintptr_t)address;

    // furi_record_open() BLOCKS FOREVER on a record that was never created, so it
    // must not be called speculatively. The header can be present while the
    // service is not (a firmware built with the GPS service disabled), and that
    // combination would hang the app on entering a scan screen rather than
    // degrading. Checking first turns that into an honest badge.
    if(!furi_record_exists(RECORD_GPS)) {
        gps_rpc_set_state(rpc, ReconGpsPhoneUnsupported);
        return;
    }

    rpc->gps = furi_record_open(RECORD_GPS);
    rpc->set_location_callback(rpc->gps, gps_rpc_on_location, rpc);
    rpc->subscribed = true;
    gps_rpc_request(rpc);
}

void gps_rpc_stop(GpsRpc* rpc) {
    if(!rpc || !rpc->gps) return;

    if(rpc->streaming) rpc->stop_stream(rpc->gps);
    if(rpc->subscribed) {
        // Unsubscribe BEFORE closing the record: the callback holds this GpsRpc
        // as its context and fires from the RPC thread, so leaving it installed
        // past teardown is a use-after-free waiting for one more location.
        rpc->set_location_callback(rpc->gps, NULL, NULL);
        rpc->subscribed = false;
    }
    furi_record_close(RECORD_GPS);
    rpc->gps = NULL;
    rpc->streaming = false;
    gps_rpc_set_state(rpc, ReconGpsPhoneOff);
}

void gps_rpc_tick(GpsRpc* rpc) {
    if(!rpc || !rpc->gps) return;

    // Already delivering -> nothing to chase.
    ReconApp* app = rpc->app;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    ReconGpsPhoneState state = (ReconGpsPhoneState)app->gps_phone;
    furi_mutex_release(app->mutex);
    if(state == ReconGpsPhoneStreaming) return;

    // A refusal is not a retry case: re-asking a phone that has denied location
    // permission just wakes its radio every three seconds forever, and the
    // operator has to go change a setting either way.
    if(state == ReconGpsPhoneNoPermission || state == ReconGpsPhoneDisabled ||
       state == ReconGpsPhoneNoFix) {
        return;
    }

    if(furi_get_tick() - rpc->retry_tick < furi_ms_to_ticks(GPS_RPC_RETRY_MS)) return;
    gps_rpc_request(rpc);
}

#else // !RECON_HAS_GPS_RPC

// Official firmware and Momentum. The service does not exist in these SDKs, so
// there is nothing to call and nothing to link. Keeping real (empty) functions
// rather than #ifdef-ing out the call sites means the wiring in scan_session.c
// and recon_app.c is identical on all three targets, with one place -- here --
// that knows about the difference.

#include "../recon_app_i.h"

#include <stdlib.h>
#include <string.h>

struct GpsRpc {
    ReconApp* app;
};

bool gps_rpc_supported(void) {
    return false;
}

GpsRpc* gps_rpc_alloc(void* app) {
    GpsRpc* rpc = malloc(sizeof(GpsRpc));
    memset(rpc, 0, sizeof(GpsRpc));
    rpc->app = app;
    // Say so immediately: selecting the phone source on a build that cannot do it
    // is a permanent, fixable condition, and the badge should name it at once
    // instead of showing "searching" for a stream that will never start.
    ReconApp* a = app;
    if(a) {
        furi_mutex_acquire(a->mutex, FuriWaitForever);
        a->gps_phone = (uint8_t)ReconGpsPhoneUnsupported;
        furi_mutex_release(a->mutex);
    }
    return rpc;
}

void gps_rpc_free(GpsRpc* rpc) {
    if(rpc) free(rpc);
}

void gps_rpc_start(GpsRpc* rpc) {
    UNUSED(rpc);
}

void gps_rpc_stop(GpsRpc* rpc) {
    UNUSED(rpc);
}

void gps_rpc_tick(GpsRpc* rpc) {
    UNUSED(rpc);
}

#endif // RECON_HAS_GPS_RPC
