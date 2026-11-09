#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLIPBOARD_PROGRAM_MAX   65536U
#define CLIPBOARD_OPERATION_MAX 128U
#define CLIPBOARD_NODE_MAX      512U
#define CLIPBOARD_MOUSE_MAX_MS  86400000U

typedef struct {
    void* context;
    size_t size;
    bool (*read)(void* context, size_t offset, uint8_t* output, size_t size);
} ClipboardReader;

typedef struct {
    void* context;
    bool (*press)(void* context, uint16_t key);
    bool (*release)(void* context);
    bool (*mouse_move)(void* context, int8_t dx, int8_t dy);
    bool (*random)(void* context, uint32_t* value);
} ClipboardKeyboard;

typedef enum {
    ClipboardProgramOk,
    ClipboardProgramInvalid,
    ClipboardProgramText,
    ClipboardProgramKey,
    ClipboardProgramStorage,
} ClipboardProgramResult;

typedef enum {
    ClipboardEngineIdle,
    ClipboardEngineRunning,
    ClipboardEngineDone,
    ClipboardEngineCancelled,
    ClipboardEngineFailed,
} ClipboardEngineState;

typedef struct {
    ClipboardReader reader;
    ClipboardKeyboard keyboard;
    const uint16_t* layout;
    ClipboardEngineState state;
    size_t cursor;
    size_t text_end;
    uint32_t due;
    uint16_t remaining;
    uint8_t phase;
    bool turkish;
    bool mac_turkish;
    bool finish_grave;
    bool literal_key;
    uint16_t typing_pause;
    uint16_t typing_variation;
    bool mouse_active;
    bool mouse_return;
    uint32_t mouse_end;
    uint32_t mouse_interval;
    bool repeat_active;
    uint32_t repeat_end;
    uint32_t repeat_min;
    uint32_t repeat_max;
    uint32_t repeat_cycle;
    size_t repeat_start;
    size_t repeat_after;
    uint16_t repeat_count;
    uint16_t outer_remaining;
} ClipboardEngine;

uint16_t clipboard_read_u16(const uint8_t* data);
uint32_t clipboard_read_u32(const uint8_t* data);
void clipboard_write_u16(uint8_t* data, uint16_t value);
void clipboard_write_u32(uint8_t* data, uint32_t value);
ClipboardProgramResult clipboard_program_validate(
    const ClipboardReader* reader,
    const uint16_t layout[128],
    bool turkish);
void clipboard_engine_start(
    ClipboardEngine* engine,
    ClipboardReader reader,
    ClipboardKeyboard keyboard,
    const uint16_t layout[128],
    bool turkish,
    bool mac_turkish,
    uint32_t now);
void clipboard_engine_tick(ClipboardEngine* engine, uint32_t now, bool usb_connected);
void clipboard_engine_cancel(ClipboardEngine* engine);
uint32_t clipboard_engine_mouse_remaining(const ClipboardEngine* engine, uint32_t now);
