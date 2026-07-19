#pragma once

#include "types.h"

typedef struct {
    uint8_t initialDamage;
    uint8_t remainingDamage;
    uint8_t platesDestroyed;
    uint8_t remainingOuterIntegrity;
    bool defenderDestroyed;
} AttackResult;

AttackResult ResolveAttack(
    const Wyrm* attacker,
    int attackingPlateIndex,
    Wyrm* defender
);
