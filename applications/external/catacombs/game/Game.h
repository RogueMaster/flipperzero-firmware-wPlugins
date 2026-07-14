#pragma once

#include <stdint.h>
#include "game/Defines.h"
#include "game/Player.h"
#include "game/Enemy.h"
#include "game/Menu.h"

class Entity;

struct Stats {
    EnemyType killedBy;
    uint8_t enemyKills[(int)EnemyType::NumEnemyTypes];
    uint8_t chestsOpened;
    uint8_t crownsCollected;
    uint8_t scrollsCollected;
    uint8_t coinsCollected;

    void Reset();
};

class Game {
public:
    static Menu menu;
    static uint8_t globalTickFrame;

    enum class State : uint8_t {
        Menu,
        EnteringLevel,
        InGame,
        GameOver,
        FadeOut,
        // TransitionToLevel
    };

    static void Init();
    static void Tick();
    static void Draw();

    static bool InMenu();
    static void GoToMenu();

    static void StartGame();
    static void StartLevel();
    static void NextLevel();
    static void GameOver();

    static void SwitchState(State newState);

    static void ShowMessage(const char* message);

    static Player player;

    static const char* displayMessage;
    static uint8_t displayMessageTime;
    static uint8_t floor;

    static Stats stats;

private:
    static void TickInGame();
    static State state;
};
