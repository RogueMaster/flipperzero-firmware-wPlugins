#pragma once

#include "sam_key_label.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    SeaderHfPacsMediaTypeUnknown = 0,
    SeaderHfPacsMediaTypeDesfire = 1,
    SeaderHfPacsMediaTypeMifare = 2,
    SeaderHfPacsMediaTypePicopass = 3,
    SeaderHfPacsMediaTypeMifarePlus = 6,
    SeaderHfPacsMediaTypeSeos = 7,
} SeaderHfPacsMediaType;

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
    SeaderHfReadFailureReasonSamKeysMissing,
    SeaderHfReadFailureReasonResourceExhausted,
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
void seader_hf_read_prepare_context(
    SeaderHfReadFailureReason* failure_reason,
    char* read_error,
    size_t read_error_size);
bool seader_pacs2_indicates_sam_keys_missing(
    bool has_media_type,
    const uint8_t* pacs_bits,
    size_t pacs_bits_size);
void seader_hf_read_format_sam_keys_missing_error(
    bool has_media_type,
    SeaderHfPacsMediaType media_type,
    bool standard_pacs_keys_probed,
    bool standard_pacs_keys_present,
    char* out,
    size_t out_size);
