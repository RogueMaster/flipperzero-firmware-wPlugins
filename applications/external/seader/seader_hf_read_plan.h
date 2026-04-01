#pragma once

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
