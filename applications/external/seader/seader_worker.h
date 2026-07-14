#pragma once

#include "seader.h"
#include "sam_api.h"
#include "seader_credential.h"
#include "seader_bridge.h"
#include "apdu_runner.h"

typedef struct SeaderWorker SeaderWorker;
typedef struct CCID_Message CCID_Message;
typedef struct SeaderAPDU SeaderAPDU;

typedef enum {
    // Init states
    SeaderWorkerStateNone,
    SeaderWorkerStateBroken,
    SeaderWorkerStateReady,
    // Main worker states
    SeaderWorkerStateCheckSam,
    SeaderWorkerStateVirtualCredential,
    SeaderWorkerStateAPDURunner,
    SeaderWorkerStateReading,
    SeaderWorkerStateHfTeardown,
    // Transition
    SeaderWorkerStateStop,
} SeaderWorkerState;

typedef enum {
    // Reserve first 50 events for application events
    SeaderWorkerEventReserved = 50,

    // Seader worker common events
    SeaderWorkerEventSuccess,
    SeaderWorkerEventFail,
    SeaderWorkerEventSamPresent,
    SeaderWorkerEventSamWrong,
    SeaderWorkerEventSamMissing,
    SeaderWorkerEventNoCardDetected,
    SeaderWorkerEventStartReading,
    SeaderWorkerEventSelectCardType,
    SeaderWorkerEventAPDURunnerUpdate,
    SeaderWorkerEventAPDURunnerSuccess,
    SeaderWorkerEventAPDURunnerError,
    SeaderWorkerEventHfTeardownComplete,
} SeaderWorkerEvent;

typedef enum {
    SeaderPollerEventTypeCardDetect,
    SeaderPollerEventTypeConversation,
    SeaderPollerEventTypeComplete,

    SeaderPollerEventTypeSuccess,
    SeaderPollerEventTypeFail,
} SeaderPollerEventType;

typedef void (*SeaderWorkerCallback)(uint32_t event, void* context);

SeaderWorker* seader_worker_alloc();

SeaderWorkerState seader_worker_get_state(SeaderWorker* seader_worker);

void seader_worker_free(SeaderWorker* seader_worker);

void seader_worker_start(
    SeaderWorker* seader_worker,
    SeaderWorkerState state,
    SeaderUartBridge* uart,
    SeaderWorkerCallback callback,
    void* context);

void seader_worker_stop(SeaderWorker* seader_worker);
void seader_worker_join(SeaderWorker* seader_worker);
bool seader_worker_process_sam_message(Seader* seader, uint8_t* apdu, uint32_t len);
void seader_worker_send_version(Seader* seader);
void seader_worker_cancel_poller_session(SeaderWorker* seader_worker);
void seader_worker_reset_poller_session(SeaderWorker* seader_worker);
void seader_worker_run_hf_conversation(Seader* seader);
