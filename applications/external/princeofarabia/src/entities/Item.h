#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"
#include "../utils/Stack.h"

#pragma pack(push, 1)

struct Invader_General {
    uint8_t y;
    Direction direction;
    uint8_t left; // Enemy formation visible
    uint8_t right; // Enemy formation visible
    uint8_t lives;
    uint16_t score;
};

struct Invader_General2 {
    uint8_t speed;
    uint8_t bulletCountdown;
    uint8_t deathCountdown;
    uint8_t appearCountdown;
    uint8_t launchOffset;
    uint8_t bulletPlayerCountdown;
    uint8_t byte7;
};

struct Invader_Enemy {
    uint8_t x;
    int8_t y;
    Status status;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
};

struct Invader_Player {
    uint8_t x;
    uint8_t y;
    Status status;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
};

struct Invader_Bullet {
    uint8_t x;
    int8_t y;
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
};

struct Invader_Barrier {
    uint8_t x;
    uint8_t y;
    uint8_t value;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
};

struct Location {
    uint8_t x;
    uint8_t y;
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
};

struct RawData {
    uint8_t x;
    uint8_t y;
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
};

struct ExitDoor {
    uint8_t x;
    uint8_t y;
    uint8_t left;
    uint8_t right;
    uint8_t position;
    Direction direction;
    uint8_t byte5;
};

struct AppearingFloor {
    uint8_t x;
    uint8_t y;
    bool visible;
};

struct ExitDoor_Button {
    uint8_t x;
    uint8_t y;
    uint8_t junk1;
    uint8_t junk2;
    uint8_t junk3;
    uint8_t frame;
};

struct Blade {
    uint8_t x;
    uint8_t y;
    int8_t position;
};

struct Gate { // 0, x, x, 255, 0
    uint8_t x;
    uint8_t y;
    uint8_t position;
    uint8_t closingDelay;
    uint8_t defaultClosingDelay;
    uint8_t closingDelayMax;
    GateMovement movement;
};

struct Sword {
    uint8_t x;
    uint8_t y;
};

struct Skeleton {
    uint8_t x;
    uint8_t y;
};

struct Spikes {
    uint8_t x;
    uint8_t y;
    uint8_t imageType;
    uint8_t position;
    uint8_t openningDelay;
    uint8_t closingDelay;
    uint8_t byte5;
};

struct CollapsingFloor {
    uint8_t x;
    uint8_t y;
    uint8_t distToFall;
    uint8_t defaultTimeToFall;
    uint8_t
        distanceFallen; // 255 not falling. Or 0 to distToFall when falling.  Turns into a CollapsedFloor when fallen.
    uint8_t timeToFall; // 255 if not counting down.
    uint8_t frame;
};

struct FloorButton {
    uint8_t x;
    uint8_t y;
    uint8_t gate1;
    uint8_t gate2;
    uint8_t gate3;
    uint8_t frame;
    uint8_t timeToFall; // How long does gate remain open for.
};

struct Potion {
    uint8_t x;
    uint8_t y;
    uint8_t frame;
};

struct LoveHeart {
    uint8_t x;
    uint8_t y;
    uint8_t counter;
};

struct Mirror {
    uint8_t x;
    uint8_t y;
    Status status;
};

struct Item {
    ItemType itemType;

    union {
        struct Location location;
        struct RawData rawData;
        struct LoveHeart loveHeart;
        struct Gate gate;
        struct ExitDoor exitDoor;
        struct Sword sword;
        struct Sword skeleton;
        struct CollapsingFloor collapsingFloor;
        struct Potion potion;
        struct FloorButton floorButton;
        struct Spikes spikes;
        struct Blade blade;
        struct ExitDoor_Button exitDoor_Button;
        struct Mirror mirror;
        struct AppearingFloor appearingFloor;
        struct Invader_General invader_General;
        struct Invader_General2 invader_General2;
        struct Invader_Enemy invader_Enemy;
        struct Invader_Player invader_Player;
        struct Invader_Bullet invader_Bullet;
        struct Invader_Barrier invader_Barrier;
    } data;
};

#pragma pack(pop)
