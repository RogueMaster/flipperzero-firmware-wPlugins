#include "macro_engine.h"
#include "keyboard_text.h"
#include <string.h>

uint16_t clipboard_read_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
uint32_t clipboard_read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
void clipboard_write_u16(uint8_t* p, uint16_t value) {
    p[0] = value;
    p[1] = value >> 8;
}
void clipboard_write_u32(uint8_t* p, uint32_t value) {
    for(unsigned i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

static bool read_at(const ClipboardReader* reader, size_t offset, uint8_t* output, size_t size) {
    return offset <= reader->size && size <= reader->size - offset &&
           reader->read(reader->context, offset, output, size);
}

static bool
    read_scalar(const ClipboardReader* reader, size_t* cursor, size_t end, uint32_t* scalar) {
    uint8_t bytes[2];
    if(*cursor >= end || !read_at(reader, *cursor, bytes, 1)) return false;
    size_t length = bytes[0] < 128 ? 1 : 2;
    if(length > end - *cursor || !read_at(reader, *cursor, bytes, length)) return false;
    size_t position = 0;
    if(!clipboard_text_next(bytes, length, &position, scalar)) return false;
    *cursor += length;
    return true;
}

static uint16_t text_key(const uint16_t layout[128], bool turkish, uint32_t scalar) {
    if(scalar == '\n' || scalar == '\r') return 0x28;
    if(scalar == '\t') return 0x2B;
    if(scalar >= 128) return turkish ? clipboard_turkish_hid(scalar) : 0;
    uint16_t key = layout[scalar];
    // Control/GUI are only legal in explicit key records, never literal text.
    return (key & 0x9900) || (key & 0xFF) < 4 || (key & 0xFF) >= 0xE0 ? 0 : key;
}

static uint16_t chord_key(const uint16_t layout[128], uint8_t modifiers, uint16_t symbol) {
    uint16_t key = 0;
    if((symbol >= 'a' && symbol <= 'z') || (symbol >= '0' && symbol <= '9'))
        key = layout[symbol];
    else {
        static const uint8_t special[] = {
            0x28,
            0x2B,
            0x29,
            0x2A,
            0x4C,
            0x52,
            0x51,
            0x50,
            0x4F,
            0x4A,
            0x4D,
            0x4B,
            0x4E,
            0x2C,
        };
        if(symbol >= 0x100 && symbol < 0x100 + sizeof(special))
            key = special[symbol - 0x100];
        else if(symbol >= 0x110 && symbol <= 0x11B)
            key = (uint16_t)(0x3A + symbol - 0x110);
    }
    if((key & 0xFF) < 4 || (key & 0xFF) >= 0xE0 || (modifiers & 0xF0)) return 0;
    return key | ((uint16_t)modifiers << 8);
}

typedef struct {
    uint32_t delay, timed;
    unsigned nodes;
} ProgramBudget;

static bool valid_move(const uint8_t record[4]) {
    int x_min = (int8_t)record[0], x_max = (int8_t)record[1];
    int y_min = (int8_t)record[2], y_max = (int8_t)record[3];
    return x_min >= -127 && y_min >= -127 && x_min <= x_max && y_min <= y_max &&
           (x_min || x_max || y_min || y_max);
}

static bool valid_timing(uint32_t duration, uint32_t low, uint32_t high) {
    return duration && duration <= CLIPBOARD_MOUSE_MAX_MS && low >= 1000 && low <= high &&
           high <= 3600000;
}

static bool valid_typing(uint16_t pause, uint16_t variation) {
    return pause <= 1000 && variation <= pause && (uint32_t)pause + variation <= 1000;
}

static ClipboardProgramResult validate_records(
    const ClipboardReader* reader,
    size_t* position,
    unsigned count,
    const uint16_t layout[128],
    bool turkish,
    bool nested,
    ProgramBudget* budget) {
    if(!count || count > CLIPBOARD_OPERATION_MAX) return ClipboardProgramInvalid;
    size_t cursor = *position;
    bool output = false;
    for(unsigned operation = 0; operation < count; operation++) {
        if(++budget->nodes > CLIPBOARD_NODE_MAX) return ClipboardProgramInvalid;
        uint8_t record[5];
        if(!read_at(reader, cursor++, record, 1)) return ClipboardProgramStorage;
        if(record[0] == 1) {
            if(!read_at(reader, cursor, record, 4)) return ClipboardProgramInvalid;
            cursor += 4;
            uint32_t size = clipboard_read_u32(record);
            if(!size || size > reader->size - cursor) return ClipboardProgramInvalid;
            size_t end = cursor + size;
            while(cursor < end) {
                uint32_t scalar;
                if(!read_scalar(reader, &cursor, end, &scalar)) return ClipboardProgramText;
                if(!text_key(layout, turkish, scalar)) return ClipboardProgramKey;
            }
            output = true;
        } else if(record[0] == 2) {
            if(!read_at(reader, cursor, record, 3)) return ClipboardProgramInvalid;
            cursor += 3;
            if(!chord_key(layout, record[0], clipboard_read_u16(record + 1)))
                return ClipboardProgramKey;
            output = true;
        } else if(record[0] == 3) {
            if(!read_at(reader, cursor, record, 4)) return ClipboardProgramInvalid;
            cursor += 4;
            uint32_t ms = clipboard_read_u32(record);
            if(!ms || ms > 30000 || budget->delay > 120000 - ms) return ClipboardProgramInvalid;
            budget->delay += ms;
        } else if(record[0] == 4) {
            uint8_t timing[8];
            if(!read_at(reader, cursor, timing, sizeof(timing))) return ClipboardProgramInvalid;
            cursor += sizeof(timing);
            uint32_t duration = clipboard_read_u32(timing);
            uint32_t interval = clipboard_read_u32(timing + 4);
            if(nested || !valid_timing(duration, interval, interval) ||
               budget->timed > CLIPBOARD_MOUSE_MAX_MS - duration)
                return ClipboardProgramInvalid;
            budget->timed += duration;
            output = true;
        } else if(record[0] == 5) {
            if(!read_at(reader, cursor, record, 4) || !valid_move(record))
                return ClipboardProgramInvalid;
            cursor += 4;
            output = true;
        } else if(record[0] == 6) {
            uint8_t repeat[18];
            if(nested || !read_at(reader, cursor, repeat, sizeof(repeat)))
                return ClipboardProgramInvalid;
            cursor += sizeof(repeat);
            uint32_t duration = clipboard_read_u32(repeat), low = clipboard_read_u32(repeat + 4);
            uint32_t high = clipboard_read_u32(repeat + 8), size = clipboard_read_u32(repeat + 14);
            if(!valid_timing(duration, low, high) ||
               budget->timed > CLIPBOARD_MOUSE_MAX_MS - duration || size > reader->size - cursor)
                return ClipboardProgramInvalid;
            budget->timed += duration;
            ClipboardReader body = *reader;
            body.size = cursor + size;
            ClipboardProgramResult result = validate_records(
                &body, &cursor, clipboard_read_u16(repeat + 12), layout, turkish, true, budget);
            if(result != ClipboardProgramOk || cursor != body.size) return ClipboardProgramInvalid;
            output = true;
        } else if(record[0] == 7) {
            if(nested || operation != 0 || cursor != 7 || !read_at(reader, cursor, record, 4))
                return ClipboardProgramInvalid;
            cursor += 4;
            if(!valid_typing(clipboard_read_u16(record), clipboard_read_u16(record + 2)))
                return ClipboardProgramInvalid;
        } else
            return ClipboardProgramInvalid;
    }
    *position = cursor;
    return output ? ClipboardProgramOk : ClipboardProgramInvalid;
}

ClipboardProgramResult clipboard_program_validate(
    const ClipboardReader* reader,
    const uint16_t layout[128],
    bool turkish) {
    uint8_t header[6];
    if(!reader || !reader->read || reader->size > CLIPBOARD_PROGRAM_MAX ||
       !read_at(reader, 0, header, sizeof(header)) || memcmp(header, "BCM4", 4) != 0)
        return ClipboardProgramInvalid;
    size_t position = 6;
    ProgramBudget budget = {0};
    ClipboardProgramResult result = validate_records(
        reader, &position, clipboard_read_u16(header + 4), layout, turkish, false, &budget);
    return result == ClipboardProgramOk && position == reader->size ? ClipboardProgramOk :
                                                                      ClipboardProgramInvalid;
}

void clipboard_engine_start(
    ClipboardEngine* engine,
    ClipboardReader reader,
    ClipboardKeyboard keyboard,
    const uint16_t layout[128],
    bool turkish,
    bool mac_turkish,
    uint32_t now) {
    memset(engine, 0, sizeof(*engine));
    engine->reader = reader;
    engine->keyboard = keyboard;
    engine->layout = layout;
    engine->turkish = turkish;
    engine->mac_turkish = mac_turkish;
    uint8_t count[2];
    engine->state = read_at(&reader, 4, count, 2) ? ClipboardEngineRunning : ClipboardEngineFailed;
    engine->remaining = engine->state == ClipboardEngineRunning ? clipboard_read_u16(count) : 0;
    engine->cursor = 6;
    engine->due = now + 900;
}

void clipboard_engine_cancel(ClipboardEngine* engine) {
    if(engine->keyboard.release) engine->keyboard.release(engine->keyboard.context);
    engine->state = ClipboardEngineCancelled;
    engine->phase = 0;
    engine->mouse_active = engine->mouse_return = false;
    engine->repeat_active = false;
}

uint32_t clipboard_engine_mouse_remaining(const ClipboardEngine* engine, uint32_t now) {
    uint32_t end = engine->repeat_active ? engine->repeat_end : engine->mouse_end;
    if((!engine->mouse_active && !engine->repeat_active) ||
       engine->state != ClipboardEngineRunning || (int32_t)(now - end) >= 0)
        return 0;
    return end - now;
}

static void engine_fail(ClipboardEngine* engine) {
    engine->keyboard.release(engine->keyboard.context);
    engine->state = ClipboardEngineFailed;
}

static bool random_index(ClipboardEngine* engine, uint32_t choices, uint32_t* result) {
    if(choices == 1) {
        *result = 0;
        return true;
    }
    uint32_t value;
    if(!choices || !engine->keyboard.random ||
       !engine->keyboard.random(engine->keyboard.context, &value))
        return false;
    // Bounded multiply-high mapping: no entropy retry loop can stall physical Stop.
    *result = (uint32_t)(((uint64_t)value * choices) >> 32);
    return true;
}

static bool move_mouse(ClipboardEngine* engine, const uint8_t record[4]) {
    if(!valid_move(record) || !engine->keyboard.mouse_move) return false;
    int32_t x_min = (int8_t)record[0], x_max = (int8_t)record[1];
    int32_t y_min = (int8_t)record[2], y_max = (int8_t)record[3];
    uint32_t width = (uint32_t)(x_max - x_min + 1), height = (uint32_t)(y_max - y_min + 1);
    bool contains_zero = x_min <= 0 && x_max >= 0 && y_min <= 0 && y_max >= 0;
    uint32_t index;
    if(!random_index(engine, width * height - (contains_zero ? 1U : 0U), &index)) return false;
    if(contains_zero && index >= (uint32_t)(-x_min) * height + (uint32_t)(-y_min)) index++;
    return engine->keyboard.mouse_move(
        engine->keyboard.context,
        (int8_t)(x_min + (int32_t)(index / height)),
        (int8_t)(y_min + (int32_t)(index % height)));
}

static bool next_key(ClipboardEngine* engine, uint32_t now, uint16_t* key) {
    while(engine->state == ClipboardEngineRunning) {
        if(engine->cursor < engine->text_end) {
            uint32_t scalar;
            if(!read_scalar(&engine->reader, &engine->cursor, engine->text_end, &scalar)) {
                engine_fail(engine);
                return false;
            }
            if(scalar == '\r' && engine->cursor < engine->text_end) {
                uint8_t next;
                if(!read_at(&engine->reader, engine->cursor, &next, 1)) {
                    engine_fail(engine);
                    return false;
                }
                if(next == '\n') engine->cursor++;
            }
            *key = text_key(engine->layout, engine->turkish, scalar);
            engine->finish_grave = engine->mac_turkish && scalar == '`';
            engine->literal_key = true;
            if(!*key) {
                engine_fail(engine);
                return false;
            }
            return true;
        }
        if(!engine->remaining) {
            if(engine->repeat_active) {
                uint32_t interval;
                if(!random_index(engine, engine->repeat_max - engine->repeat_min + 1, &interval)) {
                    engine_fail(engine);
                    return false;
                }
                engine->due = engine->repeat_cycle + engine->repeat_min + interval;
                if((int32_t)(now - engine->due) >= 0) engine->due = now + 8;
                engine->repeat_cycle = engine->due;
                engine->cursor = engine->repeat_start;
                engine->text_end = 0;
                engine->remaining = engine->repeat_count;
                return false;
            }
            engine->keyboard.release(engine->keyboard.context);
            engine->state = ClipboardEngineDone;
            return false;
        }
        engine->remaining--;
        uint8_t record[5];
        if(!read_at(&engine->reader, engine->cursor++, record, 1)) {
            engine_fail(engine);
            return false;
        }
        if(record[0] == 1) {
            if(!read_at(&engine->reader, engine->cursor, record, 4)) {
                engine_fail(engine);
                return false;
            }
            engine->cursor += 4;
            engine->text_end = engine->cursor + clipboard_read_u32(record);
        } else if(record[0] == 2) {
            if(!read_at(&engine->reader, engine->cursor, record, 3)) {
                engine_fail(engine);
                return false;
            }
            engine->cursor += 3;
            *key = chord_key(engine->layout, record[0], clipboard_read_u16(record + 1));
            engine->finish_grave = false;
            engine->literal_key = false;
            if(!*key) {
                engine_fail(engine);
                return false;
            }
            return true;
        } else if(record[0] == 3) {
            if(!read_at(&engine->reader, engine->cursor, record, 4)) {
                engine_fail(engine);
                return false;
            }
            engine->cursor += 4;
            engine->due = now + clipboard_read_u32(record);
            return false;
        } else if(record[0] == 4) {
            uint8_t timing[8];
            if(engine->repeat_active ||
               !read_at(&engine->reader, engine->cursor, timing, sizeof(timing))) {
                engine_fail(engine);
                return false;
            }
            engine->cursor += sizeof(timing);
            engine->mouse_end = now + clipboard_read_u32(timing);
            engine->mouse_interval = clipboard_read_u32(timing + 4);
            engine->mouse_active = true;
            engine->mouse_return = false;
            engine->due = now;
            return false;
        } else if(record[0] == 5) {
            if(!read_at(&engine->reader, engine->cursor, record, 4) ||
               !move_mouse(engine, record)) {
                engine_fail(engine);
                return false;
            }
            engine->cursor += 4;
            engine->due = now + 8;
            return false;
        } else if(record[0] == 6) {
            uint8_t repeat[18];
            if(engine->repeat_active ||
               !read_at(&engine->reader, engine->cursor, repeat, sizeof(repeat))) {
                engine_fail(engine);
                return false;
            }
            uint32_t duration = clipboard_read_u32(repeat), low = clipboard_read_u32(repeat + 4),
                     high = clipboard_read_u32(repeat + 8);
            uint32_t size = clipboard_read_u32(repeat + 14);
            uint16_t count = clipboard_read_u16(repeat + 12);
            engine->cursor += sizeof(repeat);
            if(!valid_timing(duration, low, high) || !count || count > CLIPBOARD_OPERATION_MAX ||
               size > engine->reader.size - engine->cursor) {
                engine_fail(engine);
                return false;
            }
            engine->repeat_active = true;
            engine->repeat_end = now + duration;
            engine->repeat_min = low;
            engine->repeat_max = high;
            engine->repeat_cycle = now;
            engine->repeat_start = engine->cursor;
            engine->repeat_after = engine->cursor + size;
            engine->repeat_count = count;
            engine->outer_remaining = engine->remaining;
            engine->remaining = count;
            engine->due = now;
            return false;
        } else if(record[0] == 7) {
            if(engine->repeat_active || engine->cursor != 7 ||
               !read_at(&engine->reader, engine->cursor, record, 4)) {
                engine_fail(engine);
                return false;
            }
            engine->cursor += 4;
            engine->typing_pause = clipboard_read_u16(record);
            engine->typing_variation = clipboard_read_u16(record + 2);
            if(!valid_typing(engine->typing_pause, engine->typing_variation)) {
                engine_fail(engine);
                return false;
            }
        } else {
            engine_fail(engine);
            return false;
        }
    }
    return false;
}

void clipboard_engine_tick(ClipboardEngine* engine, uint32_t now, bool usb_connected) {
    if(engine->state != ClipboardEngineRunning) return;
    if(!usb_connected) {
        engine_fail(engine);
        return;
    }
    if(engine->repeat_active && (int32_t)(now - engine->repeat_end) >= 0) {
        if(engine->phase && !engine->keyboard.release(engine->keyboard.context)) {
            engine_fail(engine);
            return;
        }
        engine->repeat_active = false;
        engine->phase = 0;
        engine->finish_grave = false;
        engine->mouse_active = engine->mouse_return = false;
        engine->cursor = engine->repeat_after;
        engine->text_end = 0;
        engine->remaining = engine->outer_remaining;
        engine->due = now + 8;
        return;
    }
    if(engine->mouse_active && (int32_t)(now - engine->mouse_end) >= 0) {
        engine->mouse_active = engine->mouse_return = false;
        engine->due = now;
    }
    if((int32_t)(now - engine->due) < 0) return;
    if(engine->mouse_active) {
        if(!engine->keyboard.mouse_move ||
           !engine->keyboard.mouse_move(
               engine->keyboard.context, engine->mouse_return ? -1 : 1, 0)) {
            engine_fail(engine);
            return;
        }
        engine->due = now + (engine->mouse_return ? engine->mouse_interval - 50 : 50);
        engine->mouse_return = !engine->mouse_return;
        return;
    }
    if(engine->phase == 1) {
        if(!engine->keyboard.release(engine->keyboard.context)) {
            engine_fail(engine);
            return;
        }
        engine->phase = engine->finish_grave ? 2 : 0;
        uint32_t pause = 0;
        if(engine->phase == 0 && engine->literal_key) {
            uint32_t sample;
            if(!random_index(engine, 2U * engine->typing_variation + 1U, &sample)) {
                engine_fail(engine);
                return;
            }
            pause = engine->typing_pause - engine->typing_variation + sample;
        }
        engine->due = now + 8 + pause;
        return;
    }
    uint16_t key;
    if(engine->phase == 2) {
        key = 0x2C;
        engine->finish_grave = false;
    } else if(!next_key(engine, now, &key))
        return;
    if(!engine->keyboard.press(engine->keyboard.context, key)) {
        engine_fail(engine);
        return;
    }
    engine->phase = 1;
    engine->due = now + 12;
}
