#include "runtime_policy.h"

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
