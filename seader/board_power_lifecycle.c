#include "board_power_lifecycle.h"

#define SEADER_BOARD_POWER_AVAILABLE_MV 4500U

SeaderBoardPowerAcquirePlan seader_board_power_plan_acquire(bool otg_already_enabled) {
    SeaderBoardPowerAcquirePlan plan = {
        .should_enable_otg = !otg_already_enabled,
        .owns_otg = !otg_already_enabled,
    };

    return plan;
}

bool seader_board_power_is_available(bool otg_enabled, uint16_t vbus_mv) {
    return otg_enabled || vbus_mv >= SEADER_BOARD_POWER_AVAILABLE_MV;
}

SeaderBoardRuntimePowerState seader_board_runtime_power_state(
    bool otg_requested,
    bool otg_enabled,
    uint16_t vbus_mv,
    bool otg_fault,
    bool grace_active,
    uint32_t grace_elapsed_ms,
    uint32_t grace_window_ms) {
    if(seader_board_power_is_available(otg_enabled, vbus_mv)) {
        return SeaderBoardRuntimePowerStateHealthy;
    }

    if(otg_fault && vbus_mv < SEADER_BOARD_POWER_AVAILABLE_MV) {
        return SeaderBoardRuntimePowerStateLost;
    }

    if(otg_requested && (!grace_active || grace_elapsed_ms < grace_window_ms)) {
        return SeaderBoardRuntimePowerStateGracePending;
    }

    return SeaderBoardRuntimePowerStateLost;
}

SeaderBoardRuntimeEventAction seader_board_runtime_event_action(
    SeaderBoardRuntimePowerState runtime_state,
    bool sam_present,
    bool auto_recover_pending) {
    switch(runtime_state) {
    case SeaderBoardRuntimePowerStateHealthy:
        return SeaderBoardRuntimeEventActionNone;
    case SeaderBoardRuntimePowerStateGracePending:
        return SeaderBoardRuntimeEventActionWait;
    case SeaderBoardRuntimePowerStateLost:
        if(auto_recover_pending) {
            return SeaderBoardRuntimeEventActionNone;
        }
        return sam_present ? SeaderBoardRuntimeEventActionAutoRecover :
                             SeaderBoardRuntimeEventActionBoardPowerLost;
    default:
        return SeaderBoardRuntimeEventActionNone;
    }
}

bool seader_board_should_disable_owned_otg(bool power_owned, bool otg_enabled) {
    return power_owned && otg_enabled;
}

bool seader_board_status_requires_power_cycle(SeaderBoardStatus status) {
    switch(status) {
    case SeaderBoardStatusFaultPreEnable:
    case SeaderBoardStatusFaultPostEnable:
    case SeaderBoardStatusNoResponse:
    case SeaderBoardStatusPowerLost:
    case SeaderBoardStatusRetryRequested:
        return true;
    case SeaderBoardStatusUnknown:
    case SeaderBoardStatusPowerReadyPendingValidation:
    case SeaderBoardStatusReady:
    default:
        return false;
    }
}

SeaderBoardStatus seader_board_status_on_sam_missing(SeaderBoardStatus status) {
    if(status == SeaderBoardStatusPowerReadyPendingValidation) {
        return SeaderBoardStatusNoResponse;
    }

    return status;
}

const char* seader_board_status_label(SeaderBoardStatus status) {
    switch(status) {
    case SeaderBoardStatusFaultPreEnable:
    case SeaderBoardStatusFaultPostEnable:
        return "Board Fault";
    case SeaderBoardStatusNoResponse:
        return "Board No Response";
    case SeaderBoardStatusPowerLost:
        return "Power Lost";
    case SeaderBoardStatusRetryRequested:
        return "Retry Board";
    case SeaderBoardStatusPowerReadyPendingValidation:
        return "Checking Board";
    case SeaderBoardStatusReady:
        return "Board Ready";
    case SeaderBoardStatusUnknown:
    default:
        return "NO SAM";
    }
}
