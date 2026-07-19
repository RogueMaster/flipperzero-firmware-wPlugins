#pragma once

#include "combat.h"
#include "types.h"

typedef struct {
    Wyrm player;
    Wyrm enemy;

    Plate shopPlates[SHOP_CAPACITY];
    uint8_t shopLength;

    RandomGenerator randomGenerator;
    AttackResult latestAttack;

    int coins;
    int round;
    int enemyReward;

    uint8_t selectedPlayerPlate;
    uint8_t selectedShopItem;
    uint8_t pendingShopIndex;
    uint8_t introParagraph;

    PlatePlacement selectedPlacement;
    ShopMessage shopMessage;
    GameMode mode;
} GameState;

void InitializeGame(GameState* gameState, uint32_t randomSeed);
void HandleGameAction(GameState* gameState, GameAction action);
int GetMinimumAttackingPlateIndex(const GameState* gameState);
