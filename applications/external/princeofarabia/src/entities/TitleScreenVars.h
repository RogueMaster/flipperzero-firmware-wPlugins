#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"
#include "../../fxdata/fxdata.h"
#include "Level.h"

extern void setTitleFrame(TitleFrameIndex index /*, uint8_t frame = 0*/);

struct TitleScreenVars {
    uint8_t counter;

public:
    TitleScreenOptions option;

    void reset(uint8_t frameIndex) {
        //this->option = TitleScreenOptions::Play;
        this->option = (TitleScreenOptions)(frameIndex >> 1);
        setTitleFrame(
            (TitleFrameIndex)((uint8_t)TitleFrameIndex::Intro_PoP_Frame_NC + frameIndex));
    }
};
