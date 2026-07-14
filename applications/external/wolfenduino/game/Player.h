#ifndef PLAYER_H_
#define PLAYER_H_

#include "game/FixedMath.h"

enum WeaponType {
    WeaponType_Knife,
    WeaponType_Pistol,
    WeaponType_MachineGun,
    WeaponType_ChainGun
};

class Player {
public:
    Player();
    void init();
    void update();
    void move(int16_t deltaX, int16_t deltaZ);
    void damage(uint8_t amount);
    void givePoints(int16_t amount);
    void giveLife();
    void onLoad();

    int16_t x, z;
    angle_t direction;
    uint8_t hp;
    uint8_t killer;
    int8_t turnVelocity;

    uint8_t ticksSinceStrafePressed;
    uint32_t score;
    uint8_t lives;
    int8_t blinkKeyTimer;

    uint8_t enemiesKilled;
    uint8_t treasureCollected;
    uint8_t secretsFound;

    union {
        struct {
            uint8_t hasMachineGun : 1;
            uint8_t hasChainGun   : 1;
            uint8_t hasKey1       : 1;
            uint8_t hasKey2       : 1;
        } inventory;
        uint8_t inventoryFlags;
    };

    struct {
        uint8_t type;
        uint8_t ammo;
        uint8_t frame;
        uint8_t time;
        uint8_t debounce : 1;
        uint8_t shooting : 1;
    } weapon;

private:
    void updateWeapon();
    void shootWeapon();
    void collectItems(int8_t cellX, int8_t cellZ);
    bool collectItem(uint8_t itemType);
    void giveAmmo(uint8_t amount);
    void heal(uint8_t amount);

#ifdef USE_SIMPLE_COLLISIONS
    bool isPlayerColliding();
    bool isPointColliding(int16_t x, int16_t z);
#endif
};

#endif
