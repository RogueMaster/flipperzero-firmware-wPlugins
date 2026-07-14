#include <lib/Arduboy2.h>
#include "PrinceOfArabia_CutScene.h"

void setTitleFrame(TitleFrameIndex index) {
#if defined(POP_OR_POA)
    uint8_t idx = 2 * static_cast<uint8_t>(index) + (cookie.pop & 1);
#elif defined(POP_ONLY)
    uint8_t idx = 2 * static_cast<uint8_t>(index) + 1;
#elif defined(POA_ONLY)
    uint8_t idx = 2 * static_cast<uint8_t>(index);
#endif

    // TitleFrameIndexTable layout is fixed: uint24_t addr (3 bytes) + uint8_t frame_count.
    // Do not use sizeof(uint24_t) here because in this port uint24_t is represented as uint32_t.
    FX::seekDataArray(TitleFrameIndexTable, idx, 0, 4);
    uint32_t data = FX::readPendingLastUInt32();
    FX::setFrame((uint24_t)(data >> 8), (uint8_t)data);
}

void title_Init() {
    uint8_t frameIndex = 0;

    if(cookie.hasSavedScore) frameIndex = frameIndex + 1;
    if(cookie.hasSavedLevel) frameIndex = frameIndex + 2;

    gamePlay.gameState = GameState::Title;
    cookie.setMode(TitleScreenMode::Intro);
    titleScreenVars.reset(frameIndex);
}

