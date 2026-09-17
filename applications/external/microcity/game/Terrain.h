#pragma once

uint8_t GetTerrainTile(int x, int y);
bool IsTerrainClear(int x, int y);

const char* GetTerrainDescription(uint8_t index);
const uint8_t* GetTerrainData(uint8_t index);
