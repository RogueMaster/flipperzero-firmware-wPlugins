#pragma once

#include "tpms_decoder.h"

#include <stdbool.h>
#include <stdint.h>

/** Receive session: owns the radio and the decoder.
 *
 * Decoding does not run in a thread of its own — the owner drives it by
 * calling tpms_session_pump(). That way the same code serves both the CLI
 * command thread (where stdout is available) and the local receive thread
 * running on the Flipper itself.
 */
typedef struct TpmsSession TpmsSession;

/** A frame that passed its protocol's checks.
 *
 * rssi_dbm is sampled when the batch of intervals arrives from the radio
 * rather than after decoding, so the value belongs to the transmission
 * itself.
 */
typedef void (*TpmsSessionFrameCallback)(const TpmsFrame* frame, float rssi_dbm, void* context);

/** Raw interval (diagnostic mode). */
typedef void (*TpmsSessionRawCallback)(bool level, uint32_t duration, void* context);

TpmsSession* tpms_session_alloc(void);
void tpms_session_free(TpmsSession* session);

void tpms_session_set_frame_callback(
    TpmsSession* session,
    TpmsSessionFrameCallback callback,
    void* context);

/** Enable raw interval output. Pass NULL to disable. */
void tpms_session_set_raw_callback(
    TpmsSession* session,
    TpmsSessionRawCallback callback,
    void* context);

bool tpms_session_start(TpmsSession* session, uint32_t frequency, TpmsModulation modulation);
void tpms_session_stop(TpmsSession* session);

/** Retune the running session.
 *
 * The radio holds one configuration at a time, so listening for the OOK
 * protocols means not listening for the FSK ones, and the same goes for
 * the two frequency bands. Both are changed without tearing the session
 * down, which keeps the wake-up timer and the CLI stream alive.
 */
bool tpms_session_retune(TpmsSession* session, uint32_t frequency, TpmsModulation modulation);

/** Decode what has been buffered. Returns the number of intervals handled. */
size_t tpms_session_pump(TpmsSession* session, uint32_t timeout_ms);

/** Emit a 125 kHz LF field pulse without interrupting reception.
 *
 * The sensor answers almost immediately after activation, so the receiver
 * must not be muted for the duration of the pulse — decoding keeps running
 * the whole time.
 */
void tpms_session_wake_pulse(TpmsSession* session, uint32_t duration_ms);

float tpms_session_get_rssi(TpmsSession* session);

/** How many intervals were lost to buffer overflow. */
uint32_t tpms_session_get_overruns(TpmsSession* session);
