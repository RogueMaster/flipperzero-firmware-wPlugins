#pragma once
#include "pico-game-engine/engine/level.hpp"
#include "pico-game-engine/engine/sprite3d.hpp"
#include "dynamic_map.hpp"
#include "general.hpp"
#include "map.hpp"

#if SKY_RENDER_ALLOWED
#include "sky.hpp"
#endif

#if GROUND_RENDER_ALLOWED
#include "ground.hpp"
#endif

class GhoulsGame;

class GhoulsLevel : public Level
{
public:
    GhoulsLevel(const char *name, const Vector &size, Game *game, GhoulsGame *ghoulsGame, const char *levelMapFilename = ASSETS_FOLDER "home.ghoulsmap");
    ~GhoulsLevel();
    bool collisionMapCheck(Vector new_position);
    map_data_t *getMapData() { return &mapData; }
#if GROUND_RENDER_ALLOWED
    Ground *getGround() const { return ground; }
#endif
#if SKY_RENDER_ALLOWED
    Sky *getSky() const { return sky; }
#endif
    bool isPositionAvailable(Vector position);
    virtual void render(Game *game) override;
    void renderMiniMap(Draw *canvas, bool miniature = false);
    bool setMapPack(const char *filename);
    bool setMapPack(const map_data_t &newMapData);
    virtual void update(Game *game) override;

private:
    bool initializeSprites();
    void registerSpritePositionsOnMap(DynamicMap *map);

    map_data_t mapData;

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

    float *renderDists = nullptr;
    uint8_t *renderIndices = nullptr;
    int renderItemsMax = 0;
};
