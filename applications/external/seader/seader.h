#pragma once

#include <stdbool.h>
#include <stddef.h>

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
    SeaderHfTeardownActionRestartRead,
    SeaderHfTeardownActionStopApp,
} SeaderHfTeardownAction;

bool seader_worker_acquire(Seader* seader);
void seader_worker_release(Seader* seader);
void seader_scratch_reset(Seader* seader);
void* seader_scratch_alloc(Seader* seader, size_t size, size_t align);
bool seader_wiegand_plugin_acquire(Seader* seader);
void seader_wiegand_plugin_release(Seader* seader);
bool seader_hf_plugin_acquire(Seader* seader);
void seader_hf_plugin_release(Seader* seader);
bool seader_hf_request_teardown(Seader* seader, SeaderHfTeardownAction action);
bool seader_hf_finish_teardown_action(Seader* seader);
