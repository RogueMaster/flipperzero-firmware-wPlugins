#include "hf_bridge_policy.h"

int seader_hf_bridge_begin_conversation(
    void* context,
    const SeaderHfBridgeConversationOps* ops,
    int stop_command) {
    if(!ops || !ops->set_conversation || !ops->begin_card_session || !ops->set_fail ||
       !ops->run_conversation) {
        return stop_command;
    }

    ops->set_conversation(context);
    if(!ops->begin_card_session(context)) {
        ops->set_fail(context);
        return stop_command;
    }

    return ops->run_conversation(context);
}

uint16_t seader_hf_bridge_rf_status_code(SeaderHfBridgeRfStatus status) {
    switch(status) {
    case SeaderHfBridgeRfStatusSuccess:
        return 0x0000U;
    case SeaderHfBridgeRfStatusTimeout:
        return 0x0020U;
    case SeaderHfBridgeRfStatusCrc:
    case SeaderHfBridgeRfStatusProtocol:
    default:
        return 0x0004U;
    }
}

void seader_hf_bridge_rf_status_bytes(SeaderHfBridgeRfStatus status, uint8_t bytes[2]) {
    if(!bytes) {
        return;
    }

    const uint16_t code = seader_hf_bridge_rf_status_code(status);
    bytes[0] = (uint8_t)((code >> 8) & 0xFFU);
    bytes[1] = (uint8_t)(code & 0xFFU);
}

SeaderHfBridgeApduDecision seader_hf_bridge_apdu_decision(
    bool virtual_credential,
    bool conversation_stage,
    size_t len,
    size_t max_len,
    bool queue_has_space) {
    if(!virtual_credential && !conversation_stage) {
        return SeaderHfBridgeApduDecisionDiscardStale;
    }

    if(len > max_len || !queue_has_space) {
        return SeaderHfBridgeApduDecisionFailProtocol;
    }

    return SeaderHfBridgeApduDecisionQueue;
}

uint32_t seader_hf_bridge_timeout_us_to_fwt_fc(uint32_t timeout_us) {
    if(timeout_us == 0U) {
        return 0U;
    }

    const uint64_t fwt_fc = ((uint64_t)timeout_us * 1356U + 99U) / 100U;
    if(fwt_fc > UINT32_MAX) {
        return UINT32_MAX;
    }

    return (uint32_t)fwt_fc;
}
