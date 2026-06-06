#include "include/domain/anti_repeat.h"
#include <assert.h>

int main(void) {
    AntiRepeat ar;
    anti_repeat_init(&ar);

    assert(anti_repeat_count(&ar) == 0u);
    assert(anti_repeat_is_marked(&ar, 0u) == false);
    assert(anti_repeat_is_marked(&ar, 100u) == false);
    assert(anti_repeat_is_marked(&ar, ANTI_REPEAT_MAX - 1u) == false);

    anti_repeat_mark(&ar, 0u);
    anti_repeat_mark(&ar, 100u);
    anti_repeat_mark(&ar, 100u); /* idempotent */
    anti_repeat_mark(&ar, ANTI_REPEAT_MAX - 1u);

    assert(anti_repeat_is_marked(&ar, 0u) == true);
    assert(anti_repeat_is_marked(&ar, 100u) == true);
    assert(anti_repeat_is_marked(&ar, 99u) == false);
    assert(anti_repeat_is_marked(&ar, ANTI_REPEAT_MAX - 1u) == true);
    assert(anti_repeat_count(&ar) == 3u);

    anti_repeat_mark(&ar, ANTI_REPEAT_MAX);
    anti_repeat_mark(&ar, ANTI_REPEAT_MAX + 50u);
    assert(anti_repeat_is_marked(&ar, ANTI_REPEAT_MAX) == false);
    assert(anti_repeat_count(&ar) == 3u);

    anti_repeat_reset(&ar);
    assert(anti_repeat_count(&ar) == 0u);
    assert(anti_repeat_is_marked(&ar, 0u) == false);
    assert(anti_repeat_is_marked(&ar, 100u) == false);

    return 0;
}
