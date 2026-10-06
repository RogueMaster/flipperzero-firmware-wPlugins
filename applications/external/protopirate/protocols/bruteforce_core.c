#include "bruteforce_core.h"
#include "protocols_common.h"

bool bruteforce_state_from_flipper_format(BruteForceState* state, FlipperFormat* ff) {
    bool ok = false;
    do {
        uint64_t key1 = 0;
        if(!pp_flipper_read_hex_u64(ff, FF_KEY, &key1)) break;
        state->key1_low = (uint32_t)(key1 & 0xFFFFFFFF);
        state->key1_high = (uint32_t)((key1 >> 32) & 0xFFFFFFFF);

        uint64_t key2 = 0;
        if(!pp_flipper_read_hex_u64(ff, "Key_2", &key2)) break;
        state->key2_low = (uint16_t)(key2 & 0xFFFF);

        state->cancel = 0;
        state->progress_current = 0;
        state->progress_total = 0;
        state->status = BRUTEFORCE_STATUS_IDLE;
        state->on_done = NULL;
        state->on_done_ctx = NULL;
        ok = true;
    } while(false);
    return ok;
}
