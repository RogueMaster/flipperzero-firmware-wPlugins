#pragma once
#include <memory>
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "engine/engine.hpp"
#include "game/general.hpp"
#include "game/player.hpp"

class FreeRoamApp;

class FreeRoamGame
{
private:
    int currentLevelIndex = 0;              // Current level index
    std::unique_ptr<Draw> draw;             // Draw instance
    std::unique_ptr<GameEngine> engine;     // Engine instance
    bool isGameRunning = false;             // Flag to check if the game is running
    InputKey lastInput = InputKeyMAX;       // Last input key pressed
    std::unique_ptr<Player> player;         // Player instance
    ToggleState soundToggle = ToggleOn;     // sound toggle state
    const int totalLevels = 4;              // Total number of levels available
    ToggleState vibrationToggle = ToggleOn; // vibration toggle state
    //
    int atoi(const char *nptr) { return (int)strtol(nptr, NULL, 10); } // convert string to integer
    void inputManager();                                               // manage input for the game, called from updateInput
    void switchToNextLevel();                                          // switch to the next level in the game
    void switchToLevel(int levelIndex);                                // switch to a specific level by index

public:
    FreeRoamGame();
    ~FreeRoamGame();

    void *appContext = nullptr;      // Pointer to the application context for accessing app-specific functionality
    bool shouldReturnToMenu = false; // Flag to signal return to menu
    //
    void endGame();                                               // end the game and return to the submenu
    InputKey getCurrentInput() const { return lastInput; }        // Get the last input key pressed
    GameEngine *getEngine() const { return engine.get(); }        // Get the game engine instance
    Draw *getDraw() const { return draw.get(); }                  // Get the Draw instance
    bool init(void *appContext);                                  // initialize the game
    bool isActive() const { return shouldReturnToMenu == false; } // Check if the game is active
    bool isRunning() const { return isGameRunning; }              // Check if the game engine is running
    void resetInput() { lastInput = InputKeyMAX; }                // Reset input after processing
    bool startGame();                                             // start the actual game
    bool startGameOnline();                                       // start the online multiplayer game
    void updateDraw(Canvas *canvas);                              // update and draw the game
    void updateInput(InputEvent *event);                          // update input for the game
    void updateSoundToggle();                                     // update sound toggle state
    void updateVibrationToggle();                                 // update vibration toggle state
};