// ----------------------------------------------------------------------------
//  Handle state updates ..
//
void title() {
    auto justPressed = arduboy.justPressedButtons();

    uint8_t frameIndex = 0;

    if(cookie.hasSavedScore) frameIndex = frameIndex + 1;
    if(cookie.hasSavedLevel) frameIndex = frameIndex + 2;

#ifdef POP_OR_POA
    auto pressed = arduboy.pressedButtons();
#endif

#ifdef DEBUG_CUT_SCENES

    if(justPressed & B_BUTTON && cookie.getMode() != TitleScreenMode::CutScene_7_PlayGame) {
        //setRenderChamberBG();
        cookie.setMode(TitleScreenMode::CutScene_7_Transition);

        Invader_General& general = level.getItem(Constants::Invaders_General).data.invader_General;
        general.y = 0;
    }

#endif

    if(cookie.getMode() == TitleScreenMode::Main) {
#ifdef DEBUG_LEVELS

        auto pressed = arduboy.pressedButtons();

        if((justPressed & UP_BUTTON) && (pressed & A_BUTTON) && (startLevel < MAX_LEVEL)) {
            startLevel++;
        }

        if((justPressed & DOWN_BUTTON) && (pressed & A_BUTTON) && (startLevel > 1)) {
            startLevel--;
        }

#endif

        // Switch between POP and POA ..

#ifdef POP_OR_POA
        if(pressed & (LEFT_BUTTON | RIGHT_BUTTON)) {
            titleScreenVars.counter++;

            if(titleScreenVars.counter == 96) {
                cookie.pop = !cookie.pop;

                FX::saveGameState((uint8_t*)&cookie, sizeof(cookie));
            }

            pressed = 0;

        } else {
            titleScreenVars.counter = 0;
        }
#endif

        // Move cursor ..

        FX::seekDataArray(Title_Select_Option, frameIndex, (uint8_t)titleScreenVars.option * 2, 8);
        uint16_t options = FX::readPendingLastUInt16();

        if(justPressed & LEFT_BUTTON) {
            titleScreenVars.option = (TitleScreenOptions)(options >> 8);
        }

        if(justPressed & RIGHT_BUTTON) {
            titleScreenVars.option = (TitleScreenOptions)(options);
        }

        if(justPressed & B_BUTTON) {
            arduboy.audio.toggle();
        }
    }

#ifdef DEBUG_CUT_SCENES
    if(justPressed & (A_BUTTON)) {
#else
    if((justPressed & A_BUTTON) || (justPressed & B_BUTTON)) {
#endif

        switch(cookie.getMode()) {
        case TitleScreenMode::Main:

            if(justPressed & B_BUTTON) break; // B button controls sound on main
            switch(titleScreenVars.option) {
#ifdef SAVE_MEMORY_OTHER
            case TitleScreenOptions::Play:

                gamePlay.gameState = GameState::Game_Init;

                break;

#else

            case TitleScreenOptions::Play:

#ifndef SAVE_MEMORY_SOUND
                setSound(SoundIndex::Seque);
#endif

                prince.setHealth(3);
                prince.setHealthMax(3);

                cookie.setMode(TitleScreenMode::IntroGame_1A);
                setTitleFrame(TitleFrameIndex::IntroGame_1A_Frame);

                break;

            case TitleScreenOptions::Resume:

                if(!FX::loadGameState(cookie)) break;
                restoreRuntimeAfterLoad();

                gamePlay.gameState = GameState::Game;
                fadeEffect.reset();
                titleScreenVars.counter = 16;
                menu.init();

#ifndef SAVE_MEMORY_SOUND
                sound.noTone();
#endif

                return;

            case TitleScreenOptions::Credits:

                cookie.setMode(TitleScreenMode::Credits);
                setTitleFrame(TitleFrameIndex::Credits_PoP);

                break;

            case TitleScreenOptions::High:

                cookie.setMode(TitleScreenMode::High);
                setTitleFrame(TitleFrameIndex::High_PoP_Frame);

                break;

#endif

            default:
                break;
            }

            break;

#ifndef SAVE_MEMORY_OTHER

        case TitleScreenMode::Credits:

#ifdef DEBUG_CUT_SCENES
            if(justPressed & (A_BUTTON)) {
#else
            if(justPressed & (A_BUTTON | B_BUTTON)) {
#endif

#ifndef SAVE_MEMORY_SOUND
                setSound(SoundIndex::Theme);
#endif
            }

            [[fallthrough]];

        case TitleScreenMode::High:
        case TitleScreenMode::TimeOut:

#ifdef DEBUG_CUT_SCENES
            if(justPressed & (A_BUTTON)) {
#else
            if(justPressed & (A_BUTTON | B_BUTTON)) {
#endif

                cookie.setMode(TitleScreenMode::Main);
                setTitleFrame(
                    (TitleFrameIndex)((uint8_t)TitleFrameIndex::Main_PoP_Frame_NC + frameIndex));

#ifndef SAVE_MEMORY_SOUND
                setSound(SoundIndex::Theme);
#endif
            }

            break;

        case TitleScreenMode::IntroGame_1A:

#ifndef SAVE_MEMORY_SOUND
            setSound(SoundIndex::Seque);
#endif

            cookie.setMode(TitleScreenMode::IntroGame_1B);
            gamePlay.gameState = GameState::Game_Init;

            break;

        case TitleScreenMode::CutScene_1:

            cookie.setMode(TitleScreenMode::IntroGame_1B);
            gamePlay.gameState = GameState::Game_Init;

            break;

        case TitleScreenMode::IntroGame_1B:

            gamePlay.gameState = GameState::Game_Init;
            break;

        case TitleScreenMode::CutScene_2:
        case TitleScreenMode::CutScene_3:
        case TitleScreenMode::CutScene_4:
        case TitleScreenMode::CutScene_5:
        case TitleScreenMode::CutScene_6:
        case TitleScreenMode::CutScene_7_Hint:
        case TitleScreenMode::CutScene_8:

            gamePlay.gameState = GameState::Game_StartLevel;
            break;

#ifndef SAVE_MEMORY_INVADER

        case TitleScreenMode::CutScene_7_PlayGame:

            if(justPressed & (A_BUTTON)) {
                cookie.setMode(TitleScreenMode::CutScene_7_PlayGame);
                gamePlay.gameState = GameState::Game_StartLevel;
                arduboy.setFrameRate(Constants::FrameRate);
            }

            break;

#endif

            //case TitleScreenMode::CutScene_End:
            //
            //    gamePlay.gameState = GameState::Title_Init;
            //    break;

#endif

        default:
            break;
        }
    }

    // Render ..

    switch(cookie.getMode()) {
    case TitleScreenMode::Intro:

        if(!FX::drawFrame()) {
            setTitleFrame(
                (TitleFrameIndex)((uint8_t)TitleFrameIndex::Intro_Last_PoP_Frame_NC + frameIndex));

            if(justPressed) {
                cookie.setMode(TitleScreenMode::Main);
                setTitleFrame(
                    (TitleFrameIndex)((uint8_t)TitleFrameIndex::Main_PoP_Frame_NC + frameIndex));
            }
        }
        break;

    case TitleScreenMode::Main:

        if(!FX::drawFrame()) {
            setTitleFrame(
                (TitleFrameIndex)((uint8_t)TitleFrameIndex::Main_Game_PoP_Frame_NC + frameIndex));
            FX::seekDataArray(Title_Cursor_XPos, frameIndex, (uint8_t)titleScreenVars.option, 4);
            uint8_t x = FX::readEnd();

            FX::drawBitmap(x, 57, Images::Title_Cursor, 0, dbmNormal);
            FX::drawBitmap(120, 56, Images::Title_SoundIcon, arduboy.audio.enabled(), dbmNormal);
        }

#ifdef DEBUG_LEVELS

        FX::drawBitmap(116, 0, Images::LevelRect, 0, dbmNormal);
        renderNumber_Upright(118, 2, startLevel);

#endif

        break;

#ifndef SAVE_MEMORY_OTHER

    case TitleScreenMode::Credits:
    case TitleScreenMode::TimeOut:

        FX::drawFrame();
        break;

    case TitleScreenMode::High:

        FX::drawFrame();
        FX::drawBitmap(38, 38, Images::Numbers_Large, cookie.highMin, dbmNormal);
        FX::drawBitmap(68, 38, Images::Numbers_Large, cookie.highSec, dbmNormal);

        renderNumber_Upright(70, 59, cookie.highSaves / 100);
        renderNumber_Upright(78, 59, cookie.highSaves % 100);

        break;

    case TitleScreenMode::IntroGame_1A:

        if(!FX::drawFrame()) {
            cookie.setMode(TitleScreenMode::CutScene_1);
            setTitleFrame(TitleFrameIndex::CutScene_1_Frame);
        }

        break;

    case TitleScreenMode::CutScene_1:

        if(!FX::drawFrame()) {
            cookie.setMode(TitleScreenMode::IntroGame_1B);
            setTitleFrame(TitleFrameIndex::IntroGame_1B_Frame);
        }

        break;

    case TitleScreenMode::IntroGame_1B:

        if(!FX::drawFrame()) {
            gamePlay.gameState = GameState::Game_Init;
        }

        break;

    case TitleScreenMode::CutScene_2:
    case TitleScreenMode::CutScene_3:
    case TitleScreenMode::CutScene_4:
    case TitleScreenMode::CutScene_5:
    case TitleScreenMode::CutScene_6:
    case TitleScreenMode::CutScene_7_Hint:
    case TitleScreenMode::CutScene_8:

        if(!FX::drawFrame()) {
            gamePlay.gameState = GameState::Game_StartLevel;
        }

        break;

    case TitleScreenMode::CutScene_End:

        if(!FX::drawFrame()) {
            cookie.setMode(TitleScreenMode::IntroGame_End);
            setTitleFrame(TitleFrameIndex::IntroGame_End_PoP_Frame);

#ifndef SAVE_MEMORY_SOUND
            setSound(SoundIndex::Ending);
#endif
        }

        break;

    case TitleScreenMode::IntroGame_End:

        if(!FX::drawFrame()) {
            gamePlay.gameState = GameState::Title_Init;
        }

        break;

#ifndef SAVE_MEMORY_INVADER

    case TitleScreenMode::CutScene_7_Transition:

        if(!FX::drawFrame()) {
            level.loadItems(0, prince);
            cookie.setMode(TitleScreenMode::CutScene_7_PlayGame);
            arduboy.setFrameCount(5);
            arduboy.setFrameRate(60);
        }

        break;

    case TitleScreenMode::CutScene_7_PlayGame:

        invader_PlayGame();
        break;

#endif

#endif

    default:
        break;
    }
}
