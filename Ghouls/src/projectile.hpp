#pragma once
#include "pico-game-engine/engine/entity.hpp"

typedef enum
{
    PROJECTILE_NONE = 0, // default/undefined projectile type
    PROJECTILE_BULLET,   // fast-moving, low-damage
    PROJECTILE_ARROW,    // medium speed, medium damage
    PROJECTILE_ROCKET,   // slow-moving, explosive-damage
    PROJECTILE_SHELL     // fast-moving, burst-damage
} ProjectileType;

class Projectile : public Entity
{
public:
    Projectile(ProjectileType type = PROJECTILE_NONE, float height = 2.0f, Vector position = Vector(0, 0));
    ~Projectile();
    void collision(Entity *other, Game *game) override;
    Entity *getEnemy(Game *game, uint8_t shift = 0) const; // Helper to get an enemy entity from the game
    Entity *getPlayer(Game *game) const;                   // Helper to get player entity from the game
    void setDamage(float damage);                          // Set the damage this projectile will deal on collision
    void setMotion(bool inMotion);                         // Set whether the projectile is currently in motion
    void setProjectileType(ProjectileType type);           // Set the type of the projectile (e.g., bullet, arrow, rocket)
    void setSpeed(float speed);                            // Set the speed of the projectile (ticks)
    void update(Game *game) override;

private:
    float damage;
    bool inMotion;
    ProjectileType projectileType;
    uint8_t collisionCount;
    float speed;

    void makeArrow(float height);  // create a 3D arrow sprite with the specified height
    void makeBullet(float height); // create a 3D bullet sprite with the specified height
    void makeRocket(float height); // create a 3D rocket sprite with the specified height
    void makeShell(float height);  // create a 3D shell sprite with the specified height
};