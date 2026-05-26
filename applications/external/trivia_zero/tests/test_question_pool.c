#include "include/domain/anti_repeat.h"
#include "include/domain/question_pool.h"
#include <assert.h>
#include <stddef.h>

static uint32_t g_seq[8];
static uint32_t g_seq_idx;

static uint32_t stub_rng(void *ctx) {
    (void)ctx;
    const uint32_t v = g_seq[g_seq_idx];
    g_seq_idx = (g_seq_idx + 1u) % 8u;
    return v;
}

int main(void) {
    AntiRepeat seen;
    anti_repeat_init(&seen);

    /* count=4. RNG returns values that mod-4 to 0,1,2,3 in order. */
    g_seq[0] = 0u;
    g_seq[1] = 1u;
    g_seq[2] = 2u;
    g_seq[3] = 3u;
    g_seq[4] = 0u;
    g_seq[5] = 1u;
    g_seq_idx = 0u;

    uint32_t id;
    bool reset_happened;
    bool ok;

    ok = question_pool_next(4u, &seen, stub_rng, NULL, &id, &reset_happened);
    assert(ok == true);
    assert(id == 0u && reset_happened == false);
    anti_repeat_mark(&seen, id);

    ok = question_pool_next(4u, &seen, stub_rng, NULL, &id, &reset_happened);
    assert(ok && id == 1u && !reset_happened);
    anti_repeat_mark(&seen, id);

    ok = question_pool_next(4u, &seen, stub_rng, NULL, &id, &reset_happened);
    assert(ok && id == 2u && !reset_happened);
    anti_repeat_mark(&seen, id);

    ok = question_pool_next(4u, &seen, stub_rng, NULL, &id, &reset_happened);
    assert(ok && id == 3u && !reset_happened);
    anti_repeat_mark(&seen, id);

    /* Pool exhausted → next call resets the seen set and picks afresh. */
    ok = question_pool_next(4u, &seen, stub_rng, NULL, &id, &reset_happened);
    assert(ok == true);
    assert(reset_happened == true);
    assert(id < 4u);

    AntiRepeat empty;
    anti_repeat_init(&empty);
    ok = question_pool_next(0u, &empty, stub_rng, NULL, &id, &reset_happened);
    assert(ok == false);

    return 0;
}
