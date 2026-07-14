#include "t_union_poller_i.h"

typedef NfcCommand (*TUnionPollerStateHandler)(TUnionPoller* instance);

TUnionPoller* t_union_poller_alloc(Nfc* nfc, TUnionMessage* msg) {
    furi_assert(nfc);
    TUnionPoller* instance = malloc(sizeof(TUnionPoller));

    instance->poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_4a);

    instance->tx_buffer = bit_buffer_alloc(T_UNION_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(T_UNION_POLLER_MAX_BUFFER_SIZE);

    instance->application.message = msg;

    return instance;
}

void t_union_poller_free(TUnionPoller* instance) {
    furi_assert(instance);
    nfc_poller_free(instance->poller);

    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);

    free(instance);
}

static NfcCommand t_union_poller_handler_idle(TUnionPoller* instance) {
    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);
    t_union_msg_reset(instance->application.message);

    instance->state = TUnionPollerStateSelectPPSE;

    instance->cb(TUnionPollerEventTypeDetected, instance->ctx);
    furi_delay_ms(50);

    return NfcCommandContinue;
}

// select PPSE
static NfcCommand t_union_poller_handler_select_ppse(TUnionPoller* instance) {
    const uint8_t aid[] = "2PAY.SYS.DDF01";

    instance->error = t_union_poller_sesect_application(instance, aid, sizeof(aid) - 1);

    if(instance->error == TUnionPollerErrorNone) {
        FURI_LOG_D(TAG, "Select PPSE success");
        if(!strcmp(instance->application.appl, "MOT_T_EP")) {
            // APP_LABEL 校验成功
            FURI_LOG_D(TAG, "APP_LABEL check success");
            instance->state = TUnionPollerStateSelectApplication;
        } else {
            // APP_LABEL 校验失败
            FURI_LOG_E(TAG, "APP_LABEL check fail");
            instance->error = TUnionPollerErrorInvaildAID;
            instance->state = TUnionPollerStateFail;
        }
    } else {
        FURI_LOG_E(TAG, "Failed to select PPSE err=%u", instance->error);
        instance->state = TUnionPollerStateFail;
    }

    return NfcCommandContinue;
}

// select 应用 AID
static NfcCommand t_union_poller_handler_select_appliction(TUnionPoller* instance) {
    TunionApplication* application = &instance->application;
    instance->error =
        t_union_poller_sesect_application(instance, application->aid, application->aid_len);

    if(instance->error == TUnionPollerErrorNone) {
        FURI_LOG_D(TAG, "Select Application success");
        instance->state = TUnionPollerStateReadBalance;
    } else {
        FURI_LOG_E(TAG, "Failed to select Application err=%u", instance->error);
        instance->state = TUnionPollerStateFail;
    }

    return NfcCommandContinue;
}

// 读余额
static NfcCommand t_union_poller_handler_read_balance(TUnionPoller* instance) {
    instance->error = t_union_poller_read_balance(instance);

    if(instance->error == TUnionPollerErrorNone) {
        FURI_LOG_D(TAG, "Read Balance success");
        instance->state = TUnionPollerStateReadTransactions;
    } else {
        FURI_LOG_E(TAG, "Failed to Read Balance err=%u", instance->error);
        instance->state = TUnionPollerStateFail;
    }

    return NfcCommandContinue;
}

// 读交易记录
static NfcCommand t_union_poller_handler_read_transactions(TUnionPoller* instance) {
    instance->error = t_union_poller_read_transactions(instance);

    if(instance->error == TUnionPollerErrorNone) {
        FURI_LOG_D(TAG, "Read Transaction success");
        instance->state = TUnionPollerStateReadTravels;
    } else {
        FURI_LOG_E(TAG, "Failed to Read Transaction err=%u", instance->error);
        instance->state = TUnionPollerStateFail;
    }

    return NfcCommandContinue;
}

// 读行程记录
static NfcCommand t_union_poller_handler_read_travels(TUnionPoller* instance) {
    instance->error = t_union_poller_read_travels(instance);

    if(instance->error == TUnionPollerErrorNone) {
        FURI_LOG_D(TAG, "Read Travels success");
        instance->state = TUnionPollerStateSuccess;
    } else {
        FURI_LOG_E(TAG, "Failed to Read Travels err=%u", instance->error);
        instance->state = TUnionPollerStateFail;
    }

    return NfcCommandContinue;
}

// 读取成功
static NfcCommand t_union_poller_handler_success(TUnionPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    command = instance->cb(TUnionPollerEventTypeSuccess, instance->ctx);
    if(command != NfcCommandStop) {
        furi_delay_ms(100);
    }

    return command;
}

// 读取失败
static NfcCommand t_union_poller_handler_fail(TUnionPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    command = instance->cb(TUnionPollerEventTypeFail, instance->ctx);
    if(command != NfcCommandStop) {
        furi_delay_ms(100);
    }

    return command;
}

static const TUnionPollerStateHandler t_union_poller_state_handlers[TUnionPollerStateNum] = {
    [TUnionPollerStateIdle] = t_union_poller_handler_idle,
    [TUnionPollerStateSelectPPSE] = t_union_poller_handler_select_ppse,
    [TUnionPollerStateSelectApplication] = t_union_poller_handler_select_appliction,
    [TUnionPollerStateReadBalance] = t_union_poller_handler_read_balance,
    [TUnionPollerStateReadTransactions] = t_union_poller_handler_read_transactions,
    [TUnionPollerStateReadTravels] = t_union_poller_handler_read_travels,

    [TUnionPollerStateSuccess] = t_union_poller_handler_success,
    [TUnionPollerStateFail] = t_union_poller_handler_fail,

};

static NfcCommand t_union_poller_cb(NfcGenericEvent event, void* ctx) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);

    TUnionPoller* instance = ctx;
    furi_assert(instance);

    furi_assert(event.event_data);
    furi_assert(event.instance);

    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;
    furi_assert(iso14443_4a_event);
    instance->iso14443_4a_poller = event.instance;

    NfcCommand command = NfcCommandContinue;

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        command = t_union_poller_state_handlers[instance->state](instance);
    }
    return command;
}

TUnionPollerError t_union_poller_get_error(TUnionPoller* instance) {
    return instance->error;
}

void t_union_poller_start(TUnionPoller* instance, TUnionPollerCallback cb, void* ctx) {
    furi_assert(instance);
    furi_assert(cb);

    instance->cb = cb;
    instance->ctx = ctx;

    instance->state = TUnionPollerStateIdle;
    nfc_poller_start(instance->poller, t_union_poller_cb, instance);
}

void t_union_poller_stop(TUnionPoller* instance) {
    furi_assert(instance);

    nfc_poller_stop(instance->poller);
}
