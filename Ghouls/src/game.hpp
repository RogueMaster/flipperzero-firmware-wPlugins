#pragma once
#include "config.hpp"
#include "pico-game-engine/engine/engine.hpp"
#include "general.hpp"
#include "player.hpp"
#include "time.hpp"

class GhoulsGame
{
private:
    int currentLevelIndex = 0;               // Current level index
    DynamicMap *currentDynamicMap = nullptr; // current dynamic map
    uint16_t currentRound = 1;               // Current round number
    bool dayJustSwitched = true;             // Flag to track if day/night just switched
    Draw *draw = nullptr;                    // Draw instance
    GameEngine *engine = nullptr;            // Engine instance
    Time *gameTime = nullptr;                // Game time instance
    Sprite3D *houseSprite = nullptr;         // Static Sprite3D instance for houses
    bool isGameRunning = false;              // Flag to check if the game is running
    int lastInput = -1;                      // Last input key pressed
    Player *player = nullptr;                // Player instance
    ToggleState soundToggle = ToggleOn;      // sound toggle state
    bool shouldExit = false;                 // Flag to signal exit the game
    Sprite3D *treeSprite = nullptr;          // Static Sprite3D instance for trees
    Sprite3D *wallSprite = nullptr;          // Static Sprite3D instance for horizontal walls (top/bottom borders)
    Sprite3D *vWallSprite = nullptr;         // Static Sprite3D instance for vertical walls (left/right borders)
    //
    static const Vector housePositions[HOUSE_SPAWN_COUNT]; // pre-computed house spawn positions (scaled)
    static const Vector treePositions[TREE_SPAWN_COUNT];   // pre-computed tree spawn positions (scaled)
    static const Vector wallPositions[MAP_OUTER_WALLS];    // pre-computed wall center positions
    //
    int atoi(const char *nptr) { return (int)strtol(nptr, NULL, 10); } // convert string to integer
    Vector getRandomGhoulPosition(Level *level);                       // get a random position for spawning ghouls
    Vector getRandomWeaponPosition(Level *level);                      // get a random position for spawning weapons
    WeaponType getUniqueWeaponType(Level *level);                      // get a unique weapon type (only two of each type allowed)
    void increaseDifficulty();                                         // increase game difficulty by increasing enemy spawn rates/stats
    bool initializeSprites();                                          // initialize the static Sprite3D instances for trees, houses, and walls
    void inputManager();                                               // manage input for the game, called from updateInput
    void makeGhoulsGoHome();                                           // make ghouls return to spawn
    void makeGhoulsGoToPlayer();                                       // make ghouls target the player
    bool positionExistsInLevel(Level *level, Vector position);         // check if a position is already occupied by an entity in the level
    void refreshPlayer();                                              // refresh player state (e.g., health and weapon displays) after day/night switch
    void registerSpritePositionsOnMap();                               // register the positions of the static sprites (houses, trees, walls) on the DynamicMap for collision detection
    bool removeGhoulsFromLevel();                                      // remove all ghouls from the level
    void renderHouses(Game *game);                                     // render houses in the level based on their positions relative to the player
    void renderTrees(Game *game);                                      // render trees in the level based on their positions relative to the player
    void renderWalls(Game *game);                                      // render walls in the level based on their positions relative to the player
    bool spawnGhouls();                                                // Spawn ghouls into the current level for the current round
    bool setDynamicMap();                                              // set the current dynamic map
    Level *spawnLevel(Game *game);                                     // spawn a new level based on index
    bool spawnWeapons(Level *level);                                   // spawn all the weapons into the level

public:
    GhoulsGame(const char *username, const char *password, bool soundEnabled = true);
    ~GhoulsGame();

    //
    bool collisionMapCheck(Vector new_position);                           // check if the new position collides with any tiles on the map
    Time *getGameTime() const { return gameTime; }                         // Get the game time instance
    void endGame();                                                        // end the game and return to the submenu
    int getCurrentInput() const { return lastInput; }                      // Get the last input key pressed
    GameEngine *getEngine() const { return engine; }                       // Get the game engine instance
    Draw *getDraw() const { return draw; }                                 // Get the Draw instance
    DynamicMap *getCurrentDynamicMap() const { return currentDynamicMap; } // Get the current dynamic map
    bool initDraw();                                                       // Initialize the Draw instance (moved here for Flipper app; must call lcd_init_canvas first)
    bool isActive() const { return shouldExit == false; }                  // Check if the game is active
    bool isRunning() const { return isGameRunning; }                       // Check if the game engine is running
    void renderEnvironment(Game *game);                                    // Render the environment (houses, trees, walls) in the level based on their positions relative to the player
    void resetInput() { lastInput = -1; }                                  // Reset input after processing
    bool startGame();                                                      // start the actual game
    bool startGameOnline();                                                // start the online multiplayer game
    void updateDraw();                                                     // update and draw the game
    void updateInput(int key, bool held);                                  // update input for the game
};
