#pragma once

#include "../utils/Arduboy2Ext.h"
#include "../utils/Constants.h"
#include "GamePlay.h"
#include "Level.h"
#include "Prince.h"
#include "Enemy.h"
#include "TitleScreenVars.h"

extern void setTitleFrame(TitleFrameIndex index /*, uint8_t frame = 0*/);

struct Cookie {
    bool hasSavedLevel;
    bool hasSavedScore;

#ifdef POP_OR_POA
    bool pop = false;
#endif

    uint8_t highMin;
    uint8_t highSec;
    uint16_t highSaves;

    GamePlay gamePlay;
    Level level;
    Prince prince;

#ifndef SAVE_MEMORY_ENEMY
    Enemy enemy;
#endif

    TitleScreenMode mode;

    TitleScreenMode getMode() {
        return this->mode;
    }

    void setMode(TitleScreenMode mode) {
        this->mode = mode;

        switch(this->mode) {
        case TitleScreenMode::CutScene_2:

            setTitleFrame(TitleFrameIndex::CutScene_2_Frame);
            break;

        case TitleScreenMode::CutScene_3:

            setTitleFrame(TitleFrameIndex::CutScene_3_Frame);
            break;

        case TitleScreenMode::CutScene_4:

            setTitleFrame(TitleFrameIndex::CutScene_4_Frame);
            break;

        case TitleScreenMode::CutScene_5:

            setTitleFrame(TitleFrameIndex::CutScene_5_Frame);
            break;

        case TitleScreenMode::CutScene_6:

            setTitleFrame(TitleFrameIndex::CutScene_6_Frame);
            break;

        case TitleScreenMode::CutScene_7_Hint:

            setTitleFrame(TitleFrameIndex::CutScene_2B_Frame);
            break;

#ifndef SAVE_MEMORY_INVADER
        case TitleScreenMode::CutScene_7_Transition:

            setTitleFrame(TitleFrameIndex::CutScene_7_Frame);
            break;
#endif

        case TitleScreenMode::CutScene_8:

            setTitleFrame(TitleFrameIndex::CutScene_8_Frame);
            break;

        case TitleScreenMode::CutScene_End:

            setTitleFrame(TitleFrameIndex::CutScene_End_Frame);
            break;

        default:
            break;
        }
    }
};
