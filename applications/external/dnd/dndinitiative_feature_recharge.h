#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

typedef enum {
    DndFeatureFastRechargeTurn,
    DndFeatureFastRechargeEncounter,
} DndFeatureFastRechargeEvent;

/* Lightweight Initiative-only recharge path. Turn/Encounter recharge does not
   need the full character/progression store because those cadences always reset
   to the feature's persisted uses_max value. */
bool dndinitiative_feature_recharge_fast_recharge(
    Storage* storage,
    uint32_t profile,
    DndFeatureFastRechargeEvent event);
