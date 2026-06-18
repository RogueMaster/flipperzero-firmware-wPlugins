#include "hf_read_lifecycle.h"

#include <stdio.h>

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
    case SeaderHfReadFailureReasonSamKeysMissing:
        return "SAM missing keys";
    case SeaderHfReadFailureReasonResourceExhausted:
        return "SAM exchange memory error";
    case SeaderHfReadFailureReasonNone:
    default:
        return "Read failed";
    }
}

void seader_hf_read_prepare_context(
    SeaderHfReadFailureReason* failure_reason,
    char* read_error,
    size_t read_error_size) {
    if(failure_reason) {
        *failure_reason = SeaderHfReadFailureReasonNone;
    }

    if(read_error && read_error_size > 0U) {
        read_error[0] = '\0';
    }
}

bool seader_pacs2_indicates_sam_keys_missing(
    bool has_media_type,
    const uint8_t* pacs_bits,
    size_t pacs_bits_size) {
    if(!has_media_type) {
        return false;
    }

    return !pacs_bits || pacs_bits_size < 2U;
}

static const char* seader_hf_read_media_type_label(SeaderHfPacsMediaType media_type) {
    switch(media_type) {
    case SeaderHfPacsMediaTypeDesfire:
        return "DESFire";
    case SeaderHfPacsMediaTypeMifare:
        return "MIFARE";
    case SeaderHfPacsMediaTypePicopass:
        return "PicoPass";
    case SeaderHfPacsMediaTypeMifarePlus:
        return "MIFARE Plus";
    case SeaderHfPacsMediaTypeSeos:
        return "Seos";
    case SeaderHfPacsMediaTypeUnknown:
    default:
        return NULL;
    }
}

void seader_hf_read_format_sam_keys_missing_error(
    bool has_media_type,
    SeaderHfPacsMediaType media_type,
    bool standard_pacs_keys_probed,
    bool standard_pacs_keys_present,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';

    const char* media_label = has_media_type ? seader_hf_read_media_type_label(media_type) : NULL;
    const bool standard_keys_missing = standard_pacs_keys_probed && !standard_pacs_keys_present;

    if(media_label && standard_keys_missing) {
        snprintf(
            out,
            out_size,
            "%s recognized.\nUnable to read keys.\nSAM missing standard keys.",
            media_label);
        return;
    }

    if(media_label) {
        snprintf(
            out, out_size, "%s recognized.\nUnable to read keys.\nCheck SAM Info.", media_label);
        return;
    }

    if(standard_keys_missing) {
        snprintf(
            out, out_size, "Unable to read keys.\nSAM missing standard\nkeys. Check SAM Info.");
        return;
    }

    snprintf(out, out_size, "Unable to read keys.\nCheck SAM Info.");
}
