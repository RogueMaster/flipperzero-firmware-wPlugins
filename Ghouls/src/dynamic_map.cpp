#include "dynamic_map.hpp"
#include "pico-game-engine/engine/vector.hpp"

DynamicMap::DynamicMap(uint8_t w, uint8_t h, bool addBorder)
    : height(h), tileData(nullptr), width(w)
{
    tileData = ENGINE_MEM_NEW TileType[w * h]();
    if (!tileData)
    {
        ENGINE_LOG_INFO("[DynamicMap] Failed to allocate tile data\n");
        return;
    }

    if (addBorder)
        addBorderWalls();
}

DynamicMap::~DynamicMap()
{
    ENGINE_MEM_DELETE[] tileData;
}

bool DynamicMap::resize(uint8_t newW, uint8_t newH)
{
    TileType *newData = ENGINE_MEM_NEW TileType[newW * newH]();
    if (!newData)
    {
        ENGINE_LOG_INFO("[DynamicMap] resize: failed to allocate new tile data\n");
        return false;
    }

    uint8_t copyW = (newW < width) ? newW : width;
    uint8_t copyH = (newH < height) ? newH : height;
    for (uint8_t y = 0; y < copyH; y++)
        for (uint8_t x = 0; x < copyW; x++)
            newData[y * newW + x] = tileData[y * width + x];

    ENGINE_MEM_DELETE[] tileData;

    tileData = newData;
    width = newW;
    height = newH;
    return true;
}

TileType DynamicMap::getTile(uint8_t x, uint8_t y) const
{
    if (!tileData || x >= width || y >= height)
        return TILE_EMPTY;
    return tileData[y * width + x];
}

void DynamicMap::setTile(uint8_t x, uint8_t y, TileType type)
{
    if (tileData && x < width && y < height)
        tileData[y * width + x] = type;
}

void DynamicMap::addBorderWalls()
{
    addHorizontalWall(0, width - 1, 0, TILE_WALL);          // top
    addHorizontalWall(0, width - 1, height - 1, TILE_WALL); // bottom
    addVerticalWall(0, 0, height - 1, TILE_WALL);           // left
    addVerticalWall(width - 1, 0, height - 1, TILE_WALL);   // right
}

void DynamicMap::addHorizontalWall(uint8_t x1, uint8_t x2, uint8_t y, TileType type)
{
    for (uint8_t x = x1; x <= x2; x++)
        setTile(x, y, type);
}

void DynamicMap::addVerticalWall(uint8_t x, uint8_t y1, uint8_t y2, TileType type)
{
    for (uint8_t y = y1; y <= y2; y++)
        setTile(x, y, type);
}

void DynamicMap::addRoom(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool add_walls)
{
    for (uint8_t y = y1; y <= y2; y++)
        for (uint8_t x = x1; x <= x2; x++)
            setTile(x, y, TILE_EMPTY);

    if (add_walls)
    {
        addHorizontalWall(x1, x2, y1, TILE_WALL);
        addHorizontalWall(x1, x2, y2, TILE_WALL);
        addVerticalWall(x1, y1, y2, TILE_WALL);
        addVerticalWall(x2, y1, y2, TILE_WALL);
    }
}

void DynamicMap::addCorridor(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    if (x1 == x2)
    {
        uint8_t s = (y1 < y2) ? y1 : y2;
        uint8_t e = (y1 < y2) ? y2 : y1;
        for (uint8_t y = s; y <= e; y++)
            setTile(x1, y, TILE_EMPTY);
    }
    else if (y1 == y2)
    {
        uint8_t s = (x1 < x2) ? x1 : x2;
        uint8_t e = (x1 < x2) ? x2 : x1;
        for (uint8_t x = s; x <= e; x++)
            setTile(x, y1, TILE_EMPTY);
    }
    else
    {
        // L-shaped: horizontal leg first, then vertical
        uint8_t sx = (x1 < x2) ? x1 : x2;
        uint8_t ex = (x1 < x2) ? x2 : x1;
        for (uint8_t x = sx; x <= ex; x++)
            setTile(x, y1, TILE_EMPTY);

        uint8_t sy = (y1 < y2) ? y1 : y2;
        uint8_t ey = (y1 < y2) ? y2 : y1;
        for (uint8_t y = sy; y <= ey; y++)
            setTile(x2, y, TILE_EMPTY);
    }
}

void DynamicMap::addDoor(uint8_t x, uint8_t y)
{
    setTile(x, y, TILE_DOOR);
}

bool DynamicMap::getMiniMap(TileType **output, uint8_t w, uint8_t h) const
{
    if (!output)
        return false;

    for (uint8_t y = 0; y < h; y++)
        for (uint8_t x = 0; x < w; x++)
            output[y][x] = (x < width && y < height) ? tileData[y * width + x] : TILE_EMPTY;

    return true;
}