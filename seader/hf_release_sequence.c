#include "hf_release_sequence.h"

static void seader_hf_release_callback_invoke(SeaderHfReleaseCallback callback, void* context) {
    if(callback) {
        callback(context);
    }
}

/* This is the one canonical HF release order used by production teardown paths and by
   the runtime-integration tests. It mirrors the ownership documentation so teardown ordering can
   be reviewed and exercised without duplicating the sequence in multiple call sites. */
void seader_hf_release_sequence_run(SeaderHfReleaseSequence* sequence) {
    if(!sequence) {
        return;
    }

    if(sequence->hf_session_state) {
        *sequence->hf_session_state = SeaderHfSessionStateTearingDown;
    }
    /* Stop live I/O before freeing any HF-owned or host-owned runtime objects. */
    seader_hf_release_callback_invoke(sequence->plugin_stop, sequence->context);
    seader_hf_release_callback_invoke(sequence->host_poller_release, sequence->context);
    seader_hf_release_callback_invoke(sequence->host_picopass_release, sequence->context);
    seader_hf_release_callback_invoke(sequence->plugin_free, sequence->context);
    seader_hf_release_callback_invoke(sequence->plugin_manager_unload, sequence->context);
    /* Reset worker-visible session state before publishing Unloaded/None. */
    seader_hf_release_callback_invoke(sequence->worker_reset, sequence->context);
    if(sequence->hf_session_state) {
        *sequence->hf_session_state = SeaderHfSessionStateUnloaded;
    }
    if(sequence->mode_runtime && *sequence->mode_runtime == SeaderModeRuntimeHF) {
        *sequence->mode_runtime = SeaderModeRuntimeNone;
    }
}
