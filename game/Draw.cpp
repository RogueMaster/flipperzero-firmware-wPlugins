#include "Game.h"
#include "Draw.h"
#include "Interface.h"
#include "Font.h"
#include "Strings.h"

const uint8_t TileImageData[] PROGMEM =
{
#include "TileData.h"
};

#include "LogoBitmap.h"

// Currently visible tiles are cached so they don't need to be recalculated between frames
uint8_t VisibleTileCache[VISIBLE_TILES_X * VISIBLE_TILES_Y];
int8_t CachedScrollX, CachedScrollY;
uint8_t AnimationFrame = 0;

// A map of which tiles should be on fire when a building is on fire
#define FIREMAP_SIZE 16
const uint8_t FireMap[FIREMAP_SIZE] PROGMEM =
{ 1,2,3,1,2,3,1,3,1,2,3,2,2,1,3,1 };

const uint8_t BuildingPopulaceMap[] PROGMEM =
{ 1,0xd,5,0xb,7,0xe,3,4,1,6,0xc,2,0xa,9,0xb,8 };

const uint8_t* GetTileData(uint8_t tile)
{
	return TileImageData + (tile * 8);
}

inline uint8_t GetProcAtTile(uint8_t x, uint8_t y)
{
	return (uint8_t)((((y * 359)) ^ ((x * 431))));
}

bool HasHighTraffic(int x, int y)
{
	// First check for buildings
	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		Building* building = &State.buildings[n];

		if (building->type && building->heavyTraffic)
		{
			if (x < building->x - 1 || y < building->y - 1)
				continue;
			const BuildingInfo* info = GetBuildingInfo(building->type);
			uint8_t width = pgm_read_byte(&info->width);
			uint8_t height = pgm_read_byte(&info->height);
			if (x > building->x + width || y > building->y + height)
				continue;

			return true;
		}
	}

	return false;
}

uint8_t CalculateBuildingTile(Building* building, uint8_t x, uint8_t y)
{
	const BuildingInfo* info = GetBuildingInfo(building->type);
	uint8_t height = pgm_read_byte(&info->height);
	uint8_t tile = pgm_read_byte(&info->drawTile);

	if (building->onFire)
	{
		if (!((building->type == Industrial || building->type == Commercial || building->type == Residential)
			&& x == 1 && y == 1))
		{
			int index = y * height + x + GetProcAtTile(building->x, building->y);
			bool onFire = building->onFire >= pgm_read_byte(&FireMap[index & (FIREMAP_SIZE - 1)]);

			if (onFire)
			{
				uint8_t procVal = GetProcAtTile(building->x + x, building->y + y);
				return FIRST_FIRE_TILE + (procVal & 3);
			}
		}
	}

	if (IsRubble(building->type))
		return RUBBLE_TILE;

	// Industrial, commercial and residential buildings have different tiles based on the population density
	if (building->type == Industrial || building->type == Commercial || building->type == Residential)
	{
		if (building->populationDensity >= MAX_POPULATION_DENSITY - 1)
		{
			tile += 48;
		}
		else if (x != 1 || y != 1)
		{
			int index = y * height + x + GetProcAtTile(building->x, building->y);
			bool hasBuilding = building->populationDensity >= pgm_read_byte(&BuildingPopulaceMap[index & 0xf]);

			if (hasBuilding)
			{
				uint8_t procVal = GetProcAtTile(x, y);
				return FIRST_BUILDING_TILE + (procVal & 7);
			}
		}
	}

	tile += y * 16;
	tile += x;
	return tile;
}

