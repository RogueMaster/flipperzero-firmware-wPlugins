#pragma once

#include <furi.h>
#include <gui/canvas.h>
#include "vector.h"

enum DrawMode {
    WhiteOnly,
    BlackOnly,
    WhiteAsBlack,
    BlackAsWhite,
    WhiteAsInverted,
    BlackAsInverted,
};

typedef struct {
    uint8_t* data;
    uint8_t* back_buffer;
    uint8_t width;
    uint8_t height;
    bool double_buffered;
} Buffer;

enum PixelColor {
    Black, //or
    White, //
    Flip //not
};

typedef struct {
    const Vector anchor;
    enum DrawMode drawMode;
} RenderSettings;

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define DEFAULT_RENDER                                          \
    (RenderSettings) {                                          \
        .anchor = {.x = 0.5f, .y = 0.5f}, .drawMode = BlackOnly \
    }

Buffer* buffer_create(uint8_t width, uint8_t height, bool double_buffered);

void buffer_release(Buffer* buffer);

bool buffer_test_coordinate(Buffer* const buffer, int x, int y);

bool buffer_get_pixel(Buffer* const buffer, int x, int y);

void buffer_set_pixel(Buffer* buffer, int16_t x, int16_t y, enum PixelColor draw_mode);

Buffer* buffer_copy(Buffer* buffer);

void buffer_swap_back(Buffer* buffer);

void buffer_swap_with(Buffer* buffer_a, Buffer* buffer_b);

void buffer_draw_all(Buffer* target, Buffer* const sprite, Vector* position, float rotation);

void buffer_draw(
    Buffer* target,
    Buffer* const sprite,
    Vector* position,
    uint8_t x_cap,
    uint8_t y_cap,
    float rotation,
    RenderSettings* settings);

//void buffer_draw_internal(Buffer *target, Buffer* sprite, bool is_black, enum PixelColor color, Vector *const position, uint8_t x_cap, uint8_t y_cap, float rotation);
void buffer_render(Buffer* buffer, Canvas* const canvas);
void buffer_clear(Buffer* buffer);

void buffer_draw_line(Buffer* buffer, int x0, int y0, int x1, int y1, enum PixelColor draw_mode);

void buffer_draw_rbox(
    Buffer* buffer,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    enum PixelColor draw_mode);

void buffer_draw_rbox_frame(
    Buffer* buffer,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    enum PixelColor draw_mode);

void buffer_draw_box(
    Buffer* buffer,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    enum PixelColor draw_mode);

void buffer_draw_circle(Buffer* buffer, int x, int y, int r, enum PixelColor draw_mode);
