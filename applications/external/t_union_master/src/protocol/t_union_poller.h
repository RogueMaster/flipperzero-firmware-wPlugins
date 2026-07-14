#pragma once

#include <nfc/nfc.h>
#include "t_union_msg.h"

typedef enum {
    TUnionPollerEventTypeSuccess,
    TUnionPollerEventTypeFail,
    TUnionPollerEventTypeDetected,
} TUnionPollerEvent;

typedef enum {
    TUnionPollerErrorNone = 0,
    TUnionPollerErrorNotPresent,
    TUnionPollerErrorTimeout,
    TUnionPollerErrorProtocol,
    TUnionPollerErrorInvaildAID,
} TUnionPollerError;

typedef NfcCommand (*TUnionPollerCallback)(TUnionPollerEvent event, void* ctx);

typedef struct TUnionPoller TUnionPoller;

TUnionPollerError
    t_union_poller_sesect_application(TUnionPoller* instance, const uint8_t* aid, uint8_t aid_len);
TUnionPollerError t_union_poller_read_balance(TUnionPoller* instance);
TUnionPollerError t_union_poller_read_transactions(TUnionPoller* instance);
TUnionPollerError t_union_poller_read_travels(TUnionPoller* instance);

TUnionPollerError t_union_poller_get_error(TUnionPoller* instance);

TUnionPoller* t_union_poller_alloc(Nfc* nfc, TUnionMessage* msg);
void t_union_poller_free(TUnionPoller* instance);

void t_union_poller_start(TUnionPoller* instance, TUnionPollerCallback cb, void* ctx);
void t_union_poller_stop(TUnionPoller* instance);
