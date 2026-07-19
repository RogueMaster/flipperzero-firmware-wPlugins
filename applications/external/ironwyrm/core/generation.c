#include "generation.h"

#include "plate.h"
#include "random.h"
#include "wyrm.h"

Wyrm MakeEnemy(int round, RandomGenerator* randomGenerator) {
    Wyrm enemy = {0};

    int basePlateCount = RandomRange(randomGenerator, 2, 4);
    int roundBonus = (round * 2) / 3;
    int targetPlateCount = basePlateCount + roundBonus;

    while(enemy.length < targetPlateCount && enemy.length < MAX_PLATES) {
        AddOuterPlate(&enemy, MakeRandomPlate(randomGenerator));
    }

    return enemy;
}

void FillShop(
    Plate shopPlates[SHOP_CAPACITY],
    uint8_t* shopLength,
    RandomGenerator* randomGenerator
) {
    *shopLength = SHOP_CAPACITY;

    for(int plateIndex = 0; plateIndex < SHOP_CAPACITY; plateIndex++) {
        shopPlates[plateIndex] = MakeRandomPlate(randomGenerator);
    }
}

int CalculateEnemyReward(const Wyrm* enemy) {
    int rewardUnits = ((enemy->length * 2) / 3) + 1;
    return rewardUnits * 3;
}
