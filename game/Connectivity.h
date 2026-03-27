#pragma once

// Connectivity refers to roads + power lines

enum ConnectivityMask
{
	RoadMask = 1,
	PowerlineMask = 2
};

uint8_t GetConnections(int x, int y);
void SetConnections(int x, int y, uint8_t newVal);
void CalculatePowerConnectivity(void);
int GetConnectivityTileVariant(int x, int y, uint8_t mask);
bool IsSuitableForBridgedTile(int x, int y, uint8_t mask);
uint8_t* GetPowerGrid();
