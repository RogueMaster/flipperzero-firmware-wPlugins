#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "seader.h"
#include "uhf_snmp_probe.h"

void seader_runtime_reset_cached_sam_metadata(
    uint8_t sam_version[2],
    char* uhf_status_label,
    size_t label_size,
    SeaderUhfSnmpProbe* probe);

bool seader_runtime_begin_uhf_probe(
    bool sam_present,
    SeaderModeRuntime* mode_runtime,
    SeaderHfSessionState hf_session_state,
    SeaderUhfSnmpProbe* probe);

void seader_runtime_finish_uhf_probe(SeaderModeRuntime* mode_runtime);

void seader_runtime_begin_hf_teardown(SeaderHfSessionState* hf_session_state);

void seader_runtime_finalize_hf_release(
    SeaderHfSessionState* hf_session_state,
    SeaderModeRuntime* mode_runtime);
