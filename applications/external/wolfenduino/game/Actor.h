#ifndef ACTOR_H_
#define ACTOR_H_

#include <stdint.h>

enum ActorType {
    ActorType_Empty,
    ActorType_Guard,
    ActorType_Dog,
    ActorType_SS,
    ActorType_Boss
};

enum ActorState {
    ActorState_Idle,
    ActorState_Waking,
    ActorState_Active,
    ActorState_Injured,
    ActorState_Aiming,
    ActorState_Shooting,
    ActorState_Recoiling,
    ActorState_Dying,
    ActorState_Dead
};

class Actor {
public:
    void init(uint8_t spawnId, uint8_t actorType, uint8_t cellX, uint8_t cellZ);
    void update();
    void draw();
    void damage(int amount);

    void switchState(uint8_t newState);
    void updateFrozenState();

    bool tryMove();
    void pickNewTargetCell();
    bool tryPickCell(int8_t x, int8_t z);
    bool tryPickCells(int8_t deltaX, int8_t deltaZ);

    void dropItem(uint8_t itemType);
    bool tryDropItem(uint8_t itemType, int cellX, int cellZ);

    bool isPlayerColliding();

    void shootPlayer();
    bool shouldShootPlayer(bool movementSucceeded);
    int8_t getPlayerCellDistance();

    uint8_t spawnId;
    uint8_t type;
    int16_t x, z;
    uint8_t state;
    uint8_t frame;
    int16_t hp;

    uint8_t targetCellX, targetCellZ;

    struct {
        bool frozen : 1;
    } flags;
};

#endif
