#include "game/Defines.h"
#include "game/Game.h"
#include "game/FixedMath.h"
#include "game/Draw.h"
#include "game/Map.h"
#include "game/Projectile.h"
#include "game/Particle.h"
#include "game/MapGenerator.h"
#include "game/Platform.h"
#include "game/Entity.h"
#include "game/Enemy.h"
#include "game/Menu.h"

Player Game::player;
const char* Game::displayMessage = nullptr;
uint8_t Game::displayMessageTime = 0;
Game::State Game::state = Game::State::Menu;
uint8_t Game::floor = 1;
uint8_t Game::globalTickFrame = 0;
Stats Game::stats;
Menu Game::menu;

void Game::Init() {
    menu.Init();
    ParticleSystemManager::Init();
    ProjectileManager::Init();
    EnemyManager::Init();
}

bool Game::InMenu() {
    return (state != State::InGame);
}

void Game::GoToMenu() {
    Game::stats.killedBy = EnemyType::Exit;
    SwitchState(State::FadeOut);
}

void Game::StartGame() {
    floor = 1;
    stats.Reset();
    player.Init();
    SwitchState(State::EnteringLevel);
}

void Game::SwitchState(State newState) {
    if(state != newState) {
        state = newState;
        menu.ResetTimer();
    }
}

void Game::ShowMessage(const char* message) {
    constexpr uint8_t messageDisplayTime = 90;

    displayMessage = message;
    displayMessageTime = messageDisplayTime;
}

void Game::NextLevel() {
    if(floor == 10) {
        GameOver();
    } else {
        floor++;
        SwitchState(State::EnteringLevel);
    }
}

void Game::StartLevel() {
    ParticleSystemManager::Init();
    ProjectileManager::Init();
    EnemyManager::Init();
    MapGenerator::Generate();
    EnemyManager::SpawnEnemies();

    player.NextLevel();
    SwitchState(State::InGame);
}

void Game::Draw() {
    switch(state) {
    case State::Menu:
        menu.Draw();
        break;
    case State::EnteringLevel:
        menu.DrawEnteringLevel();
        break;
    // case State::TransitionToLevel:
    //     menu.TransitionToLevel();
    //     break;
    case State::InGame: {
        Renderer::camera.x = player.x;
        Renderer::camera.y = player.y;
        Renderer::camera.angle = player.angle;
        Renderer::Render();
    } break;
    case State::GameOver:
        menu.DrawGameOver();
        break;
    case State::FadeOut:
        menu.FadeOut();
        break;
    default:
        break;
    }
}

void Game::TickInGame() {
    if(displayMessageTime > 0) {
        displayMessageTime--;
        if(displayMessageTime == 0) displayMessage = nullptr;
    }

    player.Tick();

    ProjectileManager::Update();
    ParticleSystemManager::Update();
    EnemyManager::Update();

    if(Map::GetCellSafe(player.x / CELL_SIZE, player.y / CELL_SIZE) == CellType::Exit) {
        NextLevel();
    }

    if(player.hp == 0) {
        GameOver();
    }
}

void Game::Tick() {
    globalTickFrame++;

    switch(state) {
    case State::InGame:
        TickInGame();
        return;
    case State::EnteringLevel:
        menu.TickEnteringLevel();
        return;
    case State::Menu:
        menu.Tick();
        return;
    case State::GameOver:
        menu.TickGameOver();
        return;
    // case State::TransitionToLevel:
    //     return;
    default:
        return;
    }
}

void Game::GameOver() {
    SwitchState(State::FadeOut);
}

void Stats::Reset() {
    killedBy = EnemyType::None;
    chestsOpened = 0;
    coinsCollected = 0;
    crownsCollected = 0;
    scrollsCollected = 0;

    for(uint8_t& killCounter : enemyKills) {
        killCounter = 0;
    }
}
