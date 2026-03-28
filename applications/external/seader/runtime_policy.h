#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "seader.h"
#include "seader_credential_type.h"
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
void seader_runtime_fail_hf_startup(
    SeaderHfReadState* hf_read_state,
    SeaderHfReadFailureReason* failure_reason,
    uint32_t* last_progress_tick,
    SeaderHfSessionState* hf_session_state,
    SeaderModeRuntime* mode_runtime);
bool seader_runtime_begin_board_auto_recover(
    bool sam_present,
    bool hf_runtime_active,
    SeaderCredentialType selected_read_type,
    bool* pending,
    bool* resume_read,
    SeaderCredentialType* preserved_read_type);
void seader_runtime_finish_board_auto_recover(
    bool* pending,
    bool* resume_read,
    SeaderCredentialType* preserved_read_type);
void seader_runtime_reset_hf_mode(
    bool* hf_mode_active,
    SeaderCredentialType* selected_read_type,
    SeaderCredentialType detected_types[],
    size_t detected_capacity,
    size_t* detected_type_count);
