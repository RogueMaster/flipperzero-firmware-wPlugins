#pragma once
#include <stdint.h>
#include "game/Defines.h"

#define TONES_END 0x8000

class Sounds
{
public:
	static const uint16_t Attack[];
	static const uint16_t Kill[];
	static const uint16_t Hit[];
	static const uint16_t PlayerDeath[];
	static const uint16_t SpotPlayer[];
	static const uint16_t Shoot[];
	static const uint16_t Pickup[];
	static const uint16_t Ouch[];
};