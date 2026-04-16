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
    TILE_ITEM_SPAWN = 5
} TileType;

class DynamicMap
{
private:
    uint8_t width;
    uint8_t height;
    TileType tiles[MAP_HEIGHT][MAP_WIDTH];
    const char *name;

public:
    DynamicMap(const char *name, uint8_t w, uint8_t h, bool addBorder = true, float height = 5.0f, float depth = 0.2f);
    ~DynamicMap();

    void addBorderWalls();                                                                // Add walls around the entire map border
    void addCorridor(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);                     // Add a simple L-shaped corridor between two points
    void addDoor(uint8_t x, uint8_t y);                                                   // Add a door tile at the specified coordinates
    void addHorizontalWall(uint8_t x1, uint8_t x2, uint8_t y, TileType type = TILE_WALL); // Add a horizontal wall segment
    void addRoom(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool add_walls = true);  // Add a rectangular room defined by top-left (x1, y1) and bottom-right (x2, y2) corners, optionally adding walls around it
    void addVerticalWall(uint8_t x, uint8_t y1, uint8_t y2, TileType type = TILE_WALL);   // Add a vertical wall segment
    uint8_t getBlockAt(uint8_t x, uint8_t y) const;                                       // Get the block type at the specified coordinates, returns 0xF for solid blocks (walls/doors) and 0x0 for empty space
    uint8_t getHeight() const { return height; }                                          // Get the height of the map in tiles
    void getMiniMap(uint8_t output[MAP_HEIGHT][MAP_WIDTH]) const;                         // Get a 2D array representation of the map for minimap rendering
    const char *getName() const { return name; }                                          // Get the name of the map
    TileType getTile(uint8_t x, uint8_t y) const;                                         // Get the tile type at the specified coordinates, returns TILE_EMPTY if out of bounds
    uint8_t getWidth() const { return width; }                                            // Get the width of the map in tiles
    void setTile(uint8_t x, uint8_t y, TileType type);                                    // Set a tile type at the specified coordinates
};
