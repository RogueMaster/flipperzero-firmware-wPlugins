#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hf_read_lifecycle.h"
#include "sam_startup_ui.h"

typedef struct Seader Seader;
typedef struct SeaderPollerContainer SeaderPollerContainer;

typedef enum {
    SeaderHfSessionStateUnloaded,
    SeaderHfSessionStateLoaded,
    SeaderHfSessionStateActive,
    SeaderHfSessionStateTearingDown,
} SeaderHfSessionState;

typedef enum {
    SeaderModeRuntimeNone,
    SeaderModeRuntimeHF,
    SeaderModeRuntimeUHF,
} SeaderModeRuntime;

typedef enum {
    SeaderHfTeardownActionNone,
    SeaderHfTeardownActionSamPresent,
    SeaderHfTeardownActionBoardMissing,
    SeaderHfTeardownActionAutoRecover,
    SeaderHfTeardownActionPrepareSave,
    SeaderHfTeardownActionRestartRead,
    SeaderHfTeardownActionStopApp,
} SeaderHfTeardownAction;

bool seader_worker_acquire(Seader* seader);
void seader_worker_release(Seader* seader);
bool seader_temp_strings_ensure(Seader* seader, size_t count);
void seader_temp_strings_release(Seader* seader, size_t count);
bool seader_board_retry_power_cycle(Seader* seader);
void seader_start_popup_set_stage(Seader* seader, SeaderStartupStage stage);
bool seader_wiegand_plugin_acquire(Seader* seader);
void seader_wiegand_plugin_release(Seader* seader);
bool seader_hf_plugin_acquire(Seader* seader);
void seader_hf_plugin_release(Seader* seader);
bool seader_hf_request_teardown(Seader* seader, SeaderHfTeardownAction action);
bool seader_hf_finish_teardown_action(Seader* seader);
