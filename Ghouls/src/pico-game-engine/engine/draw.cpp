#include "draw.hpp"
#include ENGINE_LCD_INCLUDE

Draw::Draw() : currentFontSize(ENGINE_FONT_DEFAULT), currentTextColor(0x0000)
{
#ifdef ENGINE_LCD_INIT
    ENGINE_LCD_INIT();
#endif
}

Draw::~Draw()
{
#ifdef ENGINE_LCD_DEINIT
    ENGINE_LCD_DEINIT();
#endif
}

void Draw::circle(Vector position, int16_t r, uint16_t color)
{
    ENGINE_LCD_CIRCLE(position.x, position.y, r, color);
}

void Draw::circle(int16_t x, int16_t y, int16_t r, uint16_t color)
{
    ENGINE_LCD_CIRCLE(x, y, r, color);
}

void Draw::fillCircle(Vector position, int16_t r, uint16_t color)
{
    ENGINE_LCD_FILL_CIRCLE(position.x, position.y, r, color);
}

void Draw::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color)
{
    ENGINE_LCD_FILL_CIRCLE(x, y, r, color);
}

void Draw::fillRectangle(Vector position, Vector size, uint16_t color)
{
    ENGINE_LCD_FILL_RECTANGLE(position.x, position.y, size.x, size.y, color);
}

void Draw::fillRectangle(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color)
{
    ENGINE_LCD_FILL_RECTANGLE(x, y, width, height, color);
}

Vector Draw::getDisplaySize() const noexcept
{
    return Vector(ENGINE_LCD_WIDTH, ENGINE_LCD_HEIGHT);
}

void Draw::fillScreen(uint16_t color)
{
    ENGINE_LCD_CLEAR(color);
}

void Draw::fillTriangle(Vector p1, Vector p2, Vector p3, uint16_t color)
{
    ENGINE_LCD_FILL_TRIANGLE(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, color);
}

void Draw::fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint16_t color)
{
    ENGINE_LCD_FILL_TRIANGLE(x1, y1, x2, y2, x3, y3, color);
}

void Draw::image(Vector position, const uint8_t *bitmap, Vector size)
{
    ENGINE_LCD_BLIT(position.x, position.y, size.x, size.y, bitmap);
}

void Draw::image(Vector position, const uint16_t *bitmap, Vector size)
{
    ENGINE_LCD_BLIT_16BIT(position.x, position.y, size.x, size.y, bitmap);
}

void Draw::image(int16_t x, int16_t y, const uint8_t *bitmap, int16_t width, int16_t height)
{
    ENGINE_LCD_BLIT(x, y, width, height, bitmap);
}

void Draw::image(int16_t x, int16_t y, const uint16_t *bitmap, int16_t width, int16_t height)
{
    ENGINE_LCD_BLIT_16BIT(x, y, width, height, bitmap);
}

void Draw::line(Vector position, Vector size, uint16_t color)
{
    ENGINE_LCD_LINE(position.x, position.y, size.x, size.y, color);
}

void Draw::line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    ENGINE_LCD_LINE(x1, y1, x2, y2, color);
}

void Draw::pixel(Vector position, uint16_t color)
{
    ENGINE_LCD_PIXEL(position.x, position.y, color);
}

void Draw::pixel(int16_t x, int16_t y, uint16_t color)
{
    ENGINE_LCD_PIXEL(x, y, color);
}

void Draw::rectangle(Vector position, Vector size, uint16_t color)
{
    ENGINE_LCD_RECTANGLE(position.x, position.y, size.x, size.y, color);
}

void Draw::rectangle(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color)
{
    ENGINE_LCD_RECTANGLE(x, y, width, height, color);
}

void Draw::setColor(uint16_t color)
{
    currentTextColor = color;
}

void Draw::setFont(ENGINE_FONT_SIZE size)
{
    currentFontSize = size;
}

void Draw::swap()
{
#ifdef ENGINE_LCD_SWAP
    ENGINE_LCD_SWAP();
#endif
}

void Draw::text(Vector position, const char *text)
{
    ENGINE_LCD_TEXT(position.x, position.y, text, currentTextColor, currentFontSize);
}

void Draw::text(int16_t x, int16_t y, const char *text)
{
    ENGINE_LCD_TEXT(x, y, text, currentTextColor, currentFontSize);
}

void Draw::text(Vector position, const char *text, uint16_t color, ENGINE_FONT_SIZE font)
{
    ENGINE_LCD_TEXT(position.x, position.y, text, color, font);
}

void Draw::text(int16_t x, int16_t y, const char *text, uint16_t color, ENGINE_FONT_SIZE font)
{
    ENGINE_LCD_TEXT(x, y, text, color, font);
}

void Draw::triangle(Vector p1, Vector p2, Vector p3, uint16_t color)
{
    ENGINE_LCD_TRIANGLE(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, color);
}

void Draw::triangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint16_t color)
{
    ENGINE_LCD_TRIANGLE(x1, y1, x2, y2, x3, y3, color);
}