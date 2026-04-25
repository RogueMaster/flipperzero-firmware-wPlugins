#pragma once
#include "pico-game-engine/engine/level.hpp"
#include "pico-game-engine/engine/sprite3d.hpp"
#include "dynamic_map.hpp"
#include "general.hpp"

#if SKY_RENDER_ALLOWED
#include "sky.hpp"
#endif

#if GROUND_RENDER_ALLOWED
#include "ground.hpp"
#endif

// Index encoding:
//   0 .. HOUSE_SPAWN_COUNT-1                              -> house
//   HOUSE_SPAWN_COUNT .. RENDER_WALL_OFFSET-1             -> tree
//   RENDER_WALL_OFFSET .. RENDER_ENTITY_OFFSET-1          -> wall segment
//   RENDER_ENTITY_OFFSET .. 255                           -> level entity
#define RENDER_TREE_OFFSET HOUSE_SPAWN_COUNT
#define RENDER_WALL_OFFSET (HOUSE_SPAWN_COUNT + TREE_SPAWN_COUNT)
#define RENDER_ENTITY_OFFSET (HOUSE_SPAWN_COUNT + TREE_SPAWN_COUNT + WALL_SEGMENT_COUNT)
#define MAX_RENDER_ITEMS (HOUSE_SPAWN_COUNT + TREE_SPAWN_COUNT + WALL_SEGMENT_COUNT + 32)

class GhoulsGame;

class GhoulsLevel : public Level
{
public:
    GhoulsLevel(const char *name, const Vector &size, Game *game, GhoulsGame *ghoulsGame);
    ~GhoulsLevel();
    bool collisionMapCheck(Vector new_position);
#if GROUND_RENDER_ALLOWED
    Ground *getGround() const { return ground; }
#endif
#if SKY_RENDER_ALLOWED
    Sky *getSky() const { return sky; }
#endif
    bool isPositionAvailable(Vector position);
    virtual void render(Game *game) override;
    void renderMiniMap(Draw *canvas);
    void renderMiniatureMiniMap(Draw *canvas);
    virtual void update(Game *game) override;

private:
    bool initializeSprites();
    void registerSpritePositionsOnMap(DynamicMap *map);

    static const Vector housePositions[HOUSE_SPAWN_COUNT];
    static const Vector treePositions[TREE_SPAWN_COUNT];
    static const Vector wallPositions[MAP_OUTER_WALLS];
    static const Vector wallSegmentPositions[WALL_SEGMENT_COUNT];

    DynamicMap *currentDynamicMap = nullptr; // current dynamic map
    GhoulsGame *ghoulsGame = nullptr;

#if GROUND_RENDER_ALLOWED
    Ground *ground = nullptr; // Ground instance for rendering the ground
#endif

    Sprite3D *houseSprite = nullptr;

#if SKY_RENDER_ALLOWED
    Sky *sky = nullptr; // Sky instance for day/night cycle
#endif

    Sprite3D *treeSprite = nullptr;
    Sprite3D *wallSprite = nullptr;
    Sprite3D *vWallSprite = nullptr;
};
