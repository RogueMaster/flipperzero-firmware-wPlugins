#include "include/domain/history_buffer.h"
#include <assert.h>

int main(void) {
    HistoryBuffer h;
    history_buffer_init(&h);

    uint32_t out;
    assert(history_buffer_len(&h) == 0u);
    assert(history_buffer_peek_back(&h, 0u, &out) == false);

    history_buffer_push(&h, 10u);
    assert(history_buffer_len(&h) == 1u);
    assert(history_buffer_peek_back(&h, 0u, &out) == true && out == 10u);
    assert(history_buffer_peek_back(&h, 1u, &out) == false);

    history_buffer_push(&h, 20u);
    history_buffer_push(&h, 30u);
    assert(history_buffer_len(&h) == 3u);
    assert(history_buffer_peek_back(&h, 0u, &out) == true && out == 30u);
    assert(history_buffer_peek_back(&h, 1u, &out) == true && out == 20u);
    assert(history_buffer_peek_back(&h, 2u, &out) == true && out == 10u);
    assert(history_buffer_peek_back(&h, 3u, &out) == false);

    history_buffer_push(&h, 40u);
    history_buffer_push(&h, 50u);
    history_buffer_push(&h, 60u); /* drops 10 */
    history_buffer_push(&h, 70u); /* drops 20 */
    assert(history_buffer_len(&h) == HISTORY_CAPACITY);
    assert(history_buffer_peek_back(&h, 0u, &out) == true && out == 70u);
    assert(history_buffer_peek_back(&h, 4u, &out) == true && out == 30u);
    assert(history_buffer_peek_back(&h, 5u, &out) == false);

    history_buffer_clear(&h);
    assert(history_buffer_len(&h) == 0u);
    assert(history_buffer_peek_back(&h, 0u, &out) == false);

    return 0;
}
