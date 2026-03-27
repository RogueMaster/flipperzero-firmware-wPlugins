#include "Game.h"
#include "Connectivity.h"
#include "Building.h"

void PowerFloodFill(uint8_t x, uint8_t y);

uint8_t GetConnections(int x, int y)
{
	if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT)
	{
		int index = y * MAP_WIDTH + x;
		uint8_t mapVal = State.connectionMap[index >> 2];
		int shift = 2 * (index & 3);
		return (mapVal >> shift) & 3;
	}

	return 0;
}

void SetConnections(int x, int y, uint8_t newVal)
{
	if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT)
	{
		int index = y * MAP_WIDTH + x;
		int shift = 2 * (index & 3);

		index >>= 2;
		uint8_t oldVal = State.connectionMap[index] & (~(3 << shift));
		State.connectionMap[index] = oldVal | (newVal << shift);
	}
}

const uint8_t TileVariants[] PROGMEM =
{
	0, 1, 0, 5, 1, 1, 2, 9, 0, 4, 0, 8, 3, 7, 6, 10
};

enum
{
	Neighbour_North = 1,
	Neighbour_East = 2,
	Neighbour_South = 4,
	Neighbour_West = 8
};

// Returns a 4 bit mask based on neighbouring connectivity
uint8_t GetNeighbouringConnectivity(int x, int y, uint8_t mask)
{
	uint8_t neighbourMask = 0;

	if (y > 0 && (GetConnections(x, y - 1) & mask))
	{
		neighbourMask |= Neighbour_North;
	}
	if (x < MAP_WIDTH - 1 && (GetConnections(x + 1, y) & mask))
	{
		neighbourMask |= Neighbour_East;
	}
	if (y < MAP_HEIGHT - 1 && (GetConnections(x, y + 1) & mask))
	{
		neighbourMask |= Neighbour_South;
	}
	if (x > 0 && (GetConnections(x - 1, y) & mask))
	{
		neighbourMask |= Neighbour_West;
	}

	return neighbourMask;
}

bool IsSuitableForBridgedTile(int x, int y, uint8_t mask)
{
	uint8_t neighbours = GetNeighbouringConnectivity(x, y, mask);
	
	if(neighbours == Neighbour_North || neighbours == Neighbour_East || neighbours == Neighbour_South || neighbours == Neighbour_West
	|| neighbours == (Neighbour_North | Neighbour_South) || neighbours == (Neighbour_East | Neighbour_West))
	{
		if(neighbours & Neighbour_North)
		{
			if(!IsTerrainClear(x, y - 1) && (GetNeighbouringConnectivity(x, y - 1, mask) & (Neighbour_East | Neighbour_West)))
				return false;
		}
		if(neighbours & Neighbour_East)
		{
			if(!IsTerrainClear(x + 1, y) && (GetNeighbouringConnectivity(x + 1, y, mask) & (Neighbour_North | Neighbour_South)))
				return false;
		}
		if(neighbours & Neighbour_South)
		{
			if(!IsTerrainClear(x, y + 1) && (GetNeighbouringConnectivity(x, y + 1, mask) & (Neighbour_East | Neighbour_West)))
				return false;
		}
		if(neighbours & Neighbour_West)
		{
			if(!IsTerrainClear(x - 1, y) && (GetNeighbouringConnectivity(x - 1, y, mask) & (Neighbour_North | Neighbour_South)))
				return false;
		}
		
		return true;
	}
	
	return false;
}

// Based on neighbouring tile types, get which visual tile to use
int GetConnectivityTileVariant(int x, int y, uint8_t mask)
{
	uint8_t neighbours = GetNeighbouringConnectivity(x, y, mask);

	return pgm_read_byte(&TileVariants[neighbours]);
}

inline bool IsTilePowered(uint8_t x, uint8_t y)
{
	int index = y * MAP_WIDTH + x;
	int mask = 1 << (index & 7);
	uint8_t val = GetPowerGrid()[index >> 3];

	return (val & mask) != 0;
}

inline void SetTilePowered(uint8_t x, uint8_t y)
{
	int index = y * MAP_WIDTH + x;
	int mask = 1 << (index & 7);
	GetPowerGrid()[index >> 3] |= mask;
}

void CalculatePowerConnectivity()
{
	// Clear power from grid
	for (int n = 0; n < MAP_WIDTH * MAP_HEIGHT / 8; n++)
	{
		GetPowerGrid()[n] = 0;
	}

	// Flood fill from power plants
	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		if (State.buildings[n].type == Powerplant)
		{
			PowerFloodFill(State.buildings[n].x, State.buildings[n].y);
		}
	}

	// Set powered flags on buildings
	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		if (State.buildings[n].type)
		{
			State.buildings[n].hasPower = IsTilePowered(State.buildings[n].x, State.buildings[n].y);
		}
	}
}

