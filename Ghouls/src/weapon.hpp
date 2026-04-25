#pragma once
#include "pico-game-engine/engine/draw.hpp"
#include "pico-game-engine/engine/entity.hpp"
#include "projectile.hpp"

typedef enum
{
    //                         category, damage, ammo, projectile type
    WEAPON_NONE = 0,        // none, 0, 0, none
    WEAPON_RIFLE,           // shooter, 15, 30, bullet
    WEAPON_SHOTGUN,         // shooter, 20, 10, bullet
    WEAPON_ROCKET_LAUNCHER, // launcher, 50, 5, rocket
    WEAPON_CROSSBOW,        // launcher, 35, 15, arrow
} WeaponType;

/*
when as a standalone wepon:
 - position
 - 3D sprite on the ground to pick up
when equipped by player:
 - 3D sprite attached to player (rendered in Player::render)
 - damage
 - ammo

 so instead just set the constructor to take in the weapon type
*/

class Weapon : public Entity
{
public:
    Weapon(WeaponType type = WEAPON_NONE, float height = 2.0f, Vector position = Vector(0, 0));
    ~Weapon();

    void addAmmo(uint16_t amount);       // add ammo to the current ammo count
    bool fire(Level *level);             // Attempt to fire the weapon, returns true if fired successfully
    uint16_t getAmmo() const;            // Get the current ammo count
    float getDamage() const;             // Get the damage this weapon will deal on hit
    WeaponType getWeaponType() const;    // Get the type of the weapon
    bool isHeld() const;                 // Check if the weapon is currently held by a player
    void reset(Level *level);            // Reset the weapon's state
    void setAmmo(uint16_t ammo);         // Set the current ammo count
    void setDamage(float damage);        // Set the damage this weapon will deal
    void setHeld(bool held);             // Set whether the weapon is currently held by a player
    void setWeaponType(WeaponType type); // Set the type of the weapon and update its properties accordingly
    void update(Game *game) override;

private:
    uint16_t ammo;
    float damage;
    bool held;

    WeaponType weaponType;
    ProjectileType projectileType;
    Projectile *currentProjectile;

    bool canFire() const;                  // Check if the weapon can fire based on ammo
    void makeCrossbow(float height);       // create a 3D crossbow sprite with the specified height
    void makeRifle(float height);          // create a 3D rifle sprite with the specified height
    void makeRocketLauncher(float height); // create a 3D rocket launcher sprite with the specified height
    void makeShotgun(float height);        // create a 3D shotgun sprite with the specified height
};