// Calculate which visible tile to use
uint8_t CalculateTile(int x, int y)
{
	// Check out of range
	if (x < 0 || y < 0 || x >= MAP_WIDTH || y >= MAP_HEIGHT)
		return 0;

	// First check for buildings
	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		Building* building = &State.buildings[n];

		if (building->type)
		{
			if (x < building->x || y < building->y)
				continue;
			const BuildingInfo* info = GetBuildingInfo(building->type);
			uint8_t width = pgm_read_byte(&info->width);
			uint8_t height = pgm_read_byte(&info->height);
			if (x < building->x + width && y < building->y + height)
			{
				return CalculateBuildingTile(building, x - building->x, y - building->y);
			}
		}
	}

	// Next check for roads / powerlines
	uint8_t connections = GetConnections(x, y);

	if (connections == RoadMask)
	{
		int variant = GetConnectivityTileVariant(x, y, connections);

		if(!IsTerrainClear(x, y))
			return FIRST_ROAD_BRIDGE_TILE + (variant & 1);
    
		if (HasHighTraffic(x, y))
			return FIRST_ROAD_TRAFFIC_TILE + variant;
		return FIRST_ROAD_TILE + variant;
	}
	else if (connections == PowerlineMask)
	{
		int variant = GetConnectivityTileVariant(x, y, connections);

		 if(!IsTerrainClear(x, y))
			return FIRST_POWERLINE_BRIDGE_TILE + (variant & 1);
		
		return FIRST_POWERLINE_TILE + variant;
	}
	else if (connections == (PowerlineMask | RoadMask))
	{
		int variant = GetConnectivityTileVariant(x, y, RoadMask) & 1;
		return FIRST_POWERLINE_ROAD_TILE + variant;
	}

	return GetTerrainTile(x, y);
}

inline uint8_t GetCachedTile(int x, int y)
{
	//// Uncomment to visualise power connectivity
	/*
	if (AnimationFrame & 4)
	{
		x += CachedScrollX;
		y += CachedScrollY;
		int index = y * MAP_WIDTH + x;
		int mask = 1 << (index & 7);
		uint8_t val = GetPowerGrid()[index >> 3];

		return (val & mask) != 0 ? 1 : 0;
	}
	*/
	////


	uint8_t tile = VisibleTileCache[y * VISIBLE_TILES_X + x];

	// Animate water tiles
	if (tile >= FIRST_WATER_TILE && tile <= LAST_WATER_TILE)
	{
		tile = FIRST_WATER_TILE + ((tile - FIRST_WATER_TILE + (AnimationFrame >> 1)) & 3);
	}

	// Animate fire tiles
	if (tile >= FIRST_FIRE_TILE && tile <= LAST_FIRE_TILE)
	{
		tile = FIRST_FIRE_TILE + ((tile - FIRST_FIRE_TILE + (AnimationFrame >> 1)) & 3);
	}

	// Animate traffic tiles
	if ((AnimationFrame & 4) && tile >= FIRST_ROAD_TRAFFIC_TILE && tile <= LAST_ROAD_TRAFFIC_TILE)
	{
		tile += 16;
	}

	return tile;
}

