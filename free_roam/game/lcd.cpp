#include "lcd.hpp"
#include <gui/gui.h>

static Canvas *canvas;

static Color lcd_get_color(uint16_t color)
{
    return (color == 0xFFFF) ? ColorWhite : ColorBlack;
}

static void lcd_swap_float(float *a, float *b)
{
    float t = *a;
    *a = *b;
    *b = t;
}

Canvas *lcd_get_canvas()
{
    return canvas;
}

void lcd_init_canvas(Canvas *c)
{
    canvas = c;
}

void lcd_init()
{
    canvas_reset(canvas);
    canvas_clear(canvas);
}

void lcd_deinit()
{
    canvas = nullptr;
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_draw_dot(canvas, x, y);
    }
}

void lcd_fill(uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_clear(canvas);
    }
}

void lcd_blit(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *buffer)
{
    if (canvas && buffer)
    {
        for (uint16_t j = 0; j < height; j++)
        {
            for (uint16_t i = 0; i < width; i++)
            {
                uint8_t pixel = buffer[j * width + i];
                canvas_set_color(canvas, lcd_get_color(pixel));
                canvas_draw_dot(canvas, x + i, y + j);
            }
        }
    }
}

void lcd_blit_16bit(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *buffer)
{
    if (canvas && buffer)
    {
        for (uint16_t j = 0; j < height; j++)
        {
            for (uint16_t i = 0; i < width; i++)
            {
                uint16_t pixel = buffer[j * width + i];
                canvas_set_color(canvas, lcd_get_color(pixel));
                canvas_draw_dot(canvas, x + i, y + j);
            }
        }
    }
}

void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_draw_line(canvas, x1, y1, x2, y2);
    }
}

void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_draw_frame(canvas, x, y, width, height);
    }
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_draw_box(canvas, x, y, width, height);
    }
}

void lcd_draw_circle(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_draw_circle(canvas, center_x, center_y, radius);
    }
}

void lcd_fill_circle(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        for (int16_t dy = -radius; dy <= radius; dy++)
        {
            for (int16_t dx = -radius; dx <= radius; dx++)
            {
                if (dx * dx + dy * dy <= radius * radius)
                {
                    canvas_draw_dot(canvas, center_x + dx, center_y + dy);
                }
            }
        }
    }
}

void lcd_fill_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
    float p1_x = (float)x1, p1_y = (float)y1;
    float p2_x = (float)x2, p2_y = (float)y2;
    float p3_x = (float)x3, p3_y = (float)y3;

    canvas_set_color(canvas, lcd_get_color(color));

    // Sort vertices by Y (p1.y <= p2.y <= p3.y)
    if (p1_y > p2_y)
    {
        lcd_swap_float(&p1_x, &p2_x);
        lcd_swap_float(&p1_y, &p2_y);
    }
    if (p2_y > p3_y)
    {
        lcd_swap_float(&p2_x, &p3_x);
        lcd_swap_float(&p2_y, &p3_y);
    }
    if (p1_y > p2_y)
    {
        lcd_swap_float(&p1_x, &p2_x);
        lcd_swap_float(&p1_y, &p2_y);
    }

    int iy1 = (int)p1_y, iy2 = (int)p2_y, iy3 = (int)p3_y;
    if (iy1 == iy3)
        return;

    for (int scanY = iy1; scanY <= iy3; scanY++)
    {
        if (scanY < 0 || scanY >= LCD_HEIGHT)
            continue;

        float x_long = p1_x + (p3_x - p1_x) * (scanY - iy1) / (float)(iy3 - iy1);
        float x_short;

        if (scanY <= iy2)
            x_short = (iy2 != iy1)
                          ? p1_x + (p2_x - p1_x) * (scanY - iy1) / (float)(iy2 - iy1)
                          : p1_x;
        else
            x_short = (iy3 != iy2)
                          ? p2_x + (p3_x - p2_x) * (scanY - iy2) / (float)(iy3 - iy2)
                          : p2_x;

        int start_x = (int)(x_long < x_short ? x_long : x_short);
        int end_x = (int)(x_long > x_short ? x_long : x_short);

        canvas_set_color(canvas, lcd_get_color(color));
        canvas_draw_line(canvas, start_x, scanY, end_x, scanY);
    }
}

void lcd_fill_round_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t radius, uint16_t color)
{
    int ix = (int)x, iy = (int)y, iw = (int)width, ih = (int)height, r = (int)radius;

    if (iw <= 0 || ih <= 0 || r <= 0)
        return;

    if (ix < 0)
    {
        iw += ix;
        ix = 0;
    }
    if (iy < 0)
    {
        ih += iy;
        iy = 0;
    }
    if (ix + iw > LCD_WIDTH)
        iw = LCD_WIDTH - ix;
    if (iy + ih > LCD_HEIGHT)
        ih = LCD_HEIGHT - iy;
    if (iw <= 0 || ih <= 0)
        return;

    if (r > iw / 2)
        r = iw / 2;
    if (r > ih / 2)
        r = ih / 2;

    canvas_set_color(canvas, lcd_get_color(color));

    int tl_cx = ix + r, tl_cy = iy + r;
    int tr_cx = ix + iw - r, tr_cy = iy + r;
    int bl_cx = ix + r, bl_cy = iy + ih - r;
    int br_cx = ix + iw - r, br_cy = iy + ih - r;
    int rsq = r * r;

    for (int py = iy; py < iy + ih; py++)
    {
        for (int px = ix; px < ix + iw; px++)
        {
            bool in_corner = false;

            if (px < tl_cx && py < tl_cy)
            {
                int dx = px - tl_cx, dy = py - tl_cy;
                if (dx * dx + dy * dy > rsq)
                    in_corner = true;
            }
            else if (px >= tr_cx && py < tr_cy)
            {
                int dx = px - tr_cx, dy = py - tr_cy;
                if (dx * dx + dy * dy > rsq)
                    in_corner = true;
            }
            else if (px < bl_cx && py >= bl_cy)
            {
                int dx = px - bl_cx, dy = py - bl_cy;
                if (dx * dx + dy * dy > rsq)
                    in_corner = true;
            }
            else if (px >= br_cx && py >= br_cy)
            {
                int dx = px - br_cx, dy = py - br_cy;
                if (dx * dx + dy * dy > rsq)
                    in_corner = true;
            }

            if (!in_corner)
            {
                canvas_draw_dot(canvas, px, py);
            }
        }
    }
}

void lcd_draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_draw_line(canvas, x1, y1, x2, y2);
        canvas_draw_line(canvas, x2, y2, x3, y3);
        canvas_draw_line(canvas, x3, y3, x1, y1);
    }
}

void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, FontSize size)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_set_font_custom(canvas, size);
        canvas_draw_str(canvas, x, y, &c);
    }
}

void lcd_draw_text(uint16_t x, uint16_t y, const char *text, uint16_t color, FontSize size)
{
    if (canvas)
    {
        canvas_set_color(canvas, lcd_get_color(color));
        canvas_set_font_custom(canvas, size);
        canvas_draw_str(canvas, x, y, text);
    }
}