/*
 * File: konami.h
 * Author: vh8t
 * Created: 28/12/2025
 */

#ifndef KONAMI_H
#define KONAMI_H

#include <furi_hal.h>
#include <input/input.h>

#include <stdbool.h>
#include <stdint.h>

#define KONAMI_TIMEOUT_MS 2000

typedef void (*konami_callback_t)(void* ctx);

typedef struct {
    const InputKey* sequence;
    konami_callback_t callback;
    uint32_t last_tick;
    uint8_t length;
    uint8_t curr_step;
} konami_t;

#define UP    InputKeyUp
#define DOWN  InputKeyDown
#define LEFT  InputKeyLeft
#define RIGHT InputKeyRight
#define BACK  InputKeyBack
#define OK    InputKeyOk

#define KONAMI_CALLBACK(name) void konami_callback_##name(void* ctx)
#define KONAMI_LEN(k)         sizeof(k) / sizeof(k[0])
#define KONAMI(name, ...)                             \
    static const InputKey SEQ_##name[] = __VA_ARGS__; \
    static konami_t konami_##name = {                 \
        .sequence = SEQ_##name,                       \
        .callback = konami_callback_##name,           \
        .last_tick = 0,                               \
        .length = KONAMI_LEN(SEQ_##name),             \
        .curr_step = 0,                               \
    }

#define CHECK_KONAMI(name, ev, matched, sm) check_konami(&konami_##name, ev, matched, sm)

bool check_konami(konami_t* konami, InputEvent* ev, bool* matched, void* ctx);

KONAMI_CALLBACK(money);
KONAMI_CALLBACK(sprites);
KONAMI_CALLBACK(rigged);
KONAMI_CALLBACK(free_play);
KONAMI_CALLBACK(nuke);
KONAMI_CALLBACK(rtp);
KONAMI_CALLBACK(high_roller);

#endif // KONAMI_H
