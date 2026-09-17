#pragma once

#include <stdint.h>

enum BuildingType
{
	BuildingType_None = 0,
	Residential,
	Commercial,
	Industrial,
	Powerplant,
	Park,
	PoliceDept,
	FireDept,
	Stadium,
	Rubble3x3,
	Rubble4x4,
	Num_BuildingTypes = Stadium + 1
};

inline bool IsRubble(uint8_t buildingType)
{
	return buildingType >= Rubble3x3;
}

typedef struct
{
	uint8_t x : 6;
	uint8_t y : 6;
	uint8_t type : 4;
	uint8_t populationDensity : 4;
	uint8_t onFire : 2;
	bool heavyTraffic : 1;
	bool hasPower : 1;
} Building;

typedef struct
{
	uint16_t cost;
	uint8_t width;
	uint8_t height;
	uint8_t drawTile;
} BuildingInfo;

bool PlaceBuilding(uint8_t buildingType, uint8_t x, uint8_t y);
bool CanPlaceBuilding(uint8_t buildingType, uint8_t x, uint8_t y);
const BuildingInfo* GetBuildingInfo(uint8_t buildingType);
Building* GetBuilding(uint8_t x, uint8_t y);
void DestroyBuilding(Building* building);
