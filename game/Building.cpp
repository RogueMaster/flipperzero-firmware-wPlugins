#include "Game.h"
#include "Building.h"
#include "Connectivity.h"
#include "Draw.h"

const BuildingInfo BuildingMetaData[] PROGMEM =
{
	// None,
	{ 0, 0, 0, 0 },
	// Residential,
	{ 100, 3, 3, 64 },
	// Commercial,
	{ 100, 3, 3, 67 },
	// Industrial,
	{ 100, 3, 3, 70 },
	// Powerplant,
	{ 3000, 4, 4, 160 },
	// Park,
	{ 50, 3, 3, 73 },
	// PoliceDept,
	{ 500, 3, 3, 76 },
	// FireDept,
	{ 500, 3, 3, 124 },
	// Stadium,
	{ 3000, 4, 4, 164 },
	// Rubble3x3,
	{ 0, 3, 3, 0 },
	// Rubble4x4,
	{ 0, 4, 4, 0 },
};

const BuildingInfo* GetBuildingInfo(uint8_t buildingType)
{
	return &BuildingMetaData[buildingType];
}

bool PlaceBuilding(uint8_t buildingType, uint8_t x, uint8_t y)
{
	int index = 0;

	while (index < MAX_BUILDINGS)
	{
		if (State.buildings[index].type == 0)
		{
			break;
		}
		index++;
	}

	if (index == MAX_BUILDINGS)
	{
		// Look for rubble placements and replace them instead:
		index = 0;

		while (index < MAX_BUILDINGS)
		{
			if (State.buildings[index].type == Rubble3x3 || State.buildings[index].type == Rubble4x4)
			{
				break;
			}
			index++;
		}

		if (index == MAX_BUILDINGS)
		{
			return false;
		}
	}

	Building* newBuilding = &State.buildings[index];
	newBuilding->type = buildingType;
	newBuilding->x = x;
	newBuilding->y = y;
	newBuilding->populationDensity = 0;
	newBuilding->hasPower = false;
	newBuilding->onFire = 0;

	// Internally building space is represented as power lines to correctly flood fill etc
	const BuildingInfo* metadata = GetBuildingInfo(buildingType);
	uint8_t width = pgm_read_byte(&metadata->width);
	uint8_t height = pgm_read_byte(&metadata->height);
	uint8_t connectionMask = buildingType == Park ? 0 : PowerlineMask;

	for (int i = x; i < x + width; i++)
	{
		for (int j = y; j < y + height; j++)
		{
			SetConnections(i, j, connectionMask);
		}
	}

	// Check for overlapping rubble and remove
	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		Building* building = &State.buildings[n];

		if (IsRubble(building->type))
		{
			const BuildingInfo* otherMetadata = GetBuildingInfo(building->type);
			uint8_t otherWidth = pgm_read_byte(&otherMetadata->width);
			uint8_t otherHeight = pgm_read_byte(&otherMetadata->height);

			if (x + width > building->x && x < building->x + otherWidth
				&& y + height > building->y && y < building->y + otherHeight)
			{
				building->type = 0;
			}
		}
	}

	RefreshBuildingTiles(newBuilding);

	return true;
}

bool CanPlaceBuilding(uint8_t buildingType, uint8_t x, uint8_t y)
{
	const BuildingInfo* metadata = GetBuildingInfo(buildingType);
	uint8_t width = pgm_read_byte(&metadata->width);
	uint8_t height = pgm_read_byte(&metadata->height);

	if (x + width > MAP_WIDTH)
		return false;
	if (y + height > MAP_HEIGHT)
		return false;

	// Check if trying to build on top of road
	for (int i = x; i < x + width; i++)
	{
		for (int j = y; j < y + height; j++)
		{
			// Check terrain is not water
			if (IsTerrainClear(i, j) == false)
				return false;

			if (GetConnections(i, j) & RoadMask)
				return false;
		}
	}

	// Check building overlaps
	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		Building* building = &State.buildings[n];

		if (building->type && !IsRubble(building->type))
		{
			const BuildingInfo* otherMetadata = GetBuildingInfo(building->type);
			uint8_t otherWidth = pgm_read_byte(&otherMetadata->width);
			uint8_t otherHeight = pgm_read_byte(&otherMetadata->height);

			if (x + width > building->x && x < building->x + otherWidth
				&& y + height > building->y && y < building->y + otherHeight)
			{
				return false;
			}
		}

	}

	return true;
}

Building* GetBuilding(uint8_t x, uint8_t y)
{
	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		Building* building = &State.buildings[n];

		if (building->type)
		{
			const BuildingInfo* metadata = GetBuildingInfo(building->type);
			uint8_t width = pgm_read_byte(&metadata->width);
			uint8_t height = pgm_read_byte(&metadata->height);

			if (x >= building->x && x < building->x + width && y >= building->y && y < building->y + height)
			{
				return building;
			}
		}
	}

	return nullptr;
}

void DestroyBuilding(Building* building)
{
	const BuildingInfo* info = GetBuildingInfo(building->type);
	uint8_t width = pgm_read_byte(&info->width);
	uint8_t height = pgm_read_byte(&info->height);

	for (int y = building->y; y < building->y + height; y++)
	{
		for (int x = building->x; x < building->x + width; x++)
		{
			SetConnections(x, y, 0);
		}
	}

	building->onFire = 0;
	building->type = width == 3 ? Rubble3x3 : Rubble4x4;

	for (uint8_t y = building->y; y < building->y + height; y++)
	{
		for (uint8_t x = building->x; x < building->x + width; x++)
		{
			SetTile(x, y, RUBBLE_TILE);
		}
	}
}