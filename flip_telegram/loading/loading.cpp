#include "loading/loading.hpp"
#include "font/font.h"
#define millis() furi_get_tick() * 10
#define PI 3.14159265358979323846f
Loading::Loading(Canvas *canvas)
    : canvas(canvas)
{
    spinnerPosition = 0;
    timeElapsed = 0;
    timeStart = 0;
    animating = false;
}
void Loading::animate()
{
    if (!animating)
    {
        animating = true;
        timeStart = millis();
    }
    drawSpinner();
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, 44, 5, currentText);
    timeElapsed = millis() - timeStart;
    spinnerPosition = (spinnerPosition + 10) % 360; // Rotate by 10 degrees each frame
}

void Loading::stop()
{
    animating = false;
    timeElapsed = 0;
    timeStart = 0;
}

void Loading::drawSpinner()
{
    // Get the screen dimensions for positioning
    int centerX = 64;
    int centerY = 32;
    int radius = 20; // spinner radius
    int span = 280;  // degrees of arc
    int step = 5;    // degrees between segments

    int startAngle = spinnerPosition;
    // draw only along the circle edge as short line‐segments
    for (int offset = 0; offset < span; offset += step)
    {
        int angle = (startAngle + offset) % 360;
        int nextAngle = (angle + step) % 360;
        float rad = PI / 180.0f;

        // compute two successive points on the circumference
        int x1 = centerX + int(radius * cos(angle * rad));
        int y1 = centerY + int(radius * sin(angle * rad));
        int x2 = centerX + int(radius * cos(nextAngle * rad));
        int y2 = centerY + int(radius * sin(nextAngle * rad));

        // draw just the edge segment
        canvas_draw_line(canvas, x1, y1, x2, y2);
    }

    // draw time elapsed in milliseconds
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, 0, 60, "Time Elapsed:");
    char timeStr[16];
    int seconds = timeElapsed / 10000;
    if (seconds < 60)
    {
        if (seconds <= 1)
        {
            snprintf(timeStr, sizeof(timeStr), "%u second", seconds);
        }
        else
        {
            snprintf(timeStr, sizeof(timeStr), "%u seconds", seconds);
        }
    }
    else
    {
        snprintf(timeStr, sizeof(timeStr), "%u minutes", seconds / 60);
    }
    canvas_draw_str(canvas, 90, 60, timeStr);
}