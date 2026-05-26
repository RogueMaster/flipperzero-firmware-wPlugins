#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ANTI_REPEAT_MAX 4096u
#define ANTI_REPEAT_WORDS (ANTI_REPEAT_MAX / 32u)

typedef struct {
    uint32_t bits[ANTI_REPEAT_WORDS];
    uint32_t count;
} AntiRepeat;

void anti_repeat_init(AntiRepeat *ar);
void anti_repeat_reset(AntiRepeat *ar);
void anti_repeat_mark(AntiRepeat *ar, uint32_t id);
bool anti_repeat_is_marked(const AntiRepeat *ar, uint32_t id);
uint32_t anti_repeat_count(const AntiRepeat *ar);
