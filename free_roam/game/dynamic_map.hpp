#pragma once
#include "furi.h"
#include "engine/draw.hpp"
#include "engine/sprite3d.hpp"

#define MAX_MAP_WIDTH 64
#define MAX_MAP_HEIGHT 64
#define MAX_RENDER_WALLS 100

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
    TileType tiles[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    const char *name;
    Sprite3D *renderWalls[MAX_RENDER_WALLS];
    uint8_t renderWallCount;

public:
    DynamicMap(const char *name, uint8_t w, uint8_t h, bool addBorder = true);
    ~DynamicMap();

    void addBorderWalls(float height = 5.0f, float depth = 0.2f);                                                                  // Add walls around the entire map border
    void addCorridor(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);                                                              // Add a simple L-shaped corridor between two points
    void addDoor(uint8_t x, uint8_t y);                                                                                            // Add a door tile at the specified coordinates
    void addHorizontalWall(uint8_t x1, uint8_t x2, uint8_t y, float height = 5.0f, float depth = 0.2f, TileType type = TILE_WALL); // Add a horizontal wall and create a corresponding Sprite3D for rendering if it's a solid wall
    void addRoom(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, float height = 5.0f, float depth = 0.2f, bool add_walls = true);  // Add a rectangular room defined by top-left (x1, y1) and bottom-right (x2, y2) corners, optionally adding walls around it
    void addVerticalWall(uint8_t x, uint8_t y1, uint8_t y2, float height = 5.0f, float depth = 0.2f, TileType type = TILE_WALL);   // Add a vertical wall and create a corresponding Sprite3D for rendering if it's a solid wall
    uint8_t getBlockAt(uint8_t x, uint8_t y) const;                                                                                // Get the block type at the specified coordinates, returns 0xF for solid blocks (walls/doors) and 0x0 for empty space
    uint8_t getHeight() const { return height; }                                                                                   // Get the height of the map in tiles
    const Sprite3D *getRenderWall(uint8_t i) const { return renderWalls[i]; }                                                      // Get the Sprite3D object for the i-th render wall, returns nullptr if index is out of bounds
    uint8_t getRenderWallCount() const { return renderWallCount; }                                                                 // Get the number of render walls currently in the map
    uint8_t releaseRenderWalls(Sprite3D **out, uint8_t maxCount);                                                                  // Transfer ownership of all render-wall Sprite3D pointers to caller; clears internal list so destructor won't delete them
    void getMiniMap(uint8_t output[MAX_MAP_HEIGHT][MAX_MAP_WIDTH]) const;                                                          // Get a 2D array representation of the map for minimap rendering
    const char *getName() const { return name; }                                                                                   // Get the name of the map
    TileType getTile(uint8_t x, uint8_t y) const;                                                                                  // Get the tile type at the specified coordinates, returns TILE_EMPTY if out of bounds
    uint8_t getWidth() const { return width; }                                                                                     // Get the width of the map in tiles

    // Draw the minimap onto canvas at pixel position
    void renderMiniMap(Draw *const canvas, Vector position, Vector size,
                       Vector player_pos, Vector player_dir = Vector(0, 0), uint16_t foreground_color = 0x0000, uint16_t background_color = 0xFFFF) const;

    void setTile(uint8_t x, uint8_t y, TileType type); // Set a tile type at the specified coordinates
};
