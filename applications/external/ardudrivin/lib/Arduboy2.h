#pragma once

#include "Arduino.h"
#include "ArduboyTones.h"
#include "ArduboyAudio.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <furi.h>
#include <input/input.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#ifdef __cplusplus
}
#endif

using uint24_t = uint32_t;

#ifndef WIDTH
#define WIDTH 128
#endif
#ifndef HEIGHT
#define HEIGHT 64
#endif

#ifndef UP_BUTTON
#define UP_BUTTON    0x01
#define DOWN_BUTTON  0x02
#define LEFT_BUTTON  0x04
#define RIGHT_BUTTON 0x08
#define A_BUTTON     0x10
#define B_BUTTON     0x20
#endif

#ifndef INPUT_UP
#define INPUT_UP UP_BUTTON
#endif
#ifndef INPUT_DOWN
#define INPUT_DOWN DOWN_BUTTON
#endif
#ifndef INPUT_LEFT
#define INPUT_LEFT LEFT_BUTTON
#endif
#ifndef INPUT_RIGHT
#define INPUT_RIGHT RIGHT_BUTTON
#endif
#ifndef INPUT_A
#define INPUT_A A_BUTTON
#endif
#ifndef INPUT_B
#define INPUT_B B_BUTTON
#endif

#define ARDUBOY_LIB_VER               60000
#define ARDUBOY_UNIT_NAME_LEN         6
#define ARDUBOY_UNIT_NAME_BUFFER_SIZE (ARDUBOY_UNIT_NAME_LEN + 1)
#define EEPROM_STORAGE_SPACE_START    16
#define BLACK                         1
#define WHITE                         0
#define INVERT                        2
#define CLEAR_BUFFER                  true

struct Rect {
    int x;
    int y;
    int width;
    int height;

    Rect() = default;
    constexpr Rect(int x, int y, int width, int height)
        : x(x)
        , y(y)
        , width(width)
        , height(height) {
    }
};

struct Point {
    int x;
    int y;
    Point() = default;
    constexpr Point(int x, int y)
        : x(x)
        , y(y) {
    }
};

class Arduboy2Base {
public:
    ArduboyAudio audio;

    struct InputRuntime {
        volatile uint8_t held = 0;
        volatile uint8_t press_latch = 0;
        volatile uint8_t release_latch = 0;
    };

    struct InputContext {
        volatile uint8_t* input_state = nullptr;
        InputRuntime* runtime = nullptr;
    };

    InputContext* inputContext() {
        return &input_ctx_;
    }

    void clearInputMask(uint8_t input_mask) {
        FlipperInputClearMask(input_mask, &input_ctx_);
    }

    void begin(
        uint8_t* screen_buffer,
        volatile uint8_t* input_state,
        FuriMutex* game_mutex,
        volatile bool* exit_requested)
    {
        sBuffer = screen_buffer;
        input_state_ = input_state;
        input_ctx_.input_state = input_state_;
        input_ctx_.runtime = &input_runtime_;
        game_mutex_ = game_mutex;
        exit_requested_ = exit_requested;
        external_timing_ = false;
        frame_count_ = 0;
        last_frame_ms_ = 0;
        resetInputState();
    }

    void exitToBootloader() {
        if(exit_requested_) {
            *exit_requested_ = true;
        }
    }

    bool collide(Point point, Rect rect) {
        return (point.x >= rect.x) && (point.x < rect.x + rect.width) && (point.y >= rect.y) &&
               (point.y < rect.y + rect.height);
    }

    bool collide(Rect rect1, Rect rect2) {
        return !(
            rect2.x >= rect1.x + rect1.width || rect2.x + rect2.width <= rect1.x ||
            rect2.y >= rect1.y + rect1.height || rect2.y + rect2.height <= rect1.y);
    }

