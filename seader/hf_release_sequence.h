#pragma once

#include "seader.h"

typedef void (*SeaderHfReleaseCallback)(void* context);

typedef struct {
    void* context;
    SeaderHfSessionState* hf_session_state;
    SeaderModeRuntime* mode_runtime;
    SeaderHfReleaseCallback plugin_stop;
    SeaderHfReleaseCallback host_poller_release;
    SeaderHfReleaseCallback host_picopass_release;
    SeaderHfReleaseCallback plugin_free;
    SeaderHfReleaseCallback plugin_manager_unload;
    SeaderHfReleaseCallback worker_reset;
} SeaderHfReleaseSequence;

void seader_hf_release_sequence_run(SeaderHfReleaseSequence* sequence);
