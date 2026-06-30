#pragma once
#include "dynamic_map.hpp"
#include <memory>

#define WALL_HEIGHT 2.0f
#define WALL_DEPTH 0.2f

inline std::unique_ptr<DynamicMap> mapsFirst()
{
    // Town: huge open area with 4 houses placed as 3D sprites
    auto map = std::make_unique<DynamicMap>("First", 64, 57, false);

    // Outer border
    map->addHorizontalWall(0, 63, 0, WALL_HEIGHT, WALL_DEPTH);  // Top
    map->addHorizontalWall(0, 63, 56, WALL_HEIGHT, WALL_DEPTH); // Bottom
    map->addVerticalWall(0, 0, 56, WALL_HEIGHT, WALL_DEPTH);    // Left
    map->addVerticalWall(63, 0, 56, WALL_HEIGHT, WALL_DEPTH);   // Right

    // Teleport zone in the center
    map->setTile(31, 28, TILE_TELEPORT);
    map->setTile(32, 28, TILE_TELEPORT);
    map->setTile(31, 29, TILE_TELEPORT);

    return map;
}

inline std::unique_ptr<DynamicMap> mapsSecond()
{
    // Forest: huge open area with trees placed as 3D sprites
    auto map = std::make_unique<DynamicMap>("Second", 64, 57, false);

    // Outer border
    map->addHorizontalWall(0, 63, 0, WALL_HEIGHT, WALL_DEPTH);  // Top
    map->addHorizontalWall(0, 63, 56, WALL_HEIGHT, WALL_DEPTH); // Bottom
    map->addVerticalWall(0, 0, 56, WALL_HEIGHT, WALL_DEPTH);    // Left
    map->addVerticalWall(63, 0, 56, WALL_HEIGHT, WALL_DEPTH);   // Right

    // Teleport zone in the center
    map->setTile(31, 28, TILE_TELEPORT);
    map->setTile(32, 28, TILE_TELEPORT);
    map->setTile(31, 29, TILE_TELEPORT);

    return map;
}

inline std::unique_ptr<DynamicMap> mapsOnline()
{
    // Multiplayer gathering town: 64x57 open area with border walls.
    // Houses and trees are Sprite3D entities
    // not tile walls — players can walk around them.
    auto map = std::make_unique<DynamicMap>("Online", 64, 57, false);

    // Border walls
    map->addHorizontalWall(0, 63, 0, WALL_HEIGHT, WALL_DEPTH);  // Top
    map->addHorizontalWall(0, 63, 56, WALL_HEIGHT, WALL_DEPTH); // Bottom
    map->addVerticalWall(0, 0, 56, WALL_HEIGHT, WALL_DEPTH);    // Left
    map->addVerticalWall(63, 0, 56, WALL_HEIGHT, WALL_DEPTH);   // Right

    // No teleport tiles — online world has no level transitions.

    return map;
}

inline std::unique_ptr<DynamicMap> mapsTutorial(uint8_t width = 20, uint8_t height = 20)
{
    auto map = std::make_unique<DynamicMap>("Tutorial", width, height, false);

    // Room built from explicit walls:
    //   3 long walls: top, bottom, left
    //   2 short walls on the right side with a gap in the middle for the door
    uint8_t mid = height / 2;
    uint8_t gap = 2; // half-width of the door opening

    map->addHorizontalWall(0, width - 1, 0, WALL_HEIGHT, WALL_DEPTH);                    // Top wall
    map->addHorizontalWall(0, width - 1, height - 1, WALL_HEIGHT, WALL_DEPTH);           // Bottom wall
    map->addVerticalWall(0, 0, height - 1, WALL_HEIGHT, WALL_DEPTH);                     // Left wall
    map->addVerticalWall(width - 1, 0, mid - gap - 1, WALL_HEIGHT, WALL_DEPTH);          // Right wall (above door)
    map->addVerticalWall(width - 1, mid + gap + 1, height - 1, WALL_HEIGHT, WALL_DEPTH); // Right wall (below door)

    // Teleport tiles at the door opening (right edge, gap rows)
    for (uint8_t dy = 0; dy <= gap * 2; dy++)
    {
        map->setTile(width - 1, mid - gap + dy, TILE_TELEPORT);
    }

    return map;
}
