#pragma once
#include <engine/draw.hpp>

class Loading
{
public:
    Loading(Draw *draw);
    //
    void animate();
    void stop();
    //
    void setText(const char *text) { currentText = text; }
    //
    uint32_t getTimeElapsed() { return timeElapsed; }

private:
    Draw *draw = nullptr;
    void drawSpinner();
    uint32_t spinnerPosition;
    uint32_t timeElapsed;
    uint32_t timeStart;
    bool animating = false;
    const char *currentText = "Loading...";
};
