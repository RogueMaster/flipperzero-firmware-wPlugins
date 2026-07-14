#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SeaderBoardStatusUnknown = 0,
    SeaderBoardStatusPowerReadyPendingValidation,
    SeaderBoardStatusReady,
    SeaderBoardStatusFaultPreEnable,
    SeaderBoardStatusFaultPostEnable,
    SeaderBoardStatusNoResponse,
    SeaderBoardStatusPowerLost,
    SeaderBoardStatusRetryRequested,
} SeaderBoardStatus;

typedef enum {
    SeaderBoardRuntimePowerStateHealthy = 0,
    SeaderBoardRuntimePowerStateGracePending,
    SeaderBoardRuntimePowerStateLost,
} SeaderBoardRuntimePowerState;

typedef enum {
    SeaderBoardRuntimeEventActionNone = 0,
    SeaderBoardRuntimeEventActionWait,
    SeaderBoardRuntimeEventActionAutoRecover,
    SeaderBoardRuntimeEventActionBoardPowerLost,
} SeaderBoardRuntimeEventAction;

typedef struct {
    bool should_enable_otg;
    bool owns_otg;
} SeaderBoardPowerAcquirePlan;

SeaderBoardPowerAcquirePlan seader_board_power_plan_acquire(bool otg_already_enabled);
bool seader_board_power_is_available(bool otg_enabled, uint16_t vbus_mv);
SeaderBoardRuntimePowerState seader_board_runtime_power_state(
    bool otg_requested,
    bool otg_enabled,
    uint16_t vbus_mv,
    bool otg_fault,
    bool grace_active,
    uint32_t grace_elapsed_ms,
    uint32_t grace_window_ms);
SeaderBoardRuntimeEventAction seader_board_runtime_event_action(
    SeaderBoardRuntimePowerState runtime_state,
    bool sam_present,
    bool auto_recover_pending);
bool seader_board_should_disable_owned_otg(bool power_owned, bool otg_enabled);
bool seader_board_status_requires_power_cycle(SeaderBoardStatus status);
SeaderBoardStatus seader_board_status_on_sam_missing(SeaderBoardStatus status);
const char* seader_board_status_label(SeaderBoardStatus status);
