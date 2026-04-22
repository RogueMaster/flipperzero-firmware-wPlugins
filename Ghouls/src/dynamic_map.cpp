#include "dynamic_map.hpp"
#include "pico-game-engine/engine/vector.hpp"

DynamicMap::DynamicMap(const char *name, uint8_t w, uint8_t h, bool addBorder, float height, float depth) : width(w), height(h), name(name)
{
    (void)height;
    (void)depth;
    memset(tiles, 0, sizeof(tiles));
    if (addBorder)
    {
        this->addBorderWalls();
    }
}

DynamicMap::~DynamicMap()
{
}

void DynamicMap::addBorderWalls()
{
    // Add walls around the entire map border
    addHorizontalWall(0, width - 1, 0, TILE_WALL);          // Top border
    addHorizontalWall(0, width - 1, height - 1, TILE_WALL); // Bottom border
    addVerticalWall(0, 0, height - 1, TILE_WALL);           // Left border
    addVerticalWall(width - 1, 0, height - 1, TILE_WALL);   // Right border
}

void DynamicMap::addCorridor(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    // Simple L-shaped corridor
    if (x1 == x2)
    {
        // Vertical corridor
        uint8_t start_y = (y1 < y2) ? y1 : y2;
        uint8_t end_y = (y1 < y2) ? y2 : y1;
        for (uint8_t y = start_y; y <= end_y; y++)
        {
            setTile(x1, y, TILE_EMPTY);
        }
    }
    else if (y1 == y2)
    {
        // Horizontal corridor
        uint8_t start_x = (x1 < x2) ? x1 : x2;
        uint8_t end_x = (x1 < x2) ? x2 : x1;
        for (uint8_t x = start_x; x <= end_x; x++)
        {
            setTile(x, y1, TILE_EMPTY);
        }
    }
    else
    {
        // L-shaped corridor (horizontal then vertical)
        uint8_t start_x = (x1 < x2) ? x1 : x2;
        uint8_t end_x = (x1 < x2) ? x2 : x1;
        for (uint8_t x = start_x; x <= end_x; x++)
        {
            setTile(x, y1, TILE_EMPTY);
        }

        uint8_t start_y = (y1 < y2) ? y1 : y2;
        uint8_t end_y = (y1 < y2) ? y2 : y1;
        for (uint8_t y = start_y; y <= end_y; y++)
        {
            setTile(x2, y, TILE_EMPTY);
        }
    }
}

void DynamicMap::addDoor(uint8_t x, uint8_t y)
{
    setTile(x, y, TILE_DOOR);
}

void DynamicMap::addHorizontalWall(uint8_t x1, uint8_t x2, uint8_t y, TileType type)
{
    for (uint8_t x = x1; x <= x2; x++)
    {
        setTile(x, y, type);
    }
}

void DynamicMap::addRoom(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool add_walls)
{
    // Clear the room area
    for (uint8_t y = y1; y <= y2; y++)
    {
        for (uint8_t x = x1; x <= x2; x++)
        {
            setTile(x, y, TILE_EMPTY);
        }
    }

    // Add walls around the room
    if (add_walls)
    {
        addHorizontalWall(x1, x2, y1, TILE_WALL); // Top wall
        addHorizontalWall(x1, x2, y2, TILE_WALL); // Bottom wall
        addVerticalWall(x1, y1, y2, TILE_WALL);   // Left wall
        addVerticalWall(x2, y1, y2, TILE_WALL);   // Right wall
    }
}

void DynamicMap::addVerticalWall(uint8_t x, uint8_t y1, uint8_t y2, TileType type)
{
    for (uint8_t y = y1; y <= y2; y++)
    {
        setTile(x, y, type);
    }
}

void DynamicMap::getMiniMap(uint8_t output[MAP_HEIGHT][MAP_WIDTH]) const
{
    for (uint8_t y = 0; y < MAP_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < MAP_WIDTH; x++)
        {
            if (x < width && y < height)
                output[y][x] = (uint8_t)tiles[y][x];
            else
                output[y][x] = 0;
        }
    }
}

TileType DynamicMap::getTile(uint8_t x, uint8_t y) const
{
    if (x >= width || y >= height)
    {
        return TILE_EMPTY; // Out of bounds = empty (no walls)
    }
    return tiles[y][x];
}

void DynamicMap::setTile(uint8_t x, uint8_t y, TileType type)
{
    if (x < width && y < height)
    {
        tiles[y][x] = type;
    }
}