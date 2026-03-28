#include "runtime_policy.h"

#include <string.h>

/* A newly accepted SAM must not inherit visible metadata from the previous card while
   asynchronous version/serial/UHF maintenance responses are still in flight. */
void seader_runtime_reset_cached_sam_metadata(
    uint8_t sam_version[2],
    char* uhf_status_label,
    size_t label_size,
    SeaderUhfSnmpProbe* probe) {
    if(sam_version) {
        sam_version[0] = 0U;
        sam_version[1] = 0U;
    }

    if(uhf_status_label && label_size > 0U) {
        uhf_status_label[0] = '\0';
    }

    if(probe) {
        seader_uhf_snmp_probe_init(probe);
    }
}

/* UHF maintenance is a mutually exclusive runtime mode. The probe may only start when
   the SAM is present, HF is fully unloaded, and no other mode currently owns runtime. */
bool seader_runtime_begin_uhf_probe(
    bool sam_present,
    SeaderModeRuntime* mode_runtime,
    SeaderHfSessionState hf_session_state,
    SeaderUhfSnmpProbe* probe) {
    if(!sam_present || !mode_runtime || !probe) {
        return false;
    }

    if(hf_session_state != SeaderHfSessionStateUnloaded) {
        return false;
    }

    if(*mode_runtime != SeaderModeRuntimeNone) {
        return false;
    }

    *mode_runtime = SeaderModeRuntimeUHF;
    seader_uhf_snmp_probe_init(probe);
    return true;
}

/* Clear the narrow UHF probe runtime only when it currently owns mode_runtime. */
void seader_runtime_finish_uhf_probe(SeaderModeRuntime* mode_runtime) {
    if(!mode_runtime) {
        return;
    }

    if(*mode_runtime == SeaderModeRuntimeUHF) {
        *mode_runtime = SeaderModeRuntimeNone;
    }
}

/* Teardown publishes TearingDown before any runtime release so acquire paths and teardown
   request paths can see that HF work is already shutting down. */
void seader_runtime_begin_hf_teardown(SeaderHfSessionState* hf_session_state) {
    if(hf_session_state) {
        *hf_session_state = SeaderHfSessionStateTearingDown;
    }
}

/* Final state publication happens only after the caller has completed the release sequence. */
void seader_runtime_finalize_hf_release(
    SeaderHfSessionState* hf_session_state,
    SeaderModeRuntime* mode_runtime) {
    if(hf_session_state) {
        *hf_session_state = SeaderHfSessionStateUnloaded;
    }

    if(mode_runtime && *mode_runtime == SeaderModeRuntimeHF) {
        *mode_runtime = SeaderModeRuntimeNone;
    }
}

void seader_runtime_fail_hf_startup(
    SeaderHfReadState* hf_read_state,
    SeaderHfReadFailureReason* failure_reason,
    uint32_t* last_progress_tick,
    SeaderHfSessionState* hf_session_state,
    SeaderModeRuntime* mode_runtime) {
    if(hf_read_state) {
        *hf_read_state = SeaderHfReadStateTerminalFail;
    }

    if(failure_reason) {
        *failure_reason = SeaderHfReadFailureReasonUnavailable;
    }

    if(last_progress_tick) {
        *last_progress_tick = 0U;
    }

    if(hf_session_state) {
        *hf_session_state = SeaderHfSessionStateUnloaded;
    }

    if(mode_runtime && *mode_runtime == SeaderModeRuntimeHF) {
        *mode_runtime = SeaderModeRuntimeNone;
    }
}

bool seader_runtime_begin_board_auto_recover(
    bool sam_present,
    bool hf_runtime_active,
    SeaderCredentialType selected_read_type,
    bool* pending,
    bool* resume_read,
    SeaderCredentialType* preserved_read_type) {
    if(!sam_present || !pending || !resume_read || !preserved_read_type || *pending) {
        return false;
    }

    *pending = true;
    *resume_read = hf_runtime_active;
    *preserved_read_type = hf_runtime_active ? selected_read_type : SeaderCredentialTypeNone;
    return true;
}

void seader_runtime_finish_board_auto_recover(
    bool* pending,
    bool* resume_read,
    SeaderCredentialType* preserved_read_type) {
    if(pending) {
        *pending = false;
    }

    if(resume_read) {
        *resume_read = false;
    }

    if(preserved_read_type) {
        *preserved_read_type = SeaderCredentialTypeNone;
    }
}

void seader_runtime_reset_hf_mode(
    bool* hf_mode_active,
    SeaderCredentialType* selected_read_type,
    SeaderCredentialType detected_types[],
    size_t detected_capacity,
    size_t* detected_type_count) {
    if(selected_read_type) {
        *selected_read_type = SeaderCredentialTypeNone;
    }

    if(detected_types && detected_capacity > 0U) {
        memset(detected_types, 0, detected_capacity * sizeof(detected_types[0]));
    }

    if(detected_type_count) {
        *detected_type_count = 0U;
    }

    if(hf_mode_active) {
        *hf_mode_active = false;
    }
}
