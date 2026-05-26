#include "include/domain/anti_repeat.h"
#include <string.h>

void anti_repeat_init(AntiRepeat *ar) {
    if (!ar) {
        return;
    }
    memset(ar, 0, sizeof(*ar));
}

void anti_repeat_reset(AntiRepeat *ar) {
    anti_repeat_init(ar);
}

void anti_repeat_mark(AntiRepeat *ar, uint32_t id) {
    if (!ar || id >= ANTI_REPEAT_MAX) {
        return;
    }
    const uint32_t word = id >> 5u;
    const uint32_t mask = 1u << (id & 31u);
    if ((ar->bits[word] & mask) == 0u) {
        ar->bits[word] |= mask;
        ar->count++;
    }
}

bool anti_repeat_is_marked(const AntiRepeat *ar, uint32_t id) {
    if (!ar || id >= ANTI_REPEAT_MAX) {
        return false;
    }
    const uint32_t word = id >> 5u;
    const uint32_t mask = 1u << (id & 31u);
    return (ar->bits[word] & mask) != 0u;
}

uint32_t anti_repeat_count(const AntiRepeat *ar) {
    return ar ? ar->count : 0u;
}
