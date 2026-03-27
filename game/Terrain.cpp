#include <stdint.h>
#include "Terrain.h"
#include "Game.h"
#include "Defines.h"

const uint8_t Terrain1Data[] PROGMEM =
{
#include "Terrain1.inc.h"
};
const uint8_t Terrain2Data[] PROGMEM =
{
#include "Terrain2.inc.h"
};
const uint8_t Terrain3Data[] PROGMEM =
{
#include "Terrain3.inc.h"
};

const char Terrain1Str[] PROGMEM = "River";
const char Terrain2Str[] PROGMEM = "Island";
const char Terrain3Str[] PROGMEM = "Lake";

const char* GetTerrainDescription(uint8_t index)
{
	switch (index)
	{
	default:
	case 0:
		return Terrain1Str;
	case 1:
		return Terrain2Str;
	case 2:
		return Terrain3Str;
	}
}

const uint8_t* GetTerrainData(uint8_t index)
{
	switch (index)
	{
	default:
	case 0: return Terrain1Data;
	case 1: return Terrain2Data;
	case 2: return Terrain3Data;
	}

}

bool IsTerrainClear(int x, int y)
{
	int blockX = x >> 3;
	int blockY = y >> 3;
	uint8_t blockU = x & 7;
	uint8_t blockV = y & 7;
	int index = (blockY * (MAP_WIDTH / 8) + blockX) * 8 + blockU;
	uint8_t mask = 1 << blockV;

	const uint8_t* terrainData = GetTerrainData(State.terrainType);

	uint8_t blockData = pgm_read_byte(&terrainData[index]);

	return (blockData & mask) != 0;
}

uint8_t GetTerrainTile(int x, int y)
{
	bool northClear = y == 0 || IsTerrainClear(x, y - 1);
	bool eastClear = x >= MAP_WIDTH - 1 || IsTerrainClear(x + 1, y);
	bool southClear = y >= MAP_HEIGHT - 1 || IsTerrainClear(x, y + 1);
	bool westClear = x == 0 || IsTerrainClear(x - 1, y);

	if (IsTerrainClear(x, y))
	{
		if (!northClear && !westClear)
			return NORTH_WEST_EDGE_TILE;
		if (!northClear && !eastClear)
			return NORTH_EAST_EDGE_TILE;
		if (!southClear && !westClear)
			return SOUTH_WEST_EDGE_TILE;
		if (!southClear && !eastClear)
			return SOUTH_EAST_EDGE_TILE;

		return FIRST_TERRAIN_TILE + ((((y * 359)) ^ ((x * 431))) & 3);
	}
	else
	{
		return FIRST_WATER_TILE + ((((y * 359)) ^ ((x * 431))) & 3);
	}
}