    static void FlipperInputCallback(const InputEvent* event, void* ctx_ptr) {
        if(!event || !ctx_ptr) return;
        InputContext* ctx = (InputContext*)ctx_ptr;
        volatile uint8_t* st = ctx->input_state;
        InputRuntime* rt = ctx->runtime;
        if(!st && !rt) return;

        const uint8_t bit = FlipperInputMaskFromKey_(event->key);
        if(!bit) return;

        if(event->type == InputTypePress) {
            if(rt) {
                (void)__atomic_fetch_or((uint8_t*)&rt->held, bit, __ATOMIC_RELAXED);
                (void)__atomic_fetch_or((uint8_t*)&rt->press_latch, bit, __ATOMIC_RELAXED);
            }
            if(st) {
                (void)__atomic_fetch_or((uint8_t*)st, bit, __ATOMIC_RELAXED);
            }
        } else if(event->type == InputTypeRepeat) {
            // Repeat keeps the held state alive, but must not retrigger justPressed edges.
            if(rt) {
                (void)__atomic_fetch_or((uint8_t*)&rt->held, bit, __ATOMIC_RELAXED);
            }
            if(st) {
                (void)__atomic_fetch_or((uint8_t*)st, bit, __ATOMIC_RELAXED);
            }
        } else if(event->type == InputTypeRelease) {
            if(rt) {
                (void)__atomic_fetch_and((uint8_t*)&rt->held, (uint8_t)~bit, __ATOMIC_RELAXED);
                (void)__atomic_fetch_or((uint8_t*)&rt->release_latch, bit, __ATOMIC_RELAXED);
            }
            if(st) {
                (void)__atomic_fetch_and((uint8_t*)st, (uint8_t)~bit, __ATOMIC_RELAXED);
            }
        }
    }

    static void FlipperInputClearMask(uint8_t input_mask, void* ctx_ptr) {
        if(!input_mask || !ctx_ptr) return;
        InputContext* ctx = (InputContext*)ctx_ptr;

        if(ctx->runtime) {
            (void)__atomic_fetch_and(
                (uint8_t*)&ctx->runtime->held, (uint8_t)~input_mask, __ATOMIC_RELAXED);
            (void)__atomic_fetch_and(
                (uint8_t*)&ctx->runtime->press_latch, (uint8_t)~input_mask, __ATOMIC_RELAXED);
            (void)__atomic_fetch_and(
                (uint8_t*)&ctx->runtime->release_latch, (uint8_t)~input_mask, __ATOMIC_RELAXED);
        }

        if(ctx->input_state) {
            (void)__atomic_fetch_and(
                (uint8_t*)ctx->input_state, (uint8_t)~input_mask, __ATOMIC_RELAXED);
        }
    }

    void boot() {
    }
    void bootLogo() {
    }
    void bootLogoSpritesSelfMasked() {
    }

    void setFrameRate(uint8_t fps) {
        if(fps == 0) fps = 60;
        uint32_t d = 1000u / fps;
        frame_duration_ms_ = d ? d : 1u;
    }

    bool nextFrame() {
        if(external_timing_) {
            frame_count_++;
            return true;
        }
        const uint32_t now = millis();
        if((uint32_t)(now - last_frame_ms_) < frame_duration_ms_) return false;
        last_frame_ms_ = now;
        frame_count_++;
        return true;
    }

    bool everyXFrames(uint8_t n) const {
        if(n == 0) return false;
        return (frame_count_ % n) == 0;
    }

    void pollButtons() {
        prev_buttons_ = cur_buttons_;
        uint8_t in = 0;
        uint8_t press = 0;
        uint8_t release = 0;

        if(input_ctx_.runtime) {
            in = __atomic_load_n((uint8_t*)&input_ctx_.runtime->held, __ATOMIC_RELAXED);
            press = __atomic_exchange_n((uint8_t*)&input_ctx_.runtime->press_latch, 0, __ATOMIC_RELAXED);
            release =
                __atomic_exchange_n((uint8_t*)&input_ctx_.runtime->release_latch, 0, __ATOMIC_RELAXED);
        } else if(input_state_) {
            in = __atomic_load_n((uint8_t*)input_state_, __ATOMIC_RELAXED);
        }

        cur_buttons_ = mapInputToArduboyMask_(in);
        press_edges_ = mapInputToArduboyMask_(press);
        release_edges_ = mapInputToArduboyMask_(release);
    }

