#include "worker_loop_policy.h"

bool seader_worker_virtual_credential_should_continue(
    bool processing_ok,
    bool worker_active,
    bool stage_complete,
    bool stage_fail,
    uint8_t empty_loops_remaining) {
    return processing_ok && worker_active && !stage_complete && !stage_fail &&
           empty_loops_remaining > 0U;
}
