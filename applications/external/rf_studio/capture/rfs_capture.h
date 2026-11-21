#pragma once

#include "../radio/rfs_radio.h"
#include "../protocol/rfs_protocol.h"

/*
 * Signal Capture Engine (Specification Section 9, Addendum 001 Section 4).
 *
 * Discovery: sweeps the firmware's frequency list on a background thread, reading RSSI, and
 *   reports the strongest frequency above the trigger level (CAP-ADD-001).
 * Listening: decodes with every firmware protocol while recording a RAW copy as soon as a
 *   signal appears, so an undecodable signal can still be kept (CAP-ADD-006).
 *
 * Callbacks fire on worker threads; the UI forwards them as custom events.
 */

typedef enum {
    RfsCaptureStateIdle,
    RfsCaptureStateDiscovering,
    RfsCaptureStateListening,
    RfsCaptureStateDone,
} RfsCaptureState;

typedef enum {
    RfsCaptureEventFrequencyFound,
    RfsCaptureEventDecoded,
    RfsCaptureEventRawReady,
    RfsCaptureEventError,
} RfsCaptureEvent;

typedef void (*RfsCaptureCallback)(RfsCaptureEvent event, void* context);

typedef struct {
    bool valid;
    bool decoded; /* protocol decoded (otherwise raw) */
    bool confirmed; /* decoded identically from two frames */
    RfsSignalKind kind;
    char protocol[RFS_PROTOCOL_LEN];
    char preset[RFS_PRESET_LEN];
    uint32_t frequency;
    size_t raw_samples;
    FuriString* text; /* decoder summary or raw description */
} RfsCaptureResult;

typedef struct RfsCapture RfsCapture;

RfsCapture* rfs_capture_alloc(RfsRadio* radio, RfsDiag* diag);
void rfs_capture_free(RfsCapture* capture);

void rfs_capture_set_callback(RfsCapture* capture, RfsCaptureCallback callback, void* context);

/** Begin automatic frequency discovery on a radio. */
RfsRadioResult rfs_capture_start_discovery(
    RfsCapture* capture,
    RfsDeviceId device,
    RfsDiscoverScope scope,
    int32_t rssi_threshold);

/** Listen on a fixed frequency and preset. Switches straight from discovery when running. */
RfsRadioResult rfs_capture_start_listen(
    RfsCapture* capture,
    RfsDeviceId device,
    uint32_t frequency,
    const char* preset,
    int32_t rssi_threshold);

/** Stop the radio and keep any result. Safe to call repeatedly. */
void rfs_capture_stop(RfsCapture* capture);

/** Periodic update from the UI thread (about 10 Hz): RSSI and raw start and end detection. */
void rfs_capture_tick(RfsCapture* capture);

/** End raw recording now on user request. Returns true if a usable recording exists. */
bool rfs_capture_finish_raw(RfsCapture* capture);

RfsCaptureState rfs_capture_state(RfsCapture* capture);
float rfs_capture_rssi(RfsCapture* capture);
uint32_t rfs_capture_frequency(RfsCapture* capture);
uint32_t rfs_capture_sweep_frequency(RfsCapture* capture);
size_t rfs_capture_raw_samples(RfsCapture* capture);
bool rfs_capture_raw_active(RfsCapture* capture);
uint32_t rfs_capture_overruns(RfsCapture* capture);
RfsDeviceId rfs_capture_device(RfsCapture* capture);

/** Result of the last capture, valid after Decoded or RawReady. */
const RfsCaptureResult* rfs_capture_result(RfsCapture* capture);

/** The decoded key ("Key" value) of the result, for duplicate checks. */
bool rfs_capture_result_key(RfsCapture* capture, FuriString* out);

/** Write the result as a .sub file at dest_path. */
bool rfs_capture_save(RfsCapture* capture, Storage* storage, const char* dest_path);

/** Drop the result and any temporary recording. */
void rfs_capture_discard(RfsCapture* capture);

/** Text for the last error event. */
const char* rfs_capture_error_text(RfsCapture* capture);
