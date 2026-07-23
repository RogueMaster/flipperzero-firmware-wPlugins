#pragma once

#include "types.h"

void RandomInitialize(RandomGenerator* generator, uint32_t seed);
uint32_t RandomNext(RandomGenerator* generator);
int RandomRange(RandomGenerator* generator, int minimum, int maximum);
