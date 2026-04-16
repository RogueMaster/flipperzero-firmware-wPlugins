#pragma once
#include "config.hpp"
#include "pico-game-engine/engine/draw.hpp"

class Loading
{
public:
    Loading(Draw *draw);
    //
    void animate(); // advances the animation by one frame
    void stop();    // stops the animation and resets it to the initial state
    //
    void setText(const char *text) { currentText = text; } // sets the text to be displayed below the spinner
    //
    uint32_t getTimeElapsed() { return timeElapsed; } // returns the time elapsed since the animation started in milliseconds

private:
    Draw *draw = nullptr;
    void drawSpinner();
    uint32_t spinnerPosition;
    uint32_t timeElapsed;
    uint32_t timeStart;
    bool animating = false;
    const char *currentText = "Loading...";
};
