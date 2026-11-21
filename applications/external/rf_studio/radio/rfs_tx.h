#pragma once

#include "rfs_radio.h"

/*
 * Transmission Engine (Specification Section 11).
 * Runs one transmission at a time on its own thread. Every operation ends in Completed, Failed
 * or Cancelled, and the radio is always returned to sleep.
 */

typedef enum {
    RfsTxStateIdle,
    RfsTxStateValidating,
    RfsTxStatePreparing,
    RfsTxStateTransmitting,
    RfsTxStateCompleted,
    RfsTxStateFailed,
    RfsTxStateCancelled,
} RfsTxState;

typedef enum {
    RfsTxErrNone,
    RfsTxErrFileMissing,
    RfsTxErrFileInvalid,
    RfsTxErrProtocolUnknown,
    RfsTxErrRollingCode,
    RfsTxErrNotSendable,
    RfsTxErrRadio, /* see radio_result */
    RfsTxErrPreset,
    RfsTxErrEncoder,
    RfsTxErrStart,
    RfsTxErrTimeout,
} RfsTxError;

/** Called from the transmission thread when the operation finishes. */
typedef void (*RfsTxDoneCallback)(void* context);

typedef struct RfsTx RfsTx;

RfsTx* rfs_tx_alloc(RfsRadio* radio, RfsDiag* diag);
void rfs_tx_free(RfsTx* tx);

/** Start sending a .sub file. Returns false if a transmission is already running. */
bool rfs_tx_start(
    RfsTx* tx,
    const char* signal_path,
    RfsRadioMode mode,
    RfsTxDoneCallback callback,
    void* context);

/** Request cancellation (TX-009). The done callback still fires. */
void rfs_tx_cancel(RfsTx* tx);

bool rfs_tx_is_running(RfsTx* tx);
RfsTxState rfs_tx_state(RfsTx* tx);

/** Short human-readable outcome for the status line, such as "Sent" or "Blocked in region". */
void rfs_tx_outcome_text(RfsTx* tx, char* out, size_t size);

/** Name of the radio used for the last transmission. */
const char* rfs_tx_device_label(RfsTx* tx);
