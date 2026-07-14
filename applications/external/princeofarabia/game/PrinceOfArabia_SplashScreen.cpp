#include <lib/Arduboy2.h>

// ----------------------------------------------------------------------------
//  Initialise state ..
//
void splashScreen_Init() {
    gamePlay.gameState = GameState::SplashScreen;
    FX::setFrame(splashScreen_Frame, 12 - 1);
}

// ----------------------------------------------------------------------------
//  Handle state updates ..
//
void splashScreen() {
#ifdef POP_OR_POA

    auto pressed = arduboy.pressedButtons();

    if(titleScreenVars.counter < 16) {
        titleScreenVars.counter++;
    }

    else if(pressed > 0) {
        gamePlay.gameState = GameState::Title_Init;
    }

#else

    auto justPressed = arduboy.justPressedButtons();

    if(justPressed > 0) {
        gamePlay.gameState = GameState::Title_Init;
    }

#endif

    FX::drawFrame();
}
