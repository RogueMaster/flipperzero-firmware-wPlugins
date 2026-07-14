#pragma once

#include "Arduino.h"
#include "ArduboyTones.h"
#include "ArduboyAudio.h"
#include "EEPROM.h"
#include "Tinyfont.h"
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
#if defined(ARDUBOY_COLOR_ORIGINAL)
#define BLACK 0
#define WHITE 1
#else
#define BLACK 1
#define WHITE 0
#endif
#define INVERT                        2
#define CLEAR_BUFFER                  true
#define RED_LED                       1
#define GREEN_LED                     2
#define BLUE_LED                      4
#define ARDUBOY_NO_USB

#include "include/obj.h"

class Arduboy2Base {
public:
    ArduboyAudio audio;

    // Прямой доступ к состоянию ввода - без InputContext/InputRuntime мостов
    volatile uint8_t* input_state_ = nullptr;
    volatile uint8_t* input_press_latch_ = nullptr;

    // Состояние кнопок хранится напрямую в классе
    uint8_t cur_buttons_ = 0;
    uint8_t prev_buttons_ = 0;
    uint8_t press_edges_ = 0;
    uint8_t release_edges_ = 0;

    // Счётчик кадров и тайминг
    uint32_t frame_count_ = 0;
    uint32_t frame_duration_ms_ = 33;
    uint32_t last_frame_ms_ = 0;

    // Курсор и текст
    int16_t cursor_x_ = 0;
    int16_t cursor_y_ = 0;
    uint8_t text_color_ = WHITE;
    uint8_t text_bg_ = BLACK;
    bool text_bg_enabled_ = false;

    // Флаги
    bool external_timing_ = false;

    void begin(
        uint8_t* screen_buffer,
        volatile uint8_t* input_state,
        volatile uint8_t* input_press_latch,
        FuriMutex* game_mutex,
        volatile bool* exit_requested);

    void begin();

    void exitToBootloader();

    bool collide(Point point, Rect rect);
    bool collide(Rect rect1, Rect rect2);

    // Прямой вызов из runtime.cpp без InputContext
    static void FlipperInputCallback(const InputEvent* event, void* ctx_ptr);
    static void FlipperInputClearMask(uint8_t input_mask, void* ctx_ptr);

    void boot();
    void bootLogo();
    void bootLogoSpritesSelfMasked();

    void setFrameRate(uint8_t fps);
    bool nextFrame();
    bool everyXFrames(uint8_t n) const;

    void pollButtons();
    void clearButtonState();
    void resetInputState();
    uint8_t justPressedButtons() const;
    uint8_t pressedButtons() const;

    bool pressed(uint8_t mask) const;
    bool notPressed(uint8_t mask) const;
    bool justPressed(uint8_t mask) const;
    bool justReleased(uint8_t mask) const;

    void resetFrameCount();
    uint16_t getFrameCount() const;
    void setFrameCount(uint16_t val) const;
    uint8_t getFrameCount(uint8_t mod, int8_t offset = 0) const;
    bool getFrameCountHalf(uint8_t mod) const;
    bool isFrameCount(uint8_t mod) const;
    bool isFrameCount(uint8_t mod, uint8_t val) const;
    uint8_t randomLFSR(uint8_t min, uint8_t max);

    uint8_t* getBuffer();
    const uint8_t* getBuffer() const;

    void clear();
    void fillScreen(uint8_t color);

    void display();
    void display(bool clear);

    void invert(bool);

    void drawPixel(int16_t x, int16_t y, uint8_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint8_t color);

    void setCursor(int16_t x, int16_t y);
    void setTextColor(uint8_t color);
    void setTextBackground(uint8_t color);

    size_t write(uint8_t c);
    void print(const char* text);
    void print(uint8_t value);
    void print(int value);
    void print(unsigned long value);

    void drawBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame);
    void drawSolidBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame);
    void eraseBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame);
    void drawSolidBitmapData(int16_t x, int16_t y, const uint8_t* data, uint8_t w, uint8_t h);
    void drawSelfMaskedData(int16_t x, int16_t y, const uint8_t* data, uint8_t w, uint8_t h);
    void drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame);
    void drawPlusMaskData(int16_t x, int16_t y, const uint8_t* data, uint8_t w, uint8_t h);

    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);

    void drawSprite(
        int16_t x,
        int16_t y,
        const uint8_t* bmp,
        const uint8_t* mask,
        uint8_t frame,
        uint8_t mask_frame);

    void setRGBled(uint8_t r, uint8_t g, uint8_t b);
    void setRGBled(uint8_t color, uint8_t val);

    void expectLoadDelay();

    uint32_t frameCount() const;

    uint8_t* sBuffer = nullptr;

private:
    static uint8_t FlipperInputMaskFromKey_(InputKey key);

    volatile bool* exit_requested_ = nullptr;
    FuriMutex* game_mutex_ = nullptr;

    static uint8_t page_mask_(int16_t p, int16_t pages, int16_t h);
    static bool isSpriteVisible_(int16_t x, int16_t y, int16_t w, int16_t h);
    static bool clipVisibleColumns_(int16_t x, int16_t w, int16_t& start, int16_t& end);
    static void buildPageMasks_(uint8_t* page_masks, int16_t pages, int16_t h);

    void blitSelfMasked_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h);
    void blitErase_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h);
    void blitOverwrite_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h);
    void blitPlusMask_(int16_t x, int16_t y, const uint8_t* srcPairs, int16_t w, int16_t h);
    void blitExternalMask_(
        int16_t x,
        int16_t y,
        const uint8_t* sprite,
        const uint8_t* mask,
        int16_t w,
        int16_t h);

    static uint8_t mapInputToArduboyMask_(uint8_t in);
    void printUnsigned_(unsigned long value);

    mutable int32_t frame_offset_ = 0;
};

class Arduboy2 : public Arduboy2Base {};

// Legacy compatibility shim used by older Arduino sketches (e.g. microtd).
class BeepPin1 {
public:
    void begin();
    void timer();
    uint16_t freq(uint16_t f) const;
    void tone(uint16_t, uint16_t);
};

class Sprites {
public:
    static void setArduboy(Arduboy2Base* a);
    static void drawOverwrite(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0);
    static void drawSelfMasked(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0);
    static void drawErase(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0);
    static void drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame = 0);

private:
    static Arduboy2Base* ab_;
};

extern Arduboy2Base arduboy;
extern Sprites sprites;
