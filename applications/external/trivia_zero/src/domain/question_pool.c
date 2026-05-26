#include "include/domain/question_pool.h"

bool question_pool_next(uint32_t count, AntiRepeat *seen, RngFn rng, void *rng_ctx,
                        uint32_t *id_out, bool *reset_happened) {
    if (count == 0u || !seen || !rng || !id_out || !reset_happened) {
        return false;
    }

    *reset_happened = false;

    if (anti_repeat_count(seen) >= count) {
        anti_repeat_reset(seen);
        *reset_happened = true;
    }

    /* Linear probe from a random start. Worst case scans the whole pool, which
     * is fine — the pool is small (~2000) and we only do this on user input. */
    const uint32_t start = rng(rng_ctx) % count;
    for (uint32_t i = 0u; i < count; ++i) {
        const uint32_t cand = (start + i) % count;
        if (!anti_repeat_is_marked(seen, cand)) {
            *id_out = cand;
            return true;
        }
    }

    /* Should not happen: we just reset above if everything was seen. */
    return false;
}
