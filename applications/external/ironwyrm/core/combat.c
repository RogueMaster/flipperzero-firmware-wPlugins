#include "combat.h"
#include <stddef.h>

AttackResult ResolveAttack(
    const Wyrm* attacker,
    int attackingPlateIndex,
    Wyrm* defender
) {
    AttackResult result = {0};

    if(
        attacker == NULL ||
        defender == NULL ||
        attackingPlateIndex < 0 ||
        attackingPlateIndex >= attacker->length
    ) {
        return result;
    }

    result.initialDamage = attacker->plates[attackingPlateIndex].power;
    result.remainingDamage = result.initialDamage;

    while(result.remainingDamage > 0 && defender->length > 0) {
        Plate* defendingPlate = &defender->plates[defender->length - 1];

        if(result.remainingDamage < defendingPlate->integrity) {
            defendingPlate->integrity -= result.remainingDamage;
            result.remainingDamage = 0;
        } else {
            result.remainingDamage -= defendingPlate->integrity;
            defender->length--;
            result.platesDestroyed++;
        }
    }

    result.defenderDestroyed = defender->length == 0;

    if(!result.defenderDestroyed) {
        result.remainingOuterIntegrity =
            defender->plates[defender->length - 1].integrity;
    }

    return result;
}
