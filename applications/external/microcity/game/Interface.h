#pragma once

#include <stdint.h>
#include "Building.h"

#define INPUT_LEFT 1
#define INPUT_RIGHT 2
#define INPUT_UP 4
#define INPUT_DOWN 8
#define INPUT_A 16
#define INPUT_B 32

enum BrushTypes
{
	Bulldozer,
	RoadBrush,
	PowerlineBrush,
	FirstBuildingBrush,
	LastBuildingBrush = FirstBuildingBrush + Num_BuildingTypes - 2
};

enum
{
	SaveLoadToolbarButton = LastBuildingBrush + 1,
	BudgetToolbarButton
};

enum
{
	StartScreen,
	NewCityMenu,
	InGame,
	InGameDisaster,
	ShowingToolbar,
	SaveLoadMenu,
	BudgetMenu
};

typedef struct
{
	int16_t scrollX, scrollY;		// Where on the map (in pixels) display is scrolled to
	uint8_t selectX, selectY;		// Which tile is selected
	uint8_t brush;					// What will be placed 
	uint8_t selection;      // For when toolbar is open or in a menu
	uint8_t state;    // Which state the game is in
	bool autoBudget : 1;
} UIStateStruct;

extern UIStateStruct UIState;

uint8_t GetInput();

void ProcessInput(void);
void UpdateInterface(void);

void GetBuildingBrushLocation(BuildingType buildingType, uint8_t* outX, uint8_t* outY);


