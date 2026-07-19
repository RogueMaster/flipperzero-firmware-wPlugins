#pragma once

#include "types.h"

Wyrm MakeEnemy(int round, RandomGenerator* randomGenerator);
void FillShop(Plate shopPlates[SHOP_CAPACITY], uint8_t* shopLength, RandomGenerator* randomGenerator);
int CalculateEnemyReward(const Wyrm* enemy);
