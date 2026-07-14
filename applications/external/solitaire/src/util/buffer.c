#include "buffer.h"
#include "helpers.h"
#include <memory.h>
#include <furi.h>

static RenderSettings default_render = DEFAULT_RENDER;

uint16_t pixel(uint8_t x, uint8_t y, uint8_t w) {
    return (y * w + x) / 8;
}

unsigned long buffer_size(uint8_t width, uint8_t height) {
    return sizeof(uint8_t) * (int)ceil(width / 8.0) * ceil(height);
}

uint8_t* malloc_buffer(uint8_t width, uint8_t height) {
    return (uint8_t*)malloc(buffer_size(width, height));
}

Buffer* buffer_create(uint8_t width, uint8_t height, bool double_buffered) {
    Buffer* b = (Buffer*)malloc(sizeof(Buffer));
    b->double_buffered = double_buffered;
    b->width = width;
    b->height = height;
    b->data = malloc_buffer(width, height);
    if(double_buffered)
        b->back_buffer = malloc_buffer(width, height);
    else
        b->back_buffer = NULL;
    return b;
}

void buffer_release(Buffer* buffer) {
    free(buffer->data);
    if(buffer->double_buffered) free(buffer->back_buffer);
    free(buffer);
}

bool buffer_test_coordinate(Buffer* const buffer, int x, int y) {
    return (x >= 0 && x < buffer->width && y >= 0 && y < buffer->height);
}

bool buffer_get_pixel(Buffer* const buffer, int x, int y) {
    return buffer->data[pixel(x, y, buffer->width)] & (1 << (x & 7));
}

void buffer_set_pixel(Buffer* buffer, int16_t x, int16_t y, enum PixelColor draw_mode) {
    uint8_t bit = 1 << (x & 7);
    uint8_t* p = &(buffer->data[pixel(x, y, buffer->width)]);

    switch(draw_mode) {
    case Black:
        *p |= bit;
        break;
    case White:
        *p &= ~bit;
        break;
    case Flip:
        *p ^= bit;
        break;
    }
}

Buffer* buffer_copy(Buffer* buffer) {
    Buffer* new_buffer = (Buffer*)malloc(sizeof(Buffer));
    new_buffer->double_buffered = buffer->double_buffered;
    new_buffer->width = buffer->width;
    new_buffer->height = buffer->height;
    new_buffer->data = malloc_buffer(buffer->width, buffer->height);
    memcpy(new_buffer->data, buffer->data, buffer_size(buffer->width, buffer->height));
    if(buffer->double_buffered) {
        new_buffer->back_buffer = malloc_buffer(buffer->width, buffer->height);
        memcpy(
            new_buffer->back_buffer,
            buffer->back_buffer,
            buffer_size(buffer->width, buffer->height));
    } else {
        new_buffer->back_buffer = NULL;
    }
    return new_buffer;
}

void buffer_swap_back(Buffer* buffer) {
    check_pointer(buffer);
    check_pointer(buffer->data);
    if(buffer->double_buffered) {
        check_pointer(buffer->back_buffer);
        uint8_t* temp = buffer->data;
        buffer->data = buffer->back_buffer;
        buffer->back_buffer = temp;
    }
}

void buffer_clear(Buffer* buffer) {
    check_pointer(buffer);
    check_pointer(buffer->data);
    buffer->data = (uint8_t*)memset(buffer->data, 0, buffer_size(buffer->width, buffer->height));
}

void buffer_swap_with(Buffer* buffer_a, Buffer* buffer_b) {
    check_pointer(buffer_a);
    check_pointer(buffer_b);
    uint8_t* temp = buffer_a->data;
    buffer_a->data = buffer_b->data;
    buffer_b->data = temp;
}

void buffer_draw_internal(
    Buffer* target,
    Buffer* const sprite,
    bool is_black,
    enum PixelColor color,
    Vector* const position,
    uint8_t x_cap,
    uint8_t y_cap,
    float rotation,
    Vector anchor) {
    Vector center = {
        .x = anchor.x * sprite->width,
        .y = anchor.y * sprite->height,
    };
    Vector transform;
    int max_w = fmin(sprite->width, x_cap);
    int max_h = fmin(sprite->height, y_cap);
    bool isOn;
    int16_t finalX, finalY;
    for(int y = 0; y < max_h; y++) {
        for(int x = 0; x < max_w; x++) {
            Vector curr = {x, y};
            vector_sub(&curr, &center, &transform);
            vector_rotate(&transform, rotation, &transform);
            vector_add(&transform, position, &transform);

            finalX = (int16_t)roundf(transform.x);
            finalY = (int16_t)roundf(transform.y);
            if(buffer_test_coordinate(target, finalX, finalY)) {
                isOn = buffer_get_pixel(sprite, x, y) == is_black;
                if(isOn) buffer_set_pixel(target, finalX, finalY, color);
            }
        }
    }
}

