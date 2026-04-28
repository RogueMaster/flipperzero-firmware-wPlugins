#pragma once
#include "config.hpp"
#include "pico-game-engine/engine/engine.hpp"
#include "general.hpp"
#include "player.hpp"
#include "time.hpp"
#include "sound.hpp"

#if SKY_RENDER_ALLOWED
#include "sky.hpp"
#endif

#if GROUND_RENDER_ALLOWED
#include "ground.hpp"
#endif

class GhoulsLevel; // forward declaration

class GhoulsGame
{
private:
    uint16_t currentRound = 1;                                         // Current round number
    bool dayJustSwitched = true;                                       // Flag to track if day/night just switched
    Draw *draw = nullptr;                                              // Draw instance
    GameEngine *engine = nullptr;                                      // Engine instance
    Sound *gameSound = nullptr;                                        // Sound instance
    Time *gameTime = nullptr;                                          // Game time instance
    uint16_t ghoulCountCurrent = 0;                                    // current number of ghouls in the level
    uint16_t ghoulCountSpawned = 0;                                    // number of ghouls spawned so far for the current night
    uint16_t ghoulCountTotal = 0;                                      // total number of ghouls for the current night
    bool isGameRunning = false;                                        // Flag to check if the game is running
    int lastInput = -1;                                                // Last input key pressed
    Player *player = nullptr;                                          // Player instance
    bool shouldExit = false;                                           // Flag to signal exit the game
    int atoi(const char *nptr) { return (int)strtol(nptr, NULL, 10); } // convert string to integer
    Vector getRandomGhoulPosition(Level *level);                       // get a random position for spawning ghouls
    Vector getRandomWeaponPosition(Level *level);                      // get a random position for spawning weapons
    WeaponType getUniqueWeaponType(Level *level);                      // get a unique weapon type (only two of each type allowed)
    void increaseDifficulty();                                         // increase game difficulty by increasing enemy spawn rates/stats
    void inputManager();                                               // manage input for the game, called from updateInput
    void makeGhoulsGoHome();                                           // make ghouls return to spawn
    void makeGhoulsGoToPlayer();                                       // make ghouls target the player
    bool positionExistsInLevel(Level *level, Vector position);         // check if a position is already occupied by an entity in the level
    void refreshPlayer();                                              // refresh player state (e.g., health and weapon displays) after day/night switch
    bool removeGhoulsFromLevel();                                      // remove all ghouls from the level

#if GROUND_RENDER_ALLOWED
    bool setGroundType(GroundType groundType); // set the ground type for the current level
#endif

#if SKY_RENDER_ALLOWED
    bool setSkyType(SkyType skyType); // set the sky instance for day/night cycle
#endif

    bool spawnGhouls(uint8_t count);     // Spawn ghouls into the current level for the current round
    bool spawnOneGhoul();                // Spawn a single ghoul and update counters
    GhoulsLevel *spawnLevel(Game *game); // spawn a new level based on index
    bool spawnWeapons(Level *level);     // spawn all the weapons into the level

public:
    //
    GhoulsGame(const char *username, const char *password, bool soundEnabled = true);
    ~GhoulsGame();
    //
    Time *getGameTime() const { return gameTime; }        // Get the game time instance
    void endGame();                                       // end the game and return to the submenu
    int getCurrentInput() const { return lastInput; }     // Get the last input key pressed
    GhoulsLevel *getCurrentLevel() const;                 // Get the current level
    GameEngine *getEngine() const { return engine; }      // Get the game engine instance
    Draw *getDraw() const { return draw; }                // Get the Draw instance
    Game *getGame() const;                                // Get the Game instance
    Player *getPlayer() const { return player; }          // Get the player instance
    Sound *getGameSound() const { return gameSound; }     // Get the Sound instance
    bool initDraw();                                      // Initialize the Draw instance (moved here for Flipper app; must call lcd_init_canvas first)
    bool isActive() const { return shouldExit == false; } // Check if the game is active
    bool isDay() const;                                   // Check if it's currently day time in the game
    bool isRunning() const { return isGameRunning; }      // Check if the game engine is running
    void onGhoulDied();                                   // Called when a ghoul dies; spawns a replacement if round total not yet reached
    void resetInput() { lastInput = -1; }                 // Reset input after processing
    bool soundAllowed() const;                            // Check if sound is allowed based on player settings
    bool startGame();                                     // start the actual game
    bool startGameOnline();                               // start the online multiplayer game
    void updateDraw();                                    // update and draw the game
    void updateInput(int key, bool held);                 // update input for the game
};
