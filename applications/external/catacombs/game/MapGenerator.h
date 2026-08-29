#pragma once

#include <stdint.h>
#include "game/Map.h"

class MapGenerator
{
public:
	static void Generate();

private:

	struct NeighbourInfo
	{
		union
		{
			uint8_t mask;

			struct
			{
				bool hasNorth : 1;
				bool hasEast : 1;
				bool hasSouth : 1;
				bool hasWest : 1;

				bool canDemolishNorth : 1;
				bool canDemolishEast : 1;
				bool canDemolishSouth : 1;
				bool canDemolishWest : 1;
			};
		};

		uint8_t count;

		bool IsCorner() const 
		{
			if (count != 2)
				return false;

			if (hasNorth && hasEast)
				return true;
			if (hasEast && hasSouth)
				return true;
			if (hasSouth && hasWest)
				return true;
			if (hasWest && hasNorth)
				return true;

			return false;
		}
	};

	static uint8_t CountNeighbours(uint8_t x, uint8_t y);
	static uint8_t CountImmediateNeighbours(uint8_t x, uint8_t y);
	static NeighbourInfo GetRoomNeighbourMask(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
	static void SplitMap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t doorX, uint8_t doorY);

	static NeighbourInfo GetCellNeighbourInfo(uint8_t x, uint8_t y);
	static uint8_t GetDistanceToCellType(uint8_t x, uint8_t y, CellType cellType);
};
