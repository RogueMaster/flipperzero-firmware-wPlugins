#include "hf_read_lifecycle.h"

SeaderHfCardSessionDecision
    seader_hf_read_on_card_detect(SeaderHfReadState state, bool sam_can_accept_card) {
    if(state != SeaderHfReadStateDetecting) {
        return SeaderHfCardSessionDecisionAbort;
    }

    if(!sam_can_accept_card) {
        return SeaderHfCardSessionDecisionAbort;
    }

    return SeaderHfCardSessionDecisionStartConversation;
}

bool seader_hf_read_is_waiting_for_progress(SeaderHfReadState state) {
    return state == SeaderHfReadStateConversationStarting ||
           state == SeaderHfReadStateConversationActive || state == SeaderHfReadStateFinishing;
}

bool seader_hf_read_should_timeout(
    SeaderHfReadState state,
    uint32_t elapsed_ms,
    uint32_t timeout_ms) {
    if(!seader_hf_read_is_waiting_for_progress(state)) {
        return false;
    }

    return elapsed_ms >= timeout_ms;
}

const char* seader_hf_read_failure_reason_text(SeaderHfReadFailureReason reason) {
    switch(reason) {
    case SeaderHfReadFailureReasonUnavailable:
        return "HF unavailable";
    case SeaderHfReadFailureReasonSamBusy:
        return "SAM not idle";
    case SeaderHfReadFailureReasonSamTimeout:
        return "SAM timeout";
    case SeaderHfReadFailureReasonBoardMissing:
        return "Reader lost";
    case SeaderHfReadFailureReasonProtocolError:
        return "Protocol error";
    case SeaderHfReadFailureReasonInternalState:
        return "Read state error";
    case SeaderHfReadFailureReasonNone:
    default:
        return "Read failed";
    }
}
