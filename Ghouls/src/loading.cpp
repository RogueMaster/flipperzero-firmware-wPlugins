#include "loading.hpp"
#include TIME_INCLUDE
#define PI 3.14159265358979323846f
Loading::Loading(Draw *draw)
    : draw(draw)
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
        timeStart = TIME_MILLIS;
    }
    drawSpinner();
    draw->setFont(FONT_SIZE_SMALL);
    draw->text(draw->getDisplaySize().x * 44 / 128, draw->getDisplaySize().y * 5 / 64, currentText, 0x0000);
    uint32_t currentTime = TIME_MILLIS;
    if (currentTime >= timeStart)
    {
        timeElapsed = currentTime - timeStart;
    }
    else
    {
        timeElapsed = (UINT32_MAX - timeStart) + currentTime + 1;
    }
    spinnerPosition = (spinnerPosition + 5) % 360; // Rotate by 5 degrees each frame
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
    const int sw = draw->getDisplaySize().x;
    const int sh = draw->getDisplaySize().y;
    int centerX = sw / 2;
    int centerY = sh / 2;
    int radius = sh * 20 / 64; // spinner radius
    int span = 280;            // degrees of arc
    int step = 5;              // degrees between segments

    int startAngle = spinnerPosition;
    // draw only along the circle edge as short line‐segments
    Vector _pos = {0, 0};
    Vector _size = {0, 0};
    for (int offset = 0; offset < span; offset += step)
    {
        int angle = (startAngle + offset) % 360;
        int nextAngle = (angle + step) % 360;
        float rad = PI / 180.0f;

        // compute two successive points on the circumference
        _pos.x = centerX + int(radius * cos(angle * rad));
        _pos.y = centerY + int(radius * sin(angle * rad));
        _size.x = centerX + int(radius * cos(nextAngle * rad));
        _size.y = centerY + int(radius * sin(nextAngle * rad));

        // draw just the edge segment
        draw->line(_pos.x, _pos.y, _size.x, _size.y, 0x0000);
    }

    // draw time elapsed in milliseconds
    draw->setFont(FONT_SIZE_SMALL);
    _pos.x = 0;
    _pos.y = sh * 60 / 64;
    draw->text(_pos, "Time Elapsed:", 0x0000);
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
        draw->text(sw * 90 / 128, sh * 60 / 64, timeStr, 0x0000);
    }
    else
    {
        uint32_t minutes = seconds / 60;
        uint32_t remainingSeconds = seconds % 60;
        snprintf(timeStr, sizeof(timeStr), "%lu:%02lu", (unsigned long)minutes, (unsigned long)remainingSeconds);
        draw->text(sw * 105 / 128, sh * 60 / 64, timeStr, 0x0000);
    }
}
