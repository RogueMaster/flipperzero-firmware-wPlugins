#include <furi.h>
#include <furi_hal.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>

#include "lib/flipper.h"

#define COLOUR_WHITE 0
#define COLOUR_BLACK 1

#include "game/Game.h"
#include "game/Draw.h"
#include "game/FixedMath.h"
#include "game/Platform.h"
#include "game/Defines.h"
#include "game/Sounds.h"
#include "lib/EEPROM.h"

// ---------------- SOUND ----------------

typedef struct {
    const uint16_t* pattern;
} SoundRequest;

static FuriMessageQueue* g_sound_queue = NULL;
static FuriThread* g_sound_thread = NULL;
static volatile bool g_sound_thread_running = false;
static const float kSoundVolume = 1.0f;
static const uint32_t kToneTickHz = 780;

static inline uint32_t arduboy_ticks_to_ms(uint16_t ticks) {
    return (uint32_t)((ticks * 1000u + (kToneTickHz / 2)) / kToneTickHz);
}

static int32_t sound_thread_fn(void* ctx) {
    UNUSED(ctx);
    SoundRequest req;

    while(g_sound_thread_running) {
        if(furi_message_queue_get(g_sound_queue, &req, 50) != FuriStatusOk) continue;
        if(!g_state || !g_state->audio_enabled || !req.pattern) continue;
        if(!furi_hal_speaker_acquire(50)) continue;

        const uint16_t* p = req.pattern;
        while(g_sound_thread_running && g_state && g_state->audio_enabled) {
            SoundRequest new_req;
            if(furi_message_queue_get(g_sound_queue, &new_req, 0) == FuriStatusOk) {
                if(new_req.pattern) p = new_req.pattern;
            }

            uint16_t freq = *p++;
            if(freq == TONES_END) break;

            uint16_t dur_ticks = *p++;
            uint32_t dur_ms = arduboy_ticks_to_ms(dur_ticks);
            if(dur_ms == 0) dur_ms = 1;

            if(freq == 0) {
                furi_hal_speaker_stop();
                furi_delay_ms(dur_ms);
            } else {
                furi_hal_speaker_start((float)freq, kSoundVolume);
                furi_delay_ms(dur_ms);
                furi_hal_speaker_stop();
            }
        }

        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    return 0;
}

static void sound_system_init() {
    if(g_sound_queue || g_sound_thread) return;

    g_sound_queue = furi_message_queue_alloc(4, sizeof(SoundRequest));

    g_sound_thread = furi_thread_alloc();
    furi_thread_set_name(g_sound_thread, "GameSound");
    furi_thread_set_stack_size(g_sound_thread, 1024);
    furi_thread_set_priority(g_sound_thread, FuriThreadPriorityNormal);
    furi_thread_set_callback(g_sound_thread, sound_thread_fn);

    g_sound_thread_running = true;
    furi_thread_start(g_sound_thread);
}

static void sound_system_deinit() {
    if(!g_sound_thread) return;

    g_sound_thread_running = false;
    furi_thread_join(g_sound_thread);
    furi_thread_free(g_sound_thread);
    g_sound_thread = NULL;

    if(g_sound_queue) {
        furi_message_queue_free(g_sound_queue);
        g_sound_queue = NULL;
    }
}

void Platform::PlaySound(const uint16_t* audioPattern) {
    if(!g_state || !g_state->audio_enabled || !audioPattern || !g_sound_queue) return;

    SoundRequest req = {.pattern = audioPattern};
    if(furi_message_queue_put(g_sound_queue, &req, 0) != FuriStatusOk) {
        SoundRequest dummy;
        (void)furi_message_queue_get(g_sound_queue, &dummy, 0);
        (void)furi_message_queue_put(g_sound_queue, &req, 0);
    }
}

bool Platform::IsAudioEnabled() {
    return g_state && g_state->audio_enabled;
}

void Platform::SetAudioEnabled(bool enabled) {
    if(!g_state) return;
    bool was_enabled = g_state->audio_enabled;
    g_state->audio_enabled = enabled;
    if(enabled && !was_enabled)
        sound_system_init();
    else if(!enabled && was_enabled)
        sound_system_deinit();
}

// ---------------- INPUT ----------------

uint8_t Platform::GetInput() {
    return g_state ? g_state->input_state : 0;
}

// ---------------- DRAW ----------------

static constexpr uint8_t kDisplayPages = DISPLAY_HEIGHT / 8;

static inline int16_t floor_div8(int16_t value) {
    return (value >= 0) ? (value >> 3) : (int16_t)(-(((-value) + 7) >> 3));
}

static inline void set_pixel(int16_t x, int16_t y, bool color) {
    if(!g_state) return;
    if((uint16_t)x >= DISPLAY_WIDTH || (uint16_t)y >= DISPLAY_HEIGHT) return;

    uint8_t* buf = g_state->back_buffer;
    uint16_t idx = (uint16_t)(x + (y >> 3) * DISPLAY_WIDTH);
    uint8_t mask = (uint8_t)(1u << (y & 7));

    if(color)
        buf[idx] |= mask;
    else
        buf[idx] &= (uint8_t)~mask;
}

void Platform::PutPixel(uint8_t x, uint8_t y, uint8_t color) {
    set_pixel(x, y, color);
}

void Platform::FillScreen(uint8_t color) {
    if(!g_state) return;
    memset(g_state->back_buffer, color ? 0xFF : 0x00, BUFFER_SIZE);
}

uint8_t* Platform::GetScreenBuffer() {
    return g_state ? g_state->back_buffer : NULL;
}

void Platform::DrawVLine(uint8_t x, int8_t y0, int8_t y1, uint8_t pattern) {
    if(!g_state || pattern == 0 || x >= DISPLAY_WIDTH) return;

    int16_t top = y0;
    int16_t bottom = y1;

    if(top > bottom) {
        int16_t t = top;
        top = bottom;
        bottom = t;
    }

    if(bottom < 0 || top >= DISPLAY_HEIGHT) return;

    if(top < 0) top = 0;
    if(bottom >= DISPLAY_HEIGHT) bottom = DISPLAY_HEIGHT - 1;

    uint8_t start_page = (uint8_t)(top >> 3);
    uint8_t end_page = (uint8_t)(bottom >> 3);
    uint8_t start_bit = (uint8_t)(top & 7);
    uint8_t end_bit = (uint8_t)(bottom & 7);

    uint8_t* buf = g_state->back_buffer;

    if(start_page == end_page) {
        uint8_t clip_mask =
            (uint8_t)((uint8_t)(0xFFu << start_bit) & (uint8_t)(0xFFu >> (7u - end_bit)));
        buf[(uint16_t)x + (uint16_t)start_page * DISPLAY_WIDTH] |= (uint8_t)(pattern & clip_mask);
        return;
    }

    buf[(uint16_t)x + (uint16_t)start_page * DISPLAY_WIDTH] |=
        (uint8_t)(pattern & (uint8_t)(0xFFu << start_bit));

    for(uint8_t page = (uint8_t)(start_page + 1); page < end_page; page++) {
        buf[(uint16_t)x + (uint16_t)page * DISPLAY_WIDTH] |= pattern;
    }

    buf[(uint16_t)x + (uint16_t)end_page * DISPLAY_WIDTH] |=
        (uint8_t)(pattern & (uint8_t)(0xFFu >> (7u - end_bit)));
}

static inline uint8_t get_page_mask(uint8_t page, uint8_t total_pages, uint8_t height) {
    if((page + 1u) != total_pages) return 0xFFu;
    uint8_t tail_bits = (uint8_t)(height & 7u);
    return tail_bits ? (uint8_t)((1u << tail_bits) - 1u) : 0xFFu;
}

void Platform::DrawBitmap(int16_t x, int16_t y, const uint8_t* bmp) {
    if(!g_state || !bmp) return;

    uint8_t w = bmp[0];
    uint8_t h = bmp[1];
    if(!w || !h) return;

    int16_t x0 = x < 0 ? 0 : x;
    int16_t x1 = x + w;
    if(x1 > DISPLAY_WIDTH) x1 = DISPLAY_WIDTH;
    if(x0 >= x1) return;

    const uint8_t* data = bmp + 2;
    uint8_t pages = (uint8_t)((h + 7u) >> 3);
    uint8_t* dst = g_state->back_buffer;

    for(int16_t dx = x0; dx < x1; dx++) {
        uint8_t sx = (uint8_t)(dx - x);
        const uint8_t* src_col = data + sx;

        for(uint8_t page = 0; page < pages; page++) {
            int16_t base_y = y + ((int16_t)page << 3);
            if(base_y <= -8 || base_y >= DISPLAY_HEIGHT) continue;

            uint8_t src = src_col[(uint16_t)page * w] & get_page_mask(page, pages, h);
            if(src == 0) continue;

            int16_t dst_page = floor_div8(base_y);
            uint8_t y_shift = (uint8_t)(base_y - (dst_page << 3));

            uint8_t low = (uint8_t)(src << y_shift);
            if((uint16_t)dst_page < kDisplayPages) {
                uint16_t idx = (uint16_t)dx + (uint16_t)dst_page * DISPLAY_WIDTH;
                dst[idx] &= (uint8_t)~low;
            }

            if(y_shift && (uint16_t)(dst_page + 1) < kDisplayPages) {
                uint8_t high = (uint8_t)(src >> (8u - y_shift));
                uint16_t idx = (uint16_t)dx + (uint16_t)(dst_page + 1) * DISPLAY_WIDTH;
                dst[idx] &= (uint8_t)~high;
            }
        }
    }
}

void Platform::DrawSolidBitmap(int16_t x, int16_t y, const uint8_t* bmp) {
    if(!g_state || !bmp) return;

    uint8_t w = bmp[0];
    uint8_t h = bmp[1];
    if(!w || !h) return;

    int16_t x0 = x < 0 ? 0 : x;
    int16_t x1 = x + w;
    if(x1 > DISPLAY_WIDTH) x1 = DISPLAY_WIDTH;
    if(x0 >= x1) return;

    const uint8_t* data = bmp + 2;
    uint8_t pages = (uint8_t)((h + 7u) >> 3);
    uint8_t* dst = g_state->back_buffer;

    for(int16_t dx = x0; dx < x1; dx++) {
        uint8_t sx = (uint8_t)(dx - x);
        const uint8_t* src_col = data + sx;

        for(uint8_t page = 0; page < pages; page++) {
            int16_t base_y = y + ((int16_t)page << 3);
            if(base_y <= -8 || base_y >= DISPLAY_HEIGHT) continue;

            uint8_t page_mask = get_page_mask(page, pages, h);
            uint8_t src = src_col[(uint16_t)page * w] & page_mask;
            uint8_t fill = (uint8_t)(~src) & page_mask;

            int16_t dst_page = floor_div8(base_y);
            uint8_t y_shift = (uint8_t)(base_y - (dst_page << 3));

            uint8_t region_low = (uint8_t)(page_mask << y_shift);
            uint8_t fill_low = (uint8_t)(fill << y_shift);
            if((uint16_t)dst_page < kDisplayPages) {
                uint16_t idx = (uint16_t)dx + (uint16_t)dst_page * DISPLAY_WIDTH;
                dst[idx] = (uint8_t)((dst[idx] & (uint8_t)~region_low) | fill_low);
            }

            if(y_shift && (uint16_t)(dst_page + 1) < kDisplayPages) {
                uint8_t region_high = (uint8_t)(page_mask >> (8u - y_shift));
                uint8_t fill_high = (uint8_t)(fill >> (8u - y_shift));
                uint16_t idx = (uint16_t)dx + (uint16_t)(dst_page + 1) * DISPLAY_WIDTH;
                dst[idx] = (uint8_t)((dst[idx] & (uint8_t)~region_high) | fill_high);
            }
        }
    }
}

void Platform::DrawSprite(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
    if(!g_state || !bmp) return;

    uint8_t w = bmp[0];
    uint8_t h = bmp[1];
    if(!w || !h) return;

    int16_t x0 = x < 0 ? 0 : x;
    int16_t x1 = x + w;
    if(x1 > DISPLAY_WIDTH) x1 = DISPLAY_WIDTH;
    if(x0 >= x1) return;

    uint8_t pages = (uint8_t)((h + 7u) >> 3);
    uint16_t frame_size = (uint16_t)(w * pages);
    const uint8_t* data = bmp + 2 + (uint32_t)frame * frame_size * 2u;
    uint8_t* dst = g_state->back_buffer;

    for(int16_t dx = x0; dx < x1; dx++) {
        uint8_t sx = (uint8_t)(dx - x);

        for(uint8_t page = 0; page < pages; page++) {
            int16_t base_y = y + ((int16_t)page << 3);
            if(base_y <= -8 || base_y >= DISPLAY_HEIGHT) continue;

            uint16_t src_index = (uint16_t)((page * w + sx) * 2u);
            uint8_t src = data[src_index];
            uint8_t mask = data[src_index + 1] & get_page_mask(page, pages, h);
            if(mask == 0) continue;

            uint8_t fill = (uint8_t)(src & mask);
            int16_t dst_page = floor_div8(base_y);
            uint8_t y_shift = (uint8_t)(base_y - (dst_page << 3));

            uint8_t region_low = (uint8_t)(mask << y_shift);
            uint8_t fill_low = (uint8_t)(fill << y_shift);
            if((uint16_t)dst_page < kDisplayPages) {
                uint16_t idx = (uint16_t)dx + (uint16_t)dst_page * DISPLAY_WIDTH;
                dst[idx] = (uint8_t)((dst[idx] & (uint8_t)~region_low) | fill_low);
            }

            if(y_shift && (uint16_t)(dst_page + 1) < kDisplayPages) {
                uint8_t region_high = (uint8_t)(mask >> (8u - y_shift));
                uint8_t fill_high = (uint8_t)(fill >> (8u - y_shift));
                uint16_t idx = (uint16_t)dx + (uint16_t)(dst_page + 1) * DISPLAY_WIDTH;
                dst[idx] = (uint8_t)((dst[idx] & (uint8_t)~region_high) | fill_high);
            }
        }
    }
}
