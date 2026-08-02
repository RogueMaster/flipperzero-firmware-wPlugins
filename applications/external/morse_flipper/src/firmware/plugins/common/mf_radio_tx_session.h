#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../radio/mf_radio_hal.h"

typedef struct {
    const MfRadioHardwareOps* hardware;
    bool prepared;
    bool mark;
} MfRadioTxSession;

void mf_radio_tx_session_init(MfRadioTxSession* session, const MfRadioHardwareOps* hardware);
bool mf_radio_tx_session_frequency_allowed(const MfRadioTxSession* session, uint32_t frequency_hz);
bool mf_radio_tx_session_start(
    MfRadioTxSession* session,
    uint32_t frequency_hz,
    MfRadioTxMode mode);
bool mf_radio_tx_session_set_mark(MfRadioTxSession* session, bool mark);
void mf_radio_tx_session_stop(MfRadioTxSession* session);
