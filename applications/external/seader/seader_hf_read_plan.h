#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "seader_credential_type.h"

typedef enum {
    SeaderHfReadDecisionContinuePolling,
    SeaderHfReadDecisionStartRead,
    SeaderHfReadDecisionSelectType,
} SeaderHfReadDecision;

typedef struct {
    SeaderHfReadDecision decision;
    SeaderCredentialType type_to_read;
    SeaderCredentialType detected_types[3];
    size_t detected_type_count;
} SeaderHfReadPlan;

SeaderHfReadPlan seader_hf_read_plan_build(
    SeaderCredentialType selected_type,
    const SeaderCredentialType* detected_types,
    size_t detected_type_count);
static inline bool seader_hf_read_plan_should_verify_start_type(
    SeaderCredentialType type_to_read,
    const SeaderCredentialType* detected_types,
    size_t detected_type_count) {
    if(type_to_read == SeaderCredentialTypeNone) {
        return true;
    }

    if(!detected_types || detected_type_count != 1U) {
        return true;
    }

    return detected_types[0] != type_to_read;
}
