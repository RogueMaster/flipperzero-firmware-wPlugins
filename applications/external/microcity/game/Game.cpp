#include "Game.h"
#include "Draw.h"
#include "Interface.h"
#include "Simulation.h"

GameState State;

uint16_t GetRandFromSeed(uint16_t randVal)
{
	uint16_t lsb = randVal & 1;
	randVal >>= 1;
	if (lsb == 1)
		randVal ^= 0xB400u;

	return randVal;
}

uint16_t GetRand()
{
	static uint16_t randVal = 0xABC;

	randVal = GetRandFromSeed(randVal);

	return randVal - 1;
}

void InitGame()
{
	uint8_t* ptr = (uint8_t*)(&State);
	for (size_t n = 0; n < sizeof(GameState); n++)
	{
		*ptr = 0;
		ptr++;
	}

	State.taxRate = STARTING_TAX_RATE;
	State.timeToNextDisaster = MAX_TIME_BETWEEN_DISASTERS;

	ResetVisibleTileCache();
	UIState.brush = RoadBrush; //FirstBuildingBrush + 1;
	FocusTile(MAP_WIDTH / 2, MAP_HEIGHT / 2);

	State.money = STARTING_FUNDS;
	UIState.autoBudget = true;
}

void FocusTile(uint8_t x, uint8_t y)
{
	UIState.selectX = x;
	UIState.selectY = y;
	UIState.scrollX = UIState.selectX * 8 + TILE_SIZE / 2 - DISPLAY_WIDTH / 2;
	UIState.scrollY = UIState.selectY * 8 + TILE_SIZE / 2 - DISPLAY_HEIGHT / 2;
}

void TickGame()
{
	if (UIState.state == InGame || UIState.state == ShowingToolbar)
	{
		Simulate();
	}
	if (UIState.state == InGameDisaster)
	{
		if (UIState.selection == 0)
		{
			UIState.state = InGame;
		}
		else UIState.selection--;
	}

	ProcessInput();
	UpdateInterface();

	Draw();
}
