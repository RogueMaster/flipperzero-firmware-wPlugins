#pragma once
#include "vector.hpp"
#include "engine_config.hpp"

#include ENGINE_FONT_INCLUDE
#include ENGINE_MEM_INCLUDE

class Draw
{
private:
    ENGINE_FONT_SIZE currentFontSize;
    uint16_t currentTextColor;

public:
    Draw();                                                                                                             // Constructor.
    ~Draw();                                                                                                            // Destructor.
    void circle(Vector position, int16_t r, uint16_t color = 0x0000);                                                   // Draws a circle on the display at the specified position with the specified radius and color.
    void circle(int16_t x, int16_t y, int16_t r, uint16_t color = 0x0000);                                              // Draws a circle on the display at the specified position with the specified radius and color.
    void fillCircle(Vector position, int16_t r, uint16_t color = 0x0000);                                               // Fills a circle on the display at the specified position with the specified radius and color.
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color = 0x0000);                                          // Fills a circle on the display at the specified position with the specified radius and color.
    void fillRectangle(Vector position, Vector size, uint16_t color = 0x0000);                                          // Fills a rectangle on the display at the specified position and size with the specified color.
    void fillRectangle(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color = 0x0000);                   // Fills a rectangle on the display at the specified position and size with the specified color.
    void fillScreen(uint16_t color = 0xFFFF);                                                                           // Fills the entire screen with the specified color.
    void fillTriangle(Vector p1, Vector p2, Vector p3, uint16_t color = 0x0000);                                        // Fills a triangle defined by three vertices with the specified color.
    void fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint16_t color = 0x0000); // Fills a triangle defined by three vertices with the specified color.
    uint16_t getCurrentTextColor() const noexcept { return currentTextColor; }                                          // Returns the current drawing color.
    Vector getDisplaySize() const noexcept;                                                                             // Returns the size of the display.
    ENGINE_FONT_SIZE getCurrentFontSize() const noexcept { return currentFontSize; }                                    // Returns the current font size.
    void image(Vector position, const uint8_t *bitmap, Vector size);                                                    // Draws a bitmap on the display at the specified position.
    void image(Vector position, const uint16_t *bitmap, Vector size);                                                   // Draws a bitmap on the display at the specified position.
    void image(int16_t x, int16_t y, const uint8_t *bitmap, int16_t width, int16_t height);                             // Draws a bitmap on the display at the specified position.
    void image(int16_t x, int16_t y, const uint16_t *bitmap, int16_t width, int16_t height);                            // Draws a bitmap on the display at the specified position.
    void line(Vector position, Vector size, uint16_t color = 0x0000);                                                   // Draws a line on the display at the specified position and size with the specified color.
    void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color = 0x0000);                                 // Draws a line on the display at the specified position and size with the specified color.
    void pixel(Vector position, uint16_t color = 0x0000);                                                               // Draws a pixel on the display at the specified position with the specified color.
    void pixel(int16_t x, int16_t y, uint16_t color = 0x0000);                                                          // Draws a pixel on the display at the specified position with the specified color.
    void rectangle(Vector position, Vector size, uint16_t color = 0x0000);                                              // Draws a rectangle on the display at the specified position and size with the specified color.
    void rectangle(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color = 0x0000);                       // Draws a rectangle on the display at the specified position and size with the specified color.
    void setColor(uint16_t color = 0x0000);                                                                             // Sets the color for drawing.
    void setFont(ENGINE_FONT_SIZE size = ENGINE_FONT_DEFAULT);                                                          // Sets the font size for text rendering.
    void swap();                                                                                                        // Swaps the display buffer to show the drawn content .
    void text(Vector position, const char *text);                                                                       // Draws text on the display at the specified position with the current font and color.
    void text(int16_t x, int16_t y, const char *text);                                                                  // Draws text on the display at the specified position with the current font and color.
    void text(Vector position, const char *text, uint16_t color, ENGINE_FONT_SIZE font = ENGINE_FONT_DEFAULT);          // Draws text on the display at the specified position with the specified font.
    void text(int16_t x, int16_t y, const char *text, uint16_t color, ENGINE_FONT_SIZE font = ENGINE_FONT_DEFAULT);     // Draws text on the display at the specified position with the specified font.
    void triangle(Vector p1, Vector p2, Vector p3, uint16_t color = 0x0000);                                            // Draws a triangle defined by three vertices with the specified color.
    void triangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint16_t color = 0x0000);     // Draws a triangle defined by three vertices with the specified color.
};