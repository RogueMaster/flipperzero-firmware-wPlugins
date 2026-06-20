#include "run/loading.hpp"
#include "font/font.h"
#define millis() furi_get_tick() * 10
#define PI       3.14159265358979323846f
Loading::Loading(Draw* draw)
    : draw(draw) {
    spinnerPosition = 0;
    timeElapsed = 0;
    timeStart = 0;
    animating = false;
}
void Loading::animate() {
    if(!animating) {
        animating = true;
        timeStart = millis();
    }
    drawSpinner();
    draw->setFont(FONT_SIZE_SMALL);
    draw->text(Vector(44, 5), currentText, ColorBlack);
    uint32_t currentTime = millis();
    if(currentTime >= timeStart) {
        timeElapsed = currentTime - timeStart;
    } else {
        timeElapsed = (UINT32_MAX - timeStart) + currentTime + 1;
    }
    spinnerPosition = (spinnerPosition + 10) % 360; // Rotate by 10 degrees each frame
}

void Loading::stop() {
    animating = false;
    timeElapsed = 0;
    timeStart = 0;
}

void Loading::drawSpinner() {
    // Get the screen dimensions for positioning
    int centerX = 64;
    int centerY = 32;
    int radius = 20; // spinner radius
    int span = 280; // degrees of arc
    int step = 5; // degrees between segments

    int startAngle = spinnerPosition;
    // draw only along the circle edge as short line‐segments
    for(int offset = 0; offset < span; offset += step) {
        int angle = (startAngle + offset) % 360;
        int nextAngle = (angle + step) % 360;
        float rad = PI / 180.0f;

        // compute two successive points on the circumference
        int x1 = centerX + int(radius * cos(angle * rad));
        int y1 = centerY + int(radius * sin(angle * rad));
        int x2 = centerX + int(radius * cos(nextAngle * rad));
        int y2 = centerY + int(radius * sin(nextAngle * rad));

        // draw just the edge segment
        draw->line(Vector(x1, y1), Vector(x2, y2), ColorBlack);
    }

    // draw time elapsed in milliseconds
    draw->setFont(FONT_SIZE_SMALL);
    draw->text(Vector(0, 60), "Time Elapsed:", ColorBlack);
    char timeStr[16];
    int seconds = timeElapsed / 10000;
    if(seconds < 60) {
        if(seconds <= 1) {
            snprintf(timeStr, sizeof(timeStr), "%u second", seconds);
        } else {
            snprintf(timeStr, sizeof(timeStr), "%u seconds", seconds);
        }
        draw->text(Vector(90, 60), timeStr, ColorBlack);
    } else {
        uint32_t minutes = seconds / 60;
        uint32_t remainingSeconds = seconds % 60;
        snprintf(
            timeStr,
            sizeof(timeStr),
            "%lu:%02lu",
            (unsigned long)minutes,
            (unsigned long)remainingSeconds);
        draw->text(Vector(105, 60), timeStr, ColorBlack);
    }
}