#ifdef USE_FIXED_MEMORY_FILL

// Power flood fill method is based on the Wikipedia 'Fixed memory method (right hand fill method)'
enum
{
	FILL_NORTH = 0,
	FILL_NORTHEAST,
	FILL_EAST,
	FILL_SOUTHEAST,
	FILL_SOUTH,
	FILL_SOUTHWEST,
	FILL_WEST,
	FILL_NORTHWEST
};

// If there is a powerline / building on this tile that hasn't been powered yet
bool IsFillEmpty(uint8_t x, uint8_t y)
{
	return (GetConnections(x, y) & PowerlineMask) != 0 && !IsTilePowered(x, y);
}

uint8_t GetFilledNeighbourCount(uint8_t x, uint8_t y)
{
	uint8_t count = 0;

	if (x == 0 || !IsFillEmpty(x - 1, y))
		count++;
	if (x == MAP_WIDTH - 1 || !IsFillEmpty(x + 1, y))
		count++;
	if (y == 0 || !IsFillEmpty(x, y - 1))
		count++;
	if (y == MAP_HEIGHT - 1 || !IsFillEmpty(x, y + 1))
		count++;

	return count;
}

bool IsFilledInDir(uint8_t x, uint8_t y, uint8_t dir)
{
	switch (dir)
	{
	default:
	case FILL_NORTH:
		return y > 0 ? !IsFillEmpty(x, y - 1) : true;
	case FILL_NORTHEAST:
		return y > 0 && x < MAP_WIDTH - 1 ? !IsFillEmpty(x + 1, y - 1) : true;
	case FILL_EAST:
		return x < MAP_WIDTH - 1 ? !IsFillEmpty(x + 1, y) : true;
	case FILL_SOUTHEAST:
		return x < MAP_WIDTH - 1 && y < MAP_HEIGHT - 1 ? !IsFillEmpty(x + 1, y + 1) : true;
	case FILL_SOUTH:
		return y < MAP_HEIGHT - 1 ? !IsFillEmpty(x, y + 1) : true;
	case FILL_SOUTHWEST:
		return x > 0 && y < MAP_HEIGHT - 1 ? !IsFillEmpty(x - 1, y + 1) : true;
	case FILL_WEST:
		return x > 0 ? !IsFillEmpty(x - 1, y) : true;
	case FILL_NORTHWEST:
		return x > 0 && y > 0 ? !IsFillEmpty(x - 1, y - 1) : true;
	}
}

uint8_t FillFrontLeft(uint8_t dir)
{
	return (dir - 1) & 7;
}

uint8_t FillBackLeft(uint8_t dir)
{
	return (dir - 3) & 7;
}

uint8_t FillTurnLeft(uint8_t dir)
{
	return (dir - 2) & 7;
}

uint8_t FillTurnRight(uint8_t dir)
{
	return (dir + 2) & 7;
}

uint8_t FillTurnAround(uint8_t dir)
{
	return (dir + 4) & 7;
}

void FillMoveForward(uint8_t* x, uint8_t* y, uint8_t dir)
{
	switch (dir)
	{
	case FILL_NORTH:
		--(*y);
		break;
	case FILL_EAST:
		++(*x);
		break;
	case FILL_SOUTH:
		++(*y);
		break;
	case FILL_WEST:
		--(*x);
		break;
	}
}

