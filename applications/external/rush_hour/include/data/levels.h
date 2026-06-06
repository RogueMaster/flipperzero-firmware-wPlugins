#pragma once
#include "../domain/level.h"

uint16_t tutu_levels_count(void);
const TutuLevel* tutu_levels_get(uint16_t index); // returns NULL if out of range
