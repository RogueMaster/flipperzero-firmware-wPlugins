#include "seader_hf_read_plan.h"

#define SEADER_HF_READ_PLAN_MAX_TYPES 3U

static void seader_hf_read_plan_add_type(SeaderHfReadPlan* plan, SeaderCredentialType type) {
    if(type == SeaderCredentialTypeNone) {
        return;
    }

    for(size_t i = 0; i < plan->detected_type_count; i++) {
        if(plan->detected_types[i] == type) {
            return;
        }
    }

    if(plan->detected_type_count < SEADER_HF_READ_PLAN_MAX_TYPES) {
        plan->detected_types[plan->detected_type_count++] = type;
    }
}

SeaderHfReadPlan seader_hf_read_plan_build(
    SeaderCredentialType selected_type,
    const SeaderCredentialType* detected_types,
    size_t detected_type_count) {
    SeaderHfReadPlan plan = {0};

    if(selected_type != SeaderCredentialTypeNone) {
        plan.decision = SeaderHfReadDecisionStartRead;
        plan.type_to_read = selected_type;
        return plan;
    }

    for(size_t i = 0; i < detected_type_count; i++) {
        seader_hf_read_plan_add_type(&plan, detected_types[i]);
    }

    if(plan.detected_type_count == 1U) {
        plan.decision = SeaderHfReadDecisionStartRead;
        plan.type_to_read = plan.detected_types[0];
    } else if(plan.detected_type_count > 1U) {
        plan.decision = SeaderHfReadDecisionSelectType;
    } else {
        plan.decision = SeaderHfReadDecisionContinuePolling;
    }

    return plan;
}