void PowerFloodFill(uint8_t x, uint8_t y)
{
	uint8_t fillDir = FILL_NORTH;
	uint8_t mark1X = 0xff, mark1Y = 0xff, mark1Dir = FILL_NORTH;
	uint8_t mark2X = 0xff, mark2Y = 0xff, mark2Dir = FILL_NORTH;
	bool mark1Set = false, mark2Set = false;
	bool backtrack = false;
	bool findloop = false;

	// Move to edge
	while (y > 0 && IsFillEmpty(x, y - 1))
	{
		y--;
	}

	goto Fill_Start;

	while (1)
	{
		FillMoveForward(&x, &y, fillDir);

		// If right pixel is empty
		if (IsFilledInDir(x, y, FillTurnRight(fillDir)) == false)
		{
			if (backtrack && !findloop &&
				(IsFilledInDir(x, y, fillDir) == false
					|| IsFilledInDir(x, y, FillTurnLeft(fillDir)) == false))
			{
				findloop = true;
			}

			// Turn right
			fillDir = FillTurnRight(fillDir);

		Fill_Paint:

			FillMoveForward(&x, &y, fillDir);
		}

	Fill_Start:
		uint8_t filledNeighbourCount = GetFilledNeighbourCount(x, y);
		if (filledNeighbourCount == 4)
		{
			SetTilePowered(x, y);
			break;
		}

		do
		{
			fillDir = FillTurnRight(fillDir);
		} while (IsFilledInDir(x, y, fillDir) == false);
		do
		{
			fillDir = FillTurnLeft(fillDir);
		} while (IsFilledInDir(x, y, fillDir) != false);

		switch (filledNeighbourCount)
		{
		case 1:
			if (backtrack)
			{
				findloop = true;
			}
			else if (findloop)
			{
				mark1Set = true;
			}
			else if (IsFilledInDir(x, y, FillFrontLeft(fillDir)) == false
				&& IsFilledInDir(x, y, FillBackLeft(fillDir)) == false)
			{
				mark1Set = false;
				SetTilePowered(x, y);
				goto Fill_Paint;
			}
			break;
		case 2:
			if (IsFilledInDir(x, y, FillTurnAround(fillDir)) != false)
			{
				if (IsFilledInDir(x, y, FillFrontLeft(fillDir)) == false)
				{
					mark1Set = false;
					SetTilePowered(x, y);
					goto Fill_Paint;
				}
			}
			else if (!mark1Set)
			{
				mark1X = x;
				mark1Y = y;
				mark1Set = true;
				mark1Dir = fillDir;
				mark2Set = false;
				findloop = false;
				backtrack = false;
			}
			else
			{
				if (!mark2Set)
				{
					if (mark1Set && x == mark1X && y == mark1Y)
					{
						if (fillDir == mark1Dir)
						{
							mark1Set = false;
							fillDir = FillTurnAround(fillDir);
							SetTilePowered(x, y);
							goto Fill_Paint;
						}
						else
						{
							backtrack = true;
							findloop = false;
							fillDir = mark1Dir;
						}
					}
					else if (findloop)
					{
						mark2X = x;
						mark2Y = y;
						mark2Dir = fillDir;
						mark2Set = true;
					}
				}
				else
				{
					if (mark1Set && x == mark1X && y == mark1Y)
					{
						x = mark2X;
						y = mark2Y;
						fillDir = mark2Dir;
						mark1Set = false;
						mark2Set = false;
						backtrack = false;
						fillDir = FillTurnAround(fillDir);
						SetTilePowered(x, y);
						goto Fill_Paint;
					}
					else if (mark2Set && x == mark2X && y == mark2Y)
					{
						mark1X = x;
						mark1Y = y;
						mark1Set = true;
						fillDir = mark2Dir;
						mark1Dir = mark2Dir;
						mark2Set = false;
					}
				}
			}
			break;
		case 3:
			mark1Set = false;
			SetTilePowered(x, y);
			goto Fill_Paint;
			break;
		default:
			break;
		}
	}
}

#else

#define STACK_PUSH(px, py) \
	*stackPtr++ = px; \
	*stackPtr++ = py; \
	stackSize++; \

#define STACK_POP(px, py) \
	stackPtr--; py = *stackPtr; \
	stackPtr--; px = *stackPtr; \
	stackSize--;

void PowerFloodFill(uint8_t x, uint8_t y)
{
	uint8_t* grid = (uint8_t*)GetPowerGrid();
	uint8_t* stackPtr = grid + (MAP_WIDTH * MAP_HEIGHT / 8);
	uint8_t stackSize = 0;

	STACK_PUSH(x, y);

	while (stackSize)
	{
		STACK_POP(x, y);
		int8_t y1 = y;

		while (y1 >= 0 && (GetConnections(x, y1) & PowerlineMask) && !IsTilePowered(x, y1))
		{
			y1--;
		}
		y1++;
		bool spanLeft = false;
		bool spanRight = false;
		while (y1 < MAP_HEIGHT && (GetConnections(x, y1) & PowerlineMask) && !IsTilePowered(x, y1))
		{
			SetTilePowered(x, y1);

			bool canFillLeft = (GetConnections(x - 1, y1) & PowerlineMask) && !IsTilePowered(x - 1, y1);

			if (!spanLeft && x > 0 && canFillLeft)
			{
				STACK_PUSH(x - 1, y1);
				spanLeft = true;
			}
			else if (spanLeft && (x - 1 == 0 || !canFillLeft))
			{
				spanLeft = false;
			}

			bool canFillRight = (GetConnections(x + 1, y1) & PowerlineMask) && !IsTilePowered(x + 1, y1);

			if (!spanRight && canFillRight)
			{
				STACK_PUSH(x + 1, y1);
				spanRight = true;
			}
			else if (spanRight && x < MAP_WIDTH - 1 && !canFillRight)
			{
				spanRight = false;
			}
			y1++;
		}
	}
}

#endif
