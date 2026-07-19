#pragma once

#include "types.h"

Plate MakePlate(MaterialId materialId, ModifierId modifierId);
Plate MakeRandomPlate(RandomGenerator* randomGenerator);
