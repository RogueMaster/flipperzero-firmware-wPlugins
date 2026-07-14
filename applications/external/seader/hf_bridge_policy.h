#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SeaderHfBridgeRfStatusSuccess = 0,
    SeaderHfBridgeRfStatusTimeout,
    SeaderHfBridgeRfStatusCrc,
    SeaderHfBridgeRfStatusProtocol,
} SeaderHfBridgeRfStatus;

typedef enum {
    SeaderHfBridgeApduDecisionDiscardStale = 0,
    SeaderHfBridgeApduDecisionQueue,
    SeaderHfBridgeApduDecisionFailProtocol,
} SeaderHfBridgeApduDecision;

typedef struct {
    void (*set_conversation)(void* context);
    bool (*begin_card_session)(void* context);
    void (*set_fail)(void* context);
    int (*run_conversation)(void* context);
} SeaderHfBridgeConversationOps;

int seader_hf_bridge_begin_conversation(
    void* context,
    const SeaderHfBridgeConversationOps* ops,
    int stop_command);

uint16_t seader_hf_bridge_rf_status_code(SeaderHfBridgeRfStatus status);
void seader_hf_bridge_rf_status_bytes(SeaderHfBridgeRfStatus status, uint8_t bytes[2]);
SeaderHfBridgeApduDecision seader_hf_bridge_apdu_decision(
    bool virtual_credential,
    bool conversation_stage,
    size_t len,
    size_t max_len,
    bool queue_has_space);

/* SAM nfcSend.timeOut is microseconds; Flipper NFC uses 13.56 MHz carrier cycles. */
uint32_t seader_hf_bridge_timeout_us_to_fwt_fc(uint32_t timeout_us);