void buffer_draw_all(Buffer* target, Buffer* const sprite, Vector* position, float rotation) {
    check_pointer(target);
    check_pointer(sprite);
    check_pointer(position);
    buffer_draw(
        target, sprite, position, sprite->width, sprite->height, rotation, &default_render);
}

void buffer_draw(
    Buffer* target,
    Buffer* const sprite,
    Vector* position,
    uint8_t x_cap,
    uint8_t y_cap,
    float rotation,
    RenderSettings* settings) {
    check_pointer(target);
    check_pointer(sprite);
    check_pointer(position);
    switch(settings->drawMode) {
    default:
    case BlackOnly:
        buffer_draw_internal(
            target, sprite, true, Black, position, x_cap, y_cap, rotation, settings->anchor);
        break;
    case WhiteOnly:
        buffer_draw_internal(
            target, sprite, false, White, position, x_cap, y_cap, rotation, settings->anchor);
        break;
    case WhiteAsBlack:
        buffer_draw_internal(
            target, sprite, false, Black, position, x_cap, y_cap, rotation, settings->anchor);
        break;
    case BlackAsWhite:
        buffer_draw_internal(
            target, sprite, true, White, position, x_cap, y_cap, rotation, settings->anchor);
        break;
    case WhiteAsInverted:
        buffer_draw_internal(
            target, sprite, false, Flip, position, x_cap, y_cap, rotation, settings->anchor);
        break;
    case BlackAsInverted:
        buffer_draw_internal(
            target, sprite, true, Flip, position, x_cap, y_cap, rotation, settings->anchor);
        break;
    }
}

void buffer_render(Buffer* buffer, Canvas* const canvas) {
    check_pointer(buffer);
    canvas_draw_xbm(canvas, 0, 0, buffer->width, buffer->height, buffer->data);
}

void buffer_draw_line(Buffer* buffer, int x0, int y0, int x1, int y1, enum PixelColor draw_mode) {
    check_pointer(buffer);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;

    while(true) {
        if(buffer_test_coordinate(buffer, x0, y0)) {
            buffer_set_pixel(buffer, x0, y0, draw_mode);
        }
        if(x0 == x1 && y0 == y1) break;
        int e2 = err;
        if(e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if(e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}

void buffer_draw_rbox(
    Buffer* buffer,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    enum PixelColor draw_mode) {
    for(int16_t x = x0; x < x1; x++) {
        for(int16_t y = y0; y < y1; y++) {
            if(((x == x0 || x == x1 - 1) && (y == y0 || y == y1 - 1)) ||
               !buffer_test_coordinate(buffer, x, y))
                continue;
            buffer_set_pixel(buffer, x, y, draw_mode);
        }
    }
}

void buffer_draw_rbox_frame(
    Buffer* buffer,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    enum PixelColor draw_mode) {
    buffer_draw_line(buffer, x0 + 1, y0, x1 - 1, y0, draw_mode);
    buffer_draw_line(buffer, x0 + 1, y1, x1 - 1, y1, draw_mode);

    buffer_draw_line(buffer, x0, y0 + 1, x0, y1 - 1, draw_mode);
    buffer_draw_line(buffer, x1, y0 + 1, x1, y1 - 1, draw_mode);
}

void buffer_draw_box(
    Buffer* buffer,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    enum PixelColor draw_mode) {
    for(int16_t x = x0 + 1; x < x1 - 1; x++) {
        for(int16_t y = y0 + 1; y < y1 - 1; y++) {
            if(!buffer_test_coordinate(buffer, x, y)) continue;
            buffer_set_pixel(buffer, x, y, draw_mode);
        }
    }
}

void buffer_set_pixel_with_check(Buffer* buffer, int16_t x, int16_t y, enum PixelColor draw_mode) {
    if(buffer_test_coordinate(buffer, x, y)) buffer_set_pixel(buffer, x, y, draw_mode);
}

void buffer_draw_circle(Buffer* buffer, int x, int y, int r, enum PixelColor color) {
    int16_t a = r;
    int16_t b = 0;
    int16_t decision = 1 - a;

    while(b <= a) {
        buffer_set_pixel_with_check(buffer, a + x, b + y, color);
        buffer_set_pixel_with_check(buffer, b + x, a + y, color);
        buffer_set_pixel_with_check(buffer, -a + x, b + y, color);
        buffer_set_pixel_with_check(buffer, -b + x, a + y, color);
        buffer_set_pixel_with_check(buffer, -a + x, -b + y, color);
        buffer_set_pixel_with_check(buffer, -b + x, -a + y, color);
        buffer_set_pixel_with_check(buffer, a + x, -b + y, color);
        buffer_set_pixel_with_check(buffer, b + x, -a + y, color);

        b++;
        if(decision <= 0) {
            decision += 2 * b + 1;
        } else {
            a--;
            decision += 2 * (b - a) + 1;
        }
    }
}
