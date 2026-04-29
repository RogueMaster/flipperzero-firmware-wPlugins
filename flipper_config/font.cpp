#include "font.h"
#include "lcd.h"
#include "furi.h"

size_t font_string_width(FontSize size, const char *text)
{
    if (!text)
    {
        return 0;
    }
    const size_t length = strlen(text);
    switch (size)
    {
    case FONT_SIZE_SECONDARY:
    case FONT_SIZE_PRIMARY:
    {
        Canvas *canvas = lcd_get_canvas();
        if (canvas)
        {
            return canvas_string_width(canvas, text);
        }
        break;
    }
    case FONT_SIZE_SMALL:
        return length * 4;
    case FONT_SIZE_MEDIUM:
        return length * 5;
    case FONT_SIZE_XLARGE:
        return length * 9;
    default:
        break;
    };

    return length * 6;
}