    void clearButtonState() {
        prev_buttons_ = 0;
        cur_buttons_ = 0;
        press_edges_ = 0;
        release_edges_ = 0;
        if(input_ctx_.runtime) {
            (void)__atomic_store_n((uint8_t*)&input_ctx_.runtime->press_latch, 0, __ATOMIC_RELAXED);
            (void)__atomic_store_n(
                (uint8_t*)&input_ctx_.runtime->release_latch, 0, __ATOMIC_RELAXED);
        }
    }

    void resetInputState() {
        if(input_ctx_.runtime) {
            (void)__atomic_store_n((uint8_t*)&input_ctx_.runtime->held, 0, __ATOMIC_RELAXED);
            (void)__atomic_store_n((uint8_t*)&input_ctx_.runtime->press_latch, 0, __ATOMIC_RELAXED);
            (void)__atomic_store_n(
                (uint8_t*)&input_ctx_.runtime->release_latch, 0, __ATOMIC_RELAXED);
        }
        if(input_state_) {
            (void)__atomic_store_n((uint8_t*)input_state_, 0, __ATOMIC_RELAXED);
        }
        clearButtonState();
    }

    bool pressed(uint8_t mask) const {
        return (cur_buttons_ & mask) != 0;
    }
    bool notPressed(uint8_t mask) const {
        return (cur_buttons_ & mask) == 0;
    }
    bool justPressed(uint8_t mask) const {
        return ((press_edges_ & mask) != 0) ||
               (((cur_buttons_ & mask) != 0) && ((prev_buttons_ & mask) == 0));
    }
    bool justReleased(uint8_t mask) const {
        return ((release_edges_ & mask) != 0) ||
               (((cur_buttons_ & mask) == 0) && ((prev_buttons_ & mask) != 0));
    }

    uint8_t* getBuffer() {
        return sBuffer;
    }
    const uint8_t* getBuffer() const {
        return sBuffer;
    }

    void clear() {
        fillScreen(0);
    }

    void fillScreen(uint8_t color) {
        if(!sBuffer) return;
        memset(sBuffer, color ? 0xFF : 0x00, (WIDTH * HEIGHT) / 8);
    }

    void display() {
    }

    void drawPixel(int16_t x, int16_t y, uint8_t color) {
        if(!sBuffer) return;
        if((uint16_t)x >= (uint16_t)WIDTH || (uint16_t)y >= (uint16_t)HEIGHT) return;

        uint8_t* dst = &sBuffer[(uint16_t)x + (uint16_t)((y >> 3) * WIDTH)];
        const uint8_t mask = (uint8_t)(1u << (y & 7));
        if(color)
            *dst |= mask;
        else
            *dst &= (uint8_t)~mask;
    }

