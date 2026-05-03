#pragma once
#include "config.hpp"
#include "general.hpp"

typedef enum
{
    TILE_EMPTY = 0,
    TILE_WALL = 1,
    TILE_DOOR = 2,
    TILE_TELEPORT = 3,
    TILE_ENEMY_SPAWN = 4,
    TILE_ITEM_SPAWN = 5,
    TILE_HOUSE = 6,
    TILE_TREE = 7,
} TileType;

class DynamicMap
{
private:
    uint8_t height;
    TileType *tileData;
    uint8_t width;

public:
    DynamicMap(uint8_t w, uint8_t h, bool addBorder = true);
    ~DynamicMap();

    void addBorderWalls();
    void addCorridor(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
    void addDoor(uint8_t x, uint8_t y);
    void addHorizontalWall(uint8_t x1, uint8_t x2, uint8_t y, TileType type = TILE_WALL);
    void addRoom(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool add_walls = true);
    void addVerticalWall(uint8_t x, uint8_t y1, uint8_t y2, TileType type = TILE_WALL);

    uint8_t getHeight() const { return height; }
    bool getMiniMap(TileType **output, uint8_t w, uint8_t h) const;
    uint8_t getWidth() const { return width; }

    TileType getTile(uint8_t x, uint8_t y) const;
    bool resize(uint8_t newW, uint8_t newH);
    void setTile(uint8_t x, uint8_t y, TileType type);
};