void ResetVisibleTileCache()
{
	CachedScrollX = UIState.scrollX >> TILE_SIZE_SHIFT;
	CachedScrollY = UIState.scrollY >> TILE_SIZE_SHIFT;

	for (int y = 0; y < VISIBLE_TILES_Y; y++)
	{
		for (int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
	}
}

void DrawTiles()
{
	int tileX = 0;
	int offsetX = UIState.scrollX & (TILE_SIZE - 1);

	for (int col = 0; col < DISPLAY_WIDTH; col++)
	{
		int tileY = 0;
		int offsetY = UIState.scrollY & (TILE_SIZE - 1);
		uint8_t currentTile = GetCachedTile(tileX, tileY);
		uint8_t readBuf = pgm_read_byte(&GetTileData(currentTile)[offsetX]);
		readBuf >>= offsetY;

		for (int row = 0; row < DISPLAY_HEIGHT; row++)
		{
			PutPixel(col, row, readBuf & 1);

			offsetY = (offsetY + 1) & 7;
			readBuf >>= 1;

			if (!offsetY)
			{
				tileY++;
				currentTile = GetCachedTile(tileX, tileY);
				readBuf = pgm_read_byte(&GetTileData(currentTile)[offsetX]);
			}
		}

		offsetX = (offsetX + 1) & 7;
		if (!offsetX)
		{
			tileX++;
		}
	}
}

void ScrollUp(int amount)
{
	CachedScrollY -= amount;
	int y = VISIBLE_TILES_Y - 1;

	for (int n = 0; n < VISIBLE_TILES_Y - amount; n++)
	{
		for (int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[(y - amount) * VISIBLE_TILES_X + x];
		}
		y--;
	}

	for (y = 0; y < amount; y++)
	{
		for (int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
	}
}

void ScrollDown(int amount)
{
	CachedScrollY += amount;
	int y = 0;

	for (int n = 0; n < VISIBLE_TILES_Y - amount; n++)
	{
		for (int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[(y + amount) * VISIBLE_TILES_X + x];
		}
		y++;
	}

	y = VISIBLE_TILES_Y - 1;
	for (int n = 0; n < amount; n++)
	{
		for (int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
		y--;
	}
}

void ScrollLeft(int amount)
{
	CachedScrollX -= amount;
	int x = VISIBLE_TILES_X - 1;

	for (int n = 0; n < VISIBLE_TILES_X - amount; n++)
	{
		for (int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[y * VISIBLE_TILES_X + x - amount];
		}
		x--;
	}

	for (x = 0; x < amount; x++)
	{
		for (int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
	}
}

void ScrollRight(int amount)
{
	CachedScrollX += amount;
	int x = 0;

	for (int n = 0; n < VISIBLE_TILES_X - amount; n++)
	{
		for (int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[y * VISIBLE_TILES_X + x + amount];
		}
		x++;
	}

	x = VISIBLE_TILES_X - 1;
	for (int n = 0; n < amount; n++)
	{
		for (int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
		x--;
	}
}

void DrawCursorRect(uint8_t cursorDrawX, uint8_t cursorDrawY, uint8_t cursorWidth, uint8_t cursorHeight)
{
	for (int n = 0; n < cursorWidth; n++)
	{
		uint8_t colour = ((n + AnimationFrame) & 4) != 0 ? 1 : 0;
		PutPixel(cursorDrawX + n, cursorDrawY + cursorHeight - 1, colour);
		PutPixel(cursorDrawX + cursorWidth - n - 1, cursorDrawY, colour);
	}

	for (int n = 0; n < cursorHeight; n++)
	{
		uint8_t colour = ((n + AnimationFrame) & 4) != 0 ? 1 : 0;
		PutPixel(cursorDrawX, cursorDrawY + n, colour);
		PutPixel(cursorDrawX + cursorWidth - 1, cursorDrawY + cursorHeight - n - 1, colour);
	}
}

void DrawCursor()
{
	uint8_t cursorX, cursorY;
	int cursorWidth = TILE_SIZE, cursorHeight = TILE_SIZE;

	if (UIState.brush >= FirstBuildingBrush)
	{
		BuildingType buildingType = (BuildingType)(UIState.brush - FirstBuildingBrush + 1);
		GetBuildingBrushLocation(buildingType, &cursorX, &cursorY);
		const BuildingInfo* buildingInfo = GetBuildingInfo(buildingType);
		cursorWidth *= pgm_read_byte(&buildingInfo->width);
		cursorHeight *= pgm_read_byte(&buildingInfo->height);
	}
	else
	{
		cursorX = UIState.selectX;
		cursorY = UIState.selectY;
	}

	int cursorDrawX, cursorDrawY;
	cursorDrawX = (cursorX * 8) - UIState.scrollX;
	cursorDrawY = (cursorY * 8) - UIState.scrollY;

	if (cursorDrawX >= 0 && cursorDrawY >= 0 && cursorDrawX + cursorWidth < DISPLAY_WIDTH && cursorDrawY + cursorHeight < DISPLAY_HEIGHT)
	{
		DrawCursorRect(cursorDrawX, cursorDrawY, cursorWidth, cursorHeight);
	}
}

void AnimatePowercuts()
{
	bool showPowercut = (AnimationFrame & 8) != 0;

	for (int n = 0; n < MAX_BUILDINGS; n++)
	{
		Building* building = &State.buildings[n];

		if (building->type && building->type != Park && !IsRubble(building->type))
		{
			int screenX = building->x + 1 - CachedScrollX;
			int screenY = building->y + 1 - CachedScrollY;

			if (screenX >= 0 && screenY >= 0 && screenX < VISIBLE_TILES_X && screenY < VISIBLE_TILES_Y)
			{
				if (showPowercut && !building->hasPower)
				{
					VisibleTileCache[screenY * VISIBLE_TILES_X + screenX] = POWERCUT_TILE;
				}
				else
				{
					VisibleTileCache[screenY * VISIBLE_TILES_X + screenX] = CalculateBuildingTile(building, 1, 1);
				}
			}
		}

	}
}

void RefreshTile(uint8_t x, uint8_t y)
{
	int screenX = x - CachedScrollX;
	int screenY = y - CachedScrollY;

	if (screenX >= 0 && screenY >= 0 && screenX < VISIBLE_TILES_X && screenY < VISIBLE_TILES_Y)
	{
		VisibleTileCache[screenY * VISIBLE_TILES_X + screenX] = CalculateTile(x, y);
	}
}

void SetTile(uint8_t x, uint8_t y, uint8_t tile)
{
	int screenX = x - CachedScrollX;
	int screenY = y - CachedScrollY;

	if (screenX >= 0 && screenY >= 0 && screenX < VISIBLE_TILES_X && screenY < VISIBLE_TILES_Y)
	{
		VisibleTileCache[screenY * VISIBLE_TILES_X + screenX] = tile;
	}
}

void RefreshTileAndConnectedNeighbours(uint8_t x, uint8_t y)
{
	RefreshTile(x, y);

	if (x > 0 && GetConnections(x - 1, y))
		RefreshTile(x - 1, y);
	if (x < MAP_WIDTH - 1 && GetConnections(x + 1, y))
		RefreshTile(x + 1, y);
	if (y > 0 && GetConnections(x, y - 1))
		RefreshTile(x, y - 1);
	if (y < MAP_HEIGHT - 1 && GetConnections(x, y + 1))
		RefreshTile(x, y + 1);

}

void RefreshBuildingTiles(Building* building)
{
	const BuildingInfo* info = GetBuildingInfo(building->type);
	uint8_t width = pgm_read_byte(&info->width);
	uint8_t height = pgm_read_byte(&info->height);

	for (int j = 0; j < height; j++)
	{
		uint8_t y = building->y + j;
		for (int i = 0; i < width; i++)
		{
			uint8_t x = building->x + i;
			int screenX = x - CachedScrollX;
			int screenY = y - CachedScrollY;

			if (screenX >= 0 && screenY >= 0 && screenX < VISIBLE_TILES_X && screenY < VISIBLE_TILES_Y)
			{
				VisibleTileCache[screenY * VISIBLE_TILES_X + screenX] = CalculateBuildingTile(building, i, j);
			}
		}
	}

	// Refresh traffic
	uint8_t x1 = building->x > 0 ? building->x - 1 : 0;
	uint8_t x2 = building->x + width < MAP_WIDTH ? building->x + width : MAP_WIDTH - 1;
	uint8_t y1 = building->y > 0 ? building->y - 1 : 0;
	uint8_t y2 = building->y + height < MAP_HEIGHT ? building->y + height : MAP_HEIGHT - 1;

	for (int i = x1; i <= x2; i++)
	{
		if (GetConnections(i, y1) & RoadMask)
			RefreshTile(i, y1);
		if (GetConnections(i, y2) & RoadMask)
			RefreshTile(i, y2);
	}
	for (int i = y1; i <= y2; i++)
	{
		if (GetConnections(x1, i) & RoadMask)
			RefreshTile(x1, i);
		if (GetConnections(x2, i) & RoadMask)
			RefreshTile(x2, i);
	}

}

void DrawTileAt(uint8_t tile, int x, int y)
{
	for (int col = 0; col < TILE_SIZE; col++)
	{
		uint8_t readBuf = pgm_read_byte(&GetTileData(tile)[col]);

		for (int row = 0; row < TILE_SIZE; row++)
		{
			uint8_t colour = readBuf & 1;
			readBuf >>= 1;
			PutPixel(x + col, y + row, colour);
		}
	}
}

void DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour)
{
	for (int j = 0; j < h; j++)
	{
		for (int i = 0; i < w; i++)
		{
			PutPixel(x + i, y + j, colour);
		}
	}
}

void DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour)
{
	for (int j = 0; j < w; j++)
	{
		PutPixel(x + j, y, colour);
		PutPixel(x + j, y + h - 1, colour);
	}

	for (int j = 1; j < h - 1; j++)
	{
		PutPixel(x, y + j, colour);
		PutPixel(x + w - 1, y + j, colour);
	}
}

const char FireReportedStr[] PROGMEM = "Fire reported!";

void DrawUI()
{
	if (UIState.state == ShowingToolbar)
	{
		uint8_t buttonX = 1;

		DrawFilledRect(0, DISPLAY_HEIGHT - TILE_SIZE - 2, NUM_TOOLBAR_BUTTONS * (TILE_SIZE + 1) + 2, TILE_SIZE + 2, 1);
		DrawFilledRect(0, DISPLAY_HEIGHT - TILE_SIZE - 2 - FONT_HEIGHT - 1, DISPLAY_WIDTH / 2 + FONT_WIDTH + 1, FONT_HEIGHT + 2, 1);

		for (int n = 0; n < NUM_TOOLBAR_BUTTONS; n++)
		{
			DrawTileAt(FIRST_BRUSH_TILE + n, buttonX, DISPLAY_HEIGHT - TILE_SIZE - 1);
			buttonX += TILE_SIZE + 1;
		}

		DrawCursorRect(UIState.selection * (TILE_SIZE + 1), DISPLAY_HEIGHT - TILE_SIZE - 2, TILE_SIZE + 2, TILE_SIZE + 2);
		const char* currentSelection = GetToolbarString(UIState.selection);
		DrawString(currentSelection, 1, DISPLAY_HEIGHT - FONT_HEIGHT - TILE_SIZE - 2);

		uint16_t cost = 0;

		switch (UIState.selection)
		{
		case 0: cost = BULLDOZER_COST; break;
		case 1: cost = ROAD_COST; break;
		case 2: cost = POWERLINE_COST; break;
		default:
		{
			int buildingIndex = 1 + UIState.selection - FirstBuildingBrush;
			if (buildingIndex < Num_BuildingTypes)
			{
				const BuildingInfo* buildingInfo = GetBuildingInfo(buildingIndex);
				cost = pgm_read_word(&buildingInfo->cost);
			}
		}
		break;
		}
		if (cost > 0)
		{
			DrawCurrency(cost, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT - FONT_HEIGHT - TILE_SIZE - 2);
		}
	}
	else if (UIState.state == InGameDisaster)
	{
		int strLen = strlen_P(FireReportedStr);
		int x = DISPLAY_WIDTH / 2 - strLen * (FONT_WIDTH / 2);
		DrawFilledRect(x - 1, DISPLAY_HEIGHT - TILE_SIZE - 2, 2 + strlen_P(FireReportedStr) * FONT_WIDTH + 2, TILE_SIZE + 2, 1);

		if (UIState.selection & 4)
		{
			DrawString(FireReportedStr, x, DISPLAY_HEIGHT - FONT_HEIGHT - 1);
		}
	}
	else
	{
		// Current brush at bottom left
		const char* currentSelection = GetToolbarString(UIState.brush);
		DrawFilledRect(0, DISPLAY_HEIGHT - TILE_SIZE - 2, TILE_SIZE + 2 + strlen_P(currentSelection) * FONT_WIDTH + 2, TILE_SIZE + 2, 1);
		DrawTileAt(FIRST_BRUSH_TILE + UIState.brush, 1, DISPLAY_HEIGHT - TILE_SIZE - 1);
		DrawString(currentSelection, TILE_SIZE + 2, DISPLAY_HEIGHT - FONT_HEIGHT - 1);
	}

	// Date at top left
	DrawFilledRect(0, 0, FONT_WIDTH * 8 + 2, FONT_HEIGHT + 2, 1);
	DrawString(GetMonthString(State.month), 1, 1);
	DrawInt(State.year + 1900, FONT_WIDTH * 4 + 1, 1);

	// Funds at top right
	uint8_t currencyStrLen = DrawCurrency(State.money, DISPLAY_WIDTH - FONT_WIDTH - 1, 1);
	DrawRect(DISPLAY_WIDTH - 2 - currencyStrLen * FONT_WIDTH, 0, currencyStrLen * FONT_WIDTH + 2, FONT_HEIGHT + 2, 1);
}

void DrawInGame()
{
	// Check to see if scrolled to a new location and need to update the visible tile cache
	int tileScrollX = UIState.scrollX >> TILE_SIZE_SHIFT;
	int tileScrollY = UIState.scrollY >> TILE_SIZE_SHIFT;
	int scrollDiffX = tileScrollX - CachedScrollX;
	int scrollDiffY = tileScrollY - CachedScrollY;

	if (scrollDiffX < 0)
	{
		if (scrollDiffX > -VISIBLE_TILES_X)
		{
			ScrollLeft(-scrollDiffX);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}
	else if (scrollDiffX > 0)
	{
		if (scrollDiffX < VISIBLE_TILES_X)
		{
			ScrollRight(scrollDiffX);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}

	if (scrollDiffY < 0)
	{
		if (scrollDiffY > -VISIBLE_TILES_Y)
		{
			ScrollUp(-scrollDiffY);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}
	else if (scrollDiffY > 0)
	{
		if (scrollDiffY < VISIBLE_TILES_Y)
		{
			ScrollDown(scrollDiffY);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}

	AnimatePowercuts();

	DrawTiles();

	if (UIState.state == InGame || UIState.state == InGameDisaster)
	{
		DrawCursor();
	}

	DrawUI();
}

const char SaveCityStr[] PROGMEM = "Save City";
const char LoadCityStr[] PROGMEM = "Load City";
const char NewCityStr[] PROGMEM = "New City";
const char AutoBudgetStr[] PROGMEM = "Auto Budget:";
const char OnStr[] PROGMEM = "On";
const char OffStr[] PROGMEM = "Off";
const char TwitterStr[] PROGMEM = "by @jameshhoward";

void DrawSaveLoadMenu()
{
	const int menuWidth = 68;
	const int menuHeight = 50;
	const int spacing = 10;
	DrawRect(DISPLAY_WIDTH / 2 - menuWidth / 2 + 1, DISPLAY_HEIGHT / 2 - menuHeight / 2 + 1, menuWidth, menuHeight, 0);
	DrawFilledRect(DISPLAY_WIDTH / 2 - menuWidth / 2, DISPLAY_HEIGHT / 2 - menuHeight / 2, menuWidth, menuHeight, 1);
	DrawRect(DISPLAY_WIDTH / 2 - menuWidth / 2, DISPLAY_HEIGHT / 2 - menuHeight / 2, menuWidth, menuHeight, 0);

	uint8_t y = DISPLAY_HEIGHT / 2 - menuHeight / 2 + spacing / 2 + 2;
	uint8_t x = DISPLAY_WIDTH / 2 - menuWidth / 2 + FONT_WIDTH;

	uint8_t cursorRectY = y + spacing * UIState.selection - 2;
	DrawCursorRect(x - 2, cursorRectY, menuWidth - FONT_WIDTH * 2 + 4, FONT_HEIGHT + 4);

	DrawString(SaveCityStr, x, y);
	y += spacing;
	DrawString(LoadCityStr, x, y);
	y += spacing;
	DrawString(NewCityStr, x, y);
	y += spacing;
	DrawString(AutoBudgetStr, x, y);

	DrawString(UIState.autoBudget ? OnStr : OffStr, x + 12 * FONT_WIDTH, y);
}

void DrawStartScreen()
{
	const uint8_t logoWidth = 72;
	const uint8_t logoHeight = 40;
	const uint8_t logoY = DISPLAY_HEIGHT / 2 - 31;
	const int spacing = 9;

	DrawFilledRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
	DrawFilledRect(DISPLAY_WIDTH / 2 - logoWidth / 2, logoY, logoWidth, logoHeight, 0);
	DrawBitmap(LogoBitmap, DISPLAY_WIDTH / 2 - logoWidth / 2, logoY, logoWidth, logoHeight);

	uint8_t y = logoY + logoHeight - 2;
	uint8_t x = DISPLAY_WIDTH / 2 - FONT_WIDTH * 5;

	uint8_t cursorRectY = y + spacing * UIState.selection - 2;
	DrawCursorRect(x - 2, cursorRectY, FONT_WIDTH * 10 + 4, FONT_HEIGHT + 4);

	DrawString(NewCityStr, x, y);
	y += spacing;
	DrawString(LoadCityStr, x, y);
	y += spacing;
	DrawString(TwitterStr, x - FONT_WIDTH * 3, y);
}

const char LeftArrowStr[] PROGMEM = "<";
const char RightArrowStr[] PROGMEM = ">";

void DrawNewCityMenu()
{
	const uint8_t mapY = DISPLAY_HEIGHT / 2 - MAP_HEIGHT / 2 - 4;
	DrawFilledRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);

	DrawFilledRect(DISPLAY_WIDTH / 2 - MAP_WIDTH / 2, mapY, MAP_WIDTH, MAP_HEIGHT, 0);
	DrawBitmap(GetTerrainData(State.terrainType), DISPLAY_WIDTH / 2 - MAP_WIDTH / 2, mapY, MAP_WIDTH, MAP_HEIGHT);
	DrawRect(DISPLAY_WIDTH / 2 - MAP_WIDTH / 2 - 2, mapY - 2, MAP_WIDTH + 4, MAP_HEIGHT + 4, 0);

	DrawString(GetTerrainDescription(State.terrainType), DISPLAY_WIDTH / 2 - FONT_WIDTH * 3, mapY + MAP_HEIGHT + 5);
	DrawString(LeftArrowStr, DISPLAY_WIDTH / 2 - MAP_WIDTH / 2 - 6 - FONT_WIDTH, DISPLAY_HEIGHT / 2 - FONT_HEIGHT / 2);
	DrawString(RightArrowStr, DISPLAY_WIDTH / 2 + MAP_WIDTH / 2 + 6, DISPLAY_HEIGHT / 2 - FONT_HEIGHT / 2);
}

const char BudgetHeaderStr[] PROGMEM =		"Budget report for";
const char TaxRateStr[] PROGMEM =			"Tax rate         <   % >";
const char TaxesCollectedStr[] PROGMEM =	"Taxes collected";
const char PoliceBudgetStr[] PROGMEM =		"Police budget";
const char FireBudgetStr[] PROGMEM =		"Fire budget";
const char RoadBudgetStr[] PROGMEM =		"Road budget";
const char CashFlowStr[] PROGMEM =			"Cash flow";

void DrawBudgetMenu()
{
	const int menuWidth = 100;
	const int menuHeight = 56;
	const int spacing = FONT_HEIGHT + 1;
	DrawRect(DISPLAY_WIDTH / 2 - menuWidth / 2 + 1, DISPLAY_HEIGHT / 2 - menuHeight / 2 + 1, menuWidth, menuHeight, 0);
	DrawFilledRect(DISPLAY_WIDTH / 2 - menuWidth / 2, DISPLAY_HEIGHT / 2 - menuHeight / 2, menuWidth, menuHeight, 1);
	DrawRect(DISPLAY_WIDTH / 2 - menuWidth / 2, DISPLAY_HEIGHT / 2 - menuHeight / 2, menuWidth, menuHeight, 0);

	uint8_t y = DISPLAY_HEIGHT / 2 - menuHeight / 2 + 2;
	uint8_t x = DISPLAY_WIDTH / 2 - menuWidth / 2 + 2;
	uint8_t x2 = DISPLAY_WIDTH / 2 + menuWidth / 2 - 2 - FONT_WIDTH;

	DrawString(BudgetHeaderStr, x, y);
	int year = State.year > 0 ? State.year + 1899 : 1900;
	DrawInt(year, x + FONT_WIDTH * 18, y);
	y += spacing + 2;

	DrawString(TaxRateStr, x, y);
	DrawInt(State.taxRate, x + FONT_WIDTH * 19, y);
	y += spacing;

	DrawString(TaxesCollectedStr, x, y);
	DrawCurrency(State.taxesCollected, x2, y);
	y += spacing;

	DrawString(FireBudgetStr, x, y);
	DrawCurrency(State.fireBudget * FIRE_AND_POLICE_MAINTENANCE_COST, x2, y);
	y += spacing;

	DrawString(PoliceBudgetStr, x, y);
	DrawCurrency(State.policeBudget * FIRE_AND_POLICE_MAINTENANCE_COST, x2, y);
	y += spacing;

	DrawString(RoadBudgetStr, x, y);
	DrawCurrency(State.roadBudget, x2, y);
	y += spacing + 2;

	DrawString(CashFlowStr, x, y);
	DrawCurrency(State.taxesCollected - State.roadBudget - State.policeBudget * FIRE_AND_POLICE_MAINTENANCE_COST - State.fireBudget * FIRE_AND_POLICE_MAINTENANCE_COST, x2, y);
	y += spacing;

	if (UIState.selection < MIN_BUDGET_DISPLAY_TIME)
	{
		UIState.selection++;
	}
}

void Draw()
{
	switch (UIState.state)
	{
	case StartScreen:
		DrawStartScreen();
		break;
	case NewCityMenu:
		DrawNewCityMenu();
		break;
	case InGame:
	case InGameDisaster:
	case ShowingToolbar:
		DrawInGame();
		break;
	case SaveLoadMenu:
		DrawSaveLoadMenu();
		break;
	case BudgetMenu:
		DrawBudgetMenu();
		break;
	}

	AnimationFrame++;

}