    void drawBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
        if(!bmp || !sBuffer) return;
        const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
        const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
        const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
        blitSelfMasked_(x, y, data, w, h);
    }

    void drawSolidBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
        if(!bmp || !sBuffer) return;
        const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
        const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
        const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
        blitOverwrite_(x, y, data, w, h);
    }

    void drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame) {
        if(!plusmask || !sBuffer) return;
        const int16_t w = (int16_t)pgm_read_byte(plusmask + 0);
        const int16_t h = (int16_t)pgm_read_byte(plusmask + 1);
        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
        const uint8_t* data = plusmask + 2 + (uint32_t)frame * (uint32_t)frame_size * 2u;
        blitPlusMask_(x, y, data, w, h);
    }

    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
        if(r <= 0) return;
        int16_t x = r;
        int16_t y = 0;
        int16_t err = 1 - x;

        while(x >= y) {
            drawPixel(x0 + x, y0 + y, color);
            drawPixel(x0 + y, y0 + x, color);
            drawPixel(x0 - y, y0 + x, color);
            drawPixel(x0 - x, y0 + y, color);
            drawPixel(x0 - x, y0 - y, color);
            drawPixel(x0 - y, y0 - x, color);
            drawPixel(x0 + y, y0 - x, color);
            drawPixel(x0 + x, y0 - y, color);

            y++;
            if(err < 0) {
                err += (int16_t)(2 * y + 1);
            } else {
                x--;
                err += (int16_t)(2 * (y - x) + 1);
            }
        }
    }

    void drawSprite(
        int16_t x,
        int16_t y,
        const uint8_t* bmp,
        const uint8_t* mask,
        uint8_t frame,
        uint8_t mask_frame) {
        if(!bmp || !sBuffer) return;

        const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
        const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;

        if(mask) {
            const uint8_t* sprite_data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
            const uint8_t* mask_data = mask + (uint32_t)mask_frame * (uint32_t)frame_size;
            blitExternalMask_(x, y, sprite_data, mask_data, w, h);
        } else {
            const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size * 2u;
            blitPlusMask_(x, y, data, w, h);
        }
    }

    void setRGBled(uint8_t r, uint8_t g, uint8_t b) {
        NotificationApp* n = (NotificationApp*)furi_record_open(RECORD_NOTIFICATION);
        if(!n) return;

        notification_message(n, r ? &sequence_set_red_255 : &sequence_reset_red);
        notification_message(n, g ? &sequence_set_green_255 : &sequence_reset_green);
        notification_message(n, b ? &sequence_set_blue_255 : &sequence_reset_blue);

        furi_record_close(RECORD_NOTIFICATION);
    }

    void expectLoadDelay() {
        last_frame_ms_ = millis();
    }

    uint32_t frameCount() const {
        return frame_count_;
    }


  static constexpr uint16_t eepromSysFlags = 1;
    // Audio mute control. 0 = audio off, non-zero = audio on
  static constexpr uint16_t eepromAudioOnOff = 2;
    // -- Addresses 3-7 are currently reserved for future use --
    // A uint16_t binary unit ID
  static constexpr uint16_t eepromUnitID = 8; // A uint16_t binary unit ID
    // An up to 6 character unit name
    // The name cannot contain 0x00, 0xFF, 0x0A, 0x0D
    // Lengths less than 6 are padded with 0x00
  static constexpr uint16_t eepromUnitName = 10;
    // -- User EEPROM space starts at address 16 --

  // --- Map of the bits in the eepromSysFlags byte --
    // Display the unit name on the logo screen
  static constexpr uint8_t sysFlagUnameBit = 0;
  static constexpr uint8_t sysFlagUnameMask = _BV(sysFlagUnameBit);
    // Show the logo sequence during boot up
  static constexpr uint8_t sysFlagShowLogoBit = 1;
  static constexpr uint8_t sysFlagShowLogoMask = _BV(sysFlagShowLogoBit);
    // Flash the RGB led during the boot logo
  static constexpr uint8_t sysFlagShowLogoLEDsBit = 2;
  static constexpr uint8_t sysFlagShowLogoLEDsMask = _BV(sysFlagShowLogoLEDsBit);
uint8_t* sBuffer = nullptr;

