#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SeaderHfReadStateIdle = 0,
    SeaderHfReadStateDetecting,
    SeaderHfReadStateConversationStarting,
    SeaderHfReadStateConversationActive,
    SeaderHfReadStateFinishing,
    SeaderHfReadStateTerminalSuccess,
    SeaderHfReadStateTerminalFail,
} SeaderHfReadState;

typedef enum {
    SeaderHfReadFailureReasonNone = 0,
    SeaderHfReadFailureReasonUnavailable,
    SeaderHfReadFailureReasonSamBusy,
    SeaderHfReadFailureReasonSamTimeout,
    SeaderHfReadFailureReasonBoardMissing,
    SeaderHfReadFailureReasonProtocolError,
    SeaderHfReadFailureReasonInternalState,
} SeaderHfReadFailureReason;

typedef enum {
    SeaderHfCardSessionDecisionIgnore = 0,
    SeaderHfCardSessionDecisionStartConversation,
    SeaderHfCardSessionDecisionAbort,
} SeaderHfCardSessionDecision;

SeaderHfCardSessionDecision
    seader_hf_read_on_card_detect(SeaderHfReadState state, bool sam_can_accept_card);
bool seader_hf_read_is_waiting_for_progress(SeaderHfReadState state);
bool seader_hf_read_should_timeout(
    SeaderHfReadState state,
    uint32_t elapsed_ms,
    uint32_t timeout_ms);
const char* seader_hf_read_failure_reason_text(SeaderHfReadFailureReason reason);
