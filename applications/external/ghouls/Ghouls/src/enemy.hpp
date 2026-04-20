#pragma once
#include "config.hpp"
#include "pico-game-engine/engine/entity.hpp"
#include "pico-game-engine/engine/vector.hpp"
#include "pico-game-engine/engine/game.hpp"

typedef enum
{
    ENEMY_BULLY = 0,   // basic enemy
    ENEMY_PUNK = 1,    // shorter but faster
    ENEMY_CREEPER = 2, // taller but slower
} EnemyType;

class Enemy : public Entity
{
    Vector getPlayerPosition(Game *game); // Get the main player's position
    void makeBully(float height);         // Create a bully enemy with the specified height
    void makeCreeper(float height);       // Create a creeper enemy with the specified height
    void makePunk(float height);          // Create a punk enemy with the specified height

public:
    Enemy(const char *name, Vector position, EnemyType enemyType = ENEMY_BULLY, float height = 2.0f, float width = 1.5f, float rotation = 0.0f, Vector endPosition = Vector(-1, -1));
    ~Enemy() = default;
    void update(Game *game) override;
};