private:
    static uint8_t FlipperInputMaskFromKey_(InputKey key) {
        switch(key) {
        case InputKeyUp:
            return INPUT_UP;
        case InputKeyDown:
            return INPUT_DOWN;
        case InputKeyLeft:
            return INPUT_LEFT;
        case InputKeyRight:
            return INPUT_RIGHT;
        case InputKeyOk:
            return INPUT_B;
        case InputKeyBack:
            return INPUT_A;
        default:
            return 0;
        }
    }

    volatile bool* exit_requested_ = nullptr;

    static inline uint8_t page_mask_(int16_t p, int16_t pages, int16_t h) {
        if(p != pages - 1) return 0xFF;
        const int16_t rem = (h & 7);
        if(rem == 0) return 0xFF;
        return (uint8_t)((1u << rem) - 1u);
    }

    void blitSelfMasked_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h) {
        if(w <= 0 || h <= 0) return;

        const int16_t yOffset = (int16_t)(y & 7);
        const int16_t sRow = (int16_t)(y >> 3);
        const int16_t pages = (h + 7) >> 3;
        const int16_t maxRow = (HEIGHT >> 3);

        for(int16_t i = 0; i < w; i++) {
            const int16_t sx = (int16_t)(x + i);
            if((uint16_t)sx >= (uint16_t)WIDTH) continue;

            const uint8_t* col = src + i;
            if(yOffset == 0) {
                int16_t row = sRow;
                uint8_t* dst = sBuffer + row * WIDTH + sx;
                for(int16_t p = 0; p < pages; p++, row++, col += w) {
                    if((uint16_t)row >= (uint16_t)maxRow) continue;
                    uint8_t b = pgm_read_byte(col);
                    b &= page_mask_(p, pages, h);
                    dst[p * WIDTH] |= b;
                }
            } else {
                for(int16_t p = 0; p < pages; p++, col += w) {
                    uint8_t b = pgm_read_byte(col);
                    b &= page_mask_(p, pages, h);

                    const int16_t row = (int16_t)(sRow + p);
                    const uint8_t lo = (uint8_t)(b << yOffset);
                    const uint8_t hi = (uint8_t)(b >> (8 - yOffset));

                    if((uint16_t)row < (uint16_t)maxRow) {
                        sBuffer[row * WIDTH + sx] |= lo;
                    }
                    const int16_t row2 = (int16_t)(row + 1);
                    if((uint16_t)row2 < (uint16_t)maxRow) {
                        sBuffer[row2 * WIDTH + sx] |= hi;
                    }
                }
            }
        }
    }

    void blitOverwrite_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h) {
        if(w <= 0 || h <= 0) return;

        const int16_t yOffset = (int16_t)(y & 7);
        const int16_t sRow = (int16_t)(y >> 3);
        const int16_t pages = (h + 7) >> 3;
        const int16_t maxRow = (HEIGHT >> 3);

        for(int16_t i = 0; i < w; i++) {
            const int16_t sx = (int16_t)(x + i);
            if((uint16_t)sx >= (uint16_t)WIDTH) continue;

            const uint8_t* col = src + i;
            if(yOffset == 0) {
                int16_t row = sRow;
                for(int16_t p = 0; p < pages; p++, row++, col += w) {
                    if((uint16_t)row >= (uint16_t)maxRow) continue;
                    uint8_t b = pgm_read_byte(col);
                    b &= page_mask_(p, pages, h);
                    sBuffer[row * WIDTH + sx] = b;
                }
            } else {
                for(int16_t p = 0; p < pages; p++, col += w) {
                    uint8_t b = pgm_read_byte(col);
                    uint8_t srcMask = page_mask_(p, pages, h);
                    b &= srcMask;

                    const int16_t row = (int16_t)(sRow + p);

                    const uint8_t lo = (uint8_t)(b << yOffset);
                    const uint8_t hi = (uint8_t)(b >> (8 - yOffset));
                    const uint8_t maskLo = (uint8_t)(srcMask << yOffset);
                    const uint8_t maskHi = (uint8_t)(srcMask >> (8 - yOffset));

                    if((uint16_t)row < (uint16_t)maxRow) {
                        uint8_t* dst = &sBuffer[row * WIDTH + sx];
                        *dst = (uint8_t)((*dst & (uint8_t)~maskLo) | (lo & maskLo));
                    }
                    const int16_t row2 = (int16_t)(row + 1);
                    if((uint16_t)row2 < (uint16_t)maxRow) {
                        uint8_t* dst2 = &sBuffer[row2 * WIDTH + sx];
                        *dst2 = (uint8_t)((*dst2 & (uint8_t)~maskHi) | (hi & maskHi));
                    }
                }
            }
        }
    }

    void blitPlusMask_(int16_t x, int16_t y, const uint8_t* srcPairs, int16_t w, int16_t h) {
        if(w <= 0 || h <= 0) return;

        const int16_t yOffset = (int16_t)(y & 7);
        const int16_t sRow = (int16_t)(y >> 3);
        const int16_t pages = (h + 7) >> 3;
        const int16_t maxRow = (HEIGHT >> 3);

        for(int16_t i = 0; i < w; i++) {
            const int16_t sx = (int16_t)(x + i);
            if((uint16_t)sx >= (uint16_t)WIDTH) continue;

            for(int16_t p = 0; p < pages; p++) {
                const uint16_t idx = (uint16_t)(i + p * w) * 2u;
                uint8_t s = pgm_read_byte(srcPairs + idx + 0);
                uint8_t m = pgm_read_byte(srcPairs + idx + 1);

                uint8_t srcMask = page_mask_(p, pages, h);
                s &= srcMask;
                m &= srcMask;

                const int16_t row = (int16_t)(sRow + p);

                if(yOffset == 0) {
                    if((uint16_t)row < (uint16_t)maxRow) {
                        uint8_t* dst = &sBuffer[row * WIDTH + sx];
                        *dst = (uint8_t)((*dst & (uint8_t)~m) | (s & m));
                    }
                } else {
                    const uint8_t slo = (uint8_t)(s << yOffset);
                    const uint8_t shi = (uint8_t)(s >> (8 - yOffset));
                    const uint8_t mlo = (uint8_t)(m << yOffset);
                    const uint8_t mhi = (uint8_t)(m >> (8 - yOffset));

                    if((uint16_t)row < (uint16_t)maxRow) {
                        uint8_t* dst = &sBuffer[row * WIDTH + sx];
                        *dst = (uint8_t)((*dst & (uint8_t)~mlo) | (slo & mlo));
                    }
                    const int16_t row2 = (int16_t)(row + 1);
                    if((uint16_t)row2 < (uint16_t)maxRow) {
                        uint8_t* dst2 = &sBuffer[row2 * WIDTH + sx];
                        *dst2 = (uint8_t)((*dst2 & (uint8_t)~mhi) | (shi & mhi));
                    }
                }
            }
        }
    }

    void blitExternalMask_(
        int16_t x,
        int16_t y,
        const uint8_t* sprite,
        const uint8_t* mask,
        int16_t w,
        int16_t h) {
        if(w <= 0 || h <= 0) return;

        const int16_t yOffset = (int16_t)(y & 7);
        const int16_t sRow = (int16_t)(y >> 3);
        const int16_t pages = (h + 7) >> 3;
        const int16_t maxRow = (HEIGHT >> 3);

        for(int16_t i = 0; i < w; i++) {
            const int16_t sx = (int16_t)(x + i);
            if((uint16_t)sx >= (uint16_t)WIDTH) continue;

            const uint8_t* scol = sprite + i;
            const uint8_t* mcol = mask + i;

            if(yOffset == 0) {
                int16_t row = sRow;
                for(int16_t p = 0; p < pages; p++, row++, scol += w, mcol += w) {
                    if((uint16_t)row >= (uint16_t)maxRow) continue;

                    uint8_t s = pgm_read_byte(scol);
                    uint8_t m = pgm_read_byte(mcol);
                    uint8_t srcMask = page_mask_(p, pages, h);
                    s &= srcMask;
                    m &= srcMask;

                    uint8_t* dst = &sBuffer[row * WIDTH + sx];
                    *dst = (uint8_t)((*dst & (uint8_t)~m) | (s & m));
                }
            } else {
                for(int16_t p = 0; p < pages; p++, scol += w, mcol += w) {
                    uint8_t s = pgm_read_byte(scol);
                    uint8_t m = pgm_read_byte(mcol);

                    uint8_t srcMask = page_mask_(p, pages, h);
                    s &= srcMask;
                    m &= srcMask;

                    const int16_t row = (int16_t)(sRow + p);

                    const uint8_t slo = (uint8_t)(s << yOffset);
                    const uint8_t shi = (uint8_t)(s >> (8 - yOffset));
                    const uint8_t mlo = (uint8_t)(m << yOffset);
                    const uint8_t mhi = (uint8_t)(m >> (8 - yOffset));

                    if((uint16_t)row < (uint16_t)maxRow) {
                        uint8_t* dst = &sBuffer[row * WIDTH + sx];
                        *dst = (uint8_t)((*dst & (uint8_t)~mlo) | (slo & mlo));
                    }
                    const int16_t row2 = (int16_t)(row + 1);
                    if((uint16_t)row2 < (uint16_t)maxRow) {
                        uint8_t* dst2 = &sBuffer[row2 * WIDTH + sx];
                        *dst2 = (uint8_t)((*dst2 & (uint8_t)~mhi) | (shi & mhi));
                    }
                }
            }
        }
    }

    // static uint32_t millis() {
    //     const uint32_t tick = furi_get_tick();
    //     const uint32_t hz = furi_kernel_get_tick_frequency();
    //     if(hz == 0) return tick;
    //     return (tick * 1000u) / hz;
    // }

    static uint8_t mapInputToArduboyMask_(uint8_t in) {
        uint8_t out = 0;
        if(in & INPUT_UP) out |= UP_BUTTON;
        if(in & INPUT_DOWN) out |= DOWN_BUTTON;
        if(in & INPUT_LEFT) out |= LEFT_BUTTON;
        if(in & INPUT_RIGHT) out |= RIGHT_BUTTON;
        if(in & INPUT_B) out |= B_BUTTON;
        if(in & INPUT_A) out |= A_BUTTON;
        return out;
    }

    FuriMutex* game_mutex_ = nullptr;
    bool external_timing_ = false;

    volatile uint8_t* input_state_ = nullptr;
    InputRuntime input_runtime_{};
    InputContext input_ctx_{};

    uint32_t frame_duration_ms_ = 16;
    uint32_t last_frame_ms_ = 0;
    uint32_t frame_count_ = 0;

    uint8_t cur_buttons_ = 0;
    uint8_t prev_buttons_ = 0;
    uint8_t press_edges_ = 0;
    uint8_t release_edges_ = 0;
};

