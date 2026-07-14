#pragma once

#include "t_union_poller.h"

#include <furi.h>
#include <toolbox/bit_buffer.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#define TAG "T-UnionPoller"

#define T_UNION_POLLER_MAX_BUFFER_SIZE 256U

typedef enum {
    TUnionPollerStateIdle,
    TUnionPollerStateSelectPPSE,
    TUnionPollerStateSelectApplication,
    TUnionPollerStateReadBalance,
    TUnionPollerStateReadTransactions,
    TUnionPollerStateReadTravels,

    TUnionPollerStateSuccess,
    TUnionPollerStateFail,

    TUnionPollerStateNum,
} TUnionPollerState;

typedef struct {
    uint8_t aid[16];
    uint8_t aid_len;
    char appl[16 + 1];
    uint8_t priority;

    TUnionMessage* message;
} TunionApplication;

struct TUnionPoller {
    NfcPoller* poller;
    Iso14443_4aPoller* iso14443_4a_poller;
    TUnionPollerState state;
    TunionApplication application;

    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    TUnionPollerCallback cb;
    void* ctx;
    TUnionPollerError error;
};
