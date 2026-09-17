#pragma once

#include <stdint.h>

#include "Building.h"

void PutPixel(uint8_t x, uint8_t y, uint8_t colour);
void DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour);
void DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour);
void DrawBitmap(const uint8_t* bmp, uint8_t x, uint8_t y, uint8_t w, uint8_t h);

void Draw(void);

void ResetVisibleTileCache(void);
void RefreshBuildingTiles(Building* building);
void RefreshTile(uint8_t x, uint8_t y);
void RefreshTileAndConnectedNeighbours(uint8_t x, uint8_t y);

void SetTile(uint8_t x, uint8_t y, uint8_t tile);