class Arduboy2 : public Arduboy2Base {};

class Sprites {
public:
    static void setArduboy(Arduboy2Base* a) {
        ab_ = a;
    }

    void drawOverwrite(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        if(!ab_ || !bmp) return;
        ab_->drawSolidBitmapFrame(x, y, bmp, frame);
    }

    void drawSelfMasked(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        if(!ab_ || !bmp) return;
        ab_->drawBitmapFrame(x, y, bmp, frame);
    }

    void drawErase(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        if(!ab_ || !bmp) return;

        const uint8_t w = pgm_read_byte(bmp + 0);
        const uint8_t h = pgm_read_byte(bmp + 1);
        const uint8_t pages = (uint8_t)((h + 7) >> 3);
        const uint16_t frame_size = (uint16_t)w * pages;
        const uint8_t* f = (bmp + 2) + (uint32_t)frame * frame_size;

        for(uint8_t page = 0; page < pages; page++) {
            for(uint8_t i = 0; i < w; i++) {
                uint8_t byte = pgm_read_byte(f + i + (uint16_t)page * w);
                while(byte) {
                    uint8_t b = (uint8_t)__builtin_ctz((unsigned)byte);
                    uint8_t py = (uint8_t)(page * 8u + b);
                    if(py < h) ab_->drawPixel((int16_t)(x + i), (int16_t)(y + py), 0);
                    byte = (uint8_t)(byte & (uint8_t)(byte - 1u));
                }
            }
        }
    }

    void drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame = 0) {
        if(!ab_) return;
        ab_->drawPlusMask(x, y, plusmask, frame);
    }

private:
    inline static Arduboy2Base* ab_ = nullptr;
};
