#ifndef ENGINE_H_
#define ENGINE_H_

#ifdef _WIN32
#include "game/../Windows/SDLPlatform.h"
#else
#include "game/ArduboyPlatform.h"
#endif

#include "game/Platform.h"
#include "game/Renderer.h"
#include "game/Player.h"
#include "game/Map.h"
#include "game/Actor.h"
#include "game/Menu.h"
#include "game/Save.h"

enum {
    GameState_Menu,
    GameState_PauseMenu,
    GameState_Loading,
    GameState_Playing,
    GameState_Dead,
    GameState_EnterNextLevel,
    GameState_StartingLevel
};

enum Difficulty {
    Difficulty_Baby,
    Difficulty_Easy,
    Difficulty_Medium,
    Difficulty_Hard
};

class Engine {
public:
    void init();
    void update();
    void draw();
    void startNewGame();
    void startLevel(bool resetPlayer = true);
    void startingLevel();
    void finishLevel();
    void loadGame();
    void enterNextLevel();
    Actor* spawnActor(uint8_t spawnId, uint8_t actorType, int8_t cellX, int8_t cellZ);
    void fadeTransition();

    bool inGame();
    void exitToMenu();

    Renderer renderer;
    Player player;
    Map map;
    Menu menu;
    SaveSystem save;

    Actor actors[MAX_ACTIVE_ACTORS];

    int16_t frameCount;
    uint8_t gameState;
    uint8_t difficulty;

    uint8_t streamBuffer[STREAM_BUFFER_SIZE];

    int8_t screenFade;
};

extern Engine engine;

#endif
