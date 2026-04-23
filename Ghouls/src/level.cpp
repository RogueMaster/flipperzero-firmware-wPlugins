#include "level.hpp"
#include "game.hpp"
#include "pico-game-engine/engine/game.hpp"
#include "pico-game-engine/engine/camera.hpp"
#include "pico-game-engine/engine/entity.hpp"
#include "pico-game-engine/engine/sprite3d.hpp"
#include <math.h>
#include <stdio.h>

// clang-format off
const Vector GhoulsLevel::housePositions[HOUSE_SPAWN_COUNT] = {
    Vector(12,  8),  // 0
    Vector(12, 36),  // 1
    Vector(36, 36),  // 2
    Vector(72,  8),  // 3
    Vector(72, 28),  // 4
    Vector(84, 28),  // 5
};

const Vector GhoulsLevel::treePositions[TREE_SPAWN_COUNT] = {
    Vector( 8,  0),  //  0: (2,  0)
    Vector( 8, 44),  //  1: (2, 11)
    Vector(12,  0),  //  2: (3,  0)
    Vector(12, 44),  //  3: (3, 11)
    Vector(16,  0),  //  4: (4,  0)
    Vector(16, 44),  //  5: (4, 11)
    Vector(20, 24),  //  6: (5,  6)
    Vector(20, 28),  //  7: (5,  7)
    Vector(28,  8),  //  8: (7,  2)
    Vector(28, 12),  //  9: (7,  3)
    Vector(28, 16),  // 10: (7,  4)
    Vector(32,  8),  // 11: (8,  2)
    Vector(32, 12),  // 12: (8,  3)
    Vector(32, 16),  // 13: (8,  4)
    Vector(36,  8),  // 14: (9,  2)
    Vector(40,  8),  // 15: (10, 2)
    Vector(40, 12),  // 16: (10, 3)
    Vector(40, 16),  // 17: (10, 4)
    Vector(44,  8),  // 18: (11, 2)
    Vector(44, 12),  // 19: (11, 3)
    Vector(44, 16),  // 20: (11, 4)
    Vector(32, 44),  // 21: (8, 11)
    Vector(36, 44),  // 22: (9, 11)
    Vector(40, 44),  // 23: (10,11)
    Vector(44, 24),  // 24: (11, 6)
    Vector(44, 28),  // 25: (11, 7)
    Vector(48,  0),  // 26: (12, 0)
    Vector(52,  0),  // 27: (13, 0)
    Vector(56,  0),  // 28: (14, 0)
    Vector(64, 36),  // 29: (16, 9)
    Vector(64, 44),  // 30: (16,11)
    Vector(68, 36),  // 31: (17, 9)
    Vector(68, 44),  // 32: (17,11)
    Vector(72, 36),  // 33: (18, 9)
    Vector(72, 44),  // 34: (18,11)
    Vector(76,  0),  // 35: (19, 0)
    Vector(80,  0),  // 36: (20, 0)
    Vector(84,  0),  // 37: (21, 0)
    Vector(88,  0),  // 38: (22, 0)
    Vector(84, 36),  // 39: (21, 9)
    Vector(88, 36),  // 40: (22, 9)
    Vector(84, 44),  // 41: (21,11)
    Vector(92,  0),  // 42: (23, 0)
    Vector(92,  4),  // 43: (23, 1)
    Vector(92,  8),  // 44: (23, 2)
    Vector(92, 12),  // 45: (23, 3)
    Vector(92, 16),  // 46: (23, 4)
    Vector(92, 20),  // 47: (23, 5)
    Vector(92, 24),  // 48: (23, 6)
    Vector(92, 28),  // 49: (23, 7)
    Vector(92, 32),  // 50: (23, 8)
    Vector(92, 36),  // 51: (23, 9)
    Vector(92, 40),  // 52: (23,10)
    Vector(92, 44),  // 53: (23,11)
};

// MAP_WIDTH=96, MAP_HEIGHT=48
const Vector GhoulsLevel::wallPositions[MAP_OUTER_WALLS] = {
    Vector(48.0f,  0.5f),  // 0: top horizontal
    Vector(48.0f, 47.5f),  // 1: bottom horizontal
    Vector( 0.5f, 24.0f),  // 2: left vertical
    Vector(95.5f, 24.0f),  // 3: right vertical
};

// WALL_SEGMENT_SIZE=8 → 12 h-segments per row, 6 v-segments per column
// Indices 0–23: horizontal (top then bottom); 24–35: vertical (left then right)
const Vector GhoulsLevel::wallSegmentPositions[WALL_SEGMENT_COUNT] = {
    // top wall (y=0.5)
    Vector( 4.0f,  0.5f), Vector(12.0f,  0.5f), Vector(20.0f,  0.5f), Vector(28.0f,  0.5f),
    Vector(36.0f,  0.5f), Vector(44.0f,  0.5f), Vector(52.0f,  0.5f), Vector(60.0f,  0.5f),
    Vector(68.0f,  0.5f), Vector(76.0f,  0.5f), Vector(84.0f,  0.5f), Vector(92.0f,  0.5f),
    // bottom wall (y=47.5)
    Vector( 4.0f, 47.5f), Vector(12.0f, 47.5f), Vector(20.0f, 47.5f), Vector(28.0f, 47.5f),
    Vector(36.0f, 47.5f), Vector(44.0f, 47.5f), Vector(52.0f, 47.5f), Vector(60.0f, 47.5f),
    Vector(68.0f, 47.5f), Vector(76.0f, 47.5f), Vector(84.0f, 47.5f), Vector(92.0f, 47.5f),
    // left wall (x=0.5)
    Vector( 0.5f,  4.0f), Vector( 0.5f, 12.0f), Vector( 0.5f, 20.0f),
    Vector( 0.5f, 28.0f), Vector( 0.5f, 36.0f), Vector( 0.5f, 44.0f),
    // right wall (x=95.5)
    Vector(95.5f,  4.0f), Vector(95.5f, 12.0f), Vector(95.5f, 20.0f),
    Vector(95.5f, 28.0f), Vector(95.5f, 36.0f), Vector(95.5f, 44.0f),
};
// clang-format on

GhoulsLevel::GhoulsLevel(const char *name, const Vector &size, Game *game, GhoulsGame *ghoulsGame)
    : Level(name, size, game), ghoulsGame(ghoulsGame)
{
    if (!initializeSprites())
    {
        ENGINE_LOG_INFO("[GhoulsLevel:constructor] Failed to initialize sprites for the level");
        return;
    }
    currentDynamicMap = ENGINE_MEM_NEW DynamicMap("Online", MAP_WIDTH, MAP_HEIGHT, false, MAP_WALL_HEIGHT, MAP_WALL_DEPTH);
    if (!currentDynamicMap)
    {
        ENGINE_LOG_INFO("[GhoulsGame:setDynamicMap] Failed to create map instance");
        return;
    }
    registerSpritePositionsOnMap(currentDynamicMap);
#if SKY_RENDER_ALLOWED
    sky = ENGINE_MEM_NEW Sky(SKY_SUNNY);
    if (!sky)
    {
        ENGINE_LOG_INFO("[GhoulsGame:GhoulsGame] Failed to create Sky instance");
        return;
    }
#endif
#if GROUND_RENDER_ALLOWED
    ground = ENGINE_MEM_NEW Ground(GROUND_DIRT);
    if (!ground)
    {
        ENGINE_LOG_INFO("[GhoulsGame:GhoulsGame] Failed to create Ground instance");
        return;
    }
#endif
}

GhoulsLevel::~GhoulsLevel()
{
    if (houseSprite)
    {
        ENGINE_MEM_DELETE houseSprite;
        houseSprite = nullptr;
    }
    if (treeSprite)
    {
        ENGINE_MEM_DELETE treeSprite;
        treeSprite = nullptr;
    }
#if SKY_RENDER_ALLOWED
    if (sky)
    {
        ENGINE_MEM_DELETE sky;
        sky = nullptr;
    }
#endif
#if GROUND_RENDER_ALLOWED
    if (ground)
    {
        ENGINE_MEM_DELETE ground;
        ground = nullptr;
    }
#endif
    if (wallSprite)
    {
        ENGINE_MEM_DELETE wallSprite;
        wallSprite = nullptr;
    }
    if (vWallSprite)
    {
        ENGINE_MEM_DELETE vWallSprite;
        vWallSprite = nullptr;
    }
    if (currentDynamicMap)
    {
        ENGINE_MEM_DELETE currentDynamicMap;
        currentDynamicMap = nullptr;
    }

    ghoulsGame = nullptr;
}

bool GhoulsLevel::collisionMapCheck(Vector new_position)
{
    if (currentDynamicMap == nullptr)
        return false;

    // Check multiple points around the position to prevent clipping through walls
    float offset = 0.3f;

    Vector checkPoints[] = {
        new_position,                                             // Center
        Vector(new_position.x - offset, new_position.y - offset), // Top-left
        Vector(new_position.x + offset, new_position.y - offset), // Top-right
        Vector(new_position.x - offset, new_position.y + offset), // Bottom-left
        Vector(new_position.x + offset, new_position.y + offset)  // Bottom-right
    };

    Vector point;
    for (int i = 0; i < 5; i++)
    {
        point = checkPoints[i];

        // Ensure we're checking within bounds
        if (point.x < 0 || point.y < 0)
            return true; // Collision (out of bounds)

        uint8_t x = (uint8_t)point.x;
        uint8_t y = (uint8_t)point.y;

        // Bounds checking
        if (x >= currentDynamicMap->getWidth() || y >= currentDynamicMap->getHeight())
        {
            // Out of bounds, treat as collision
            return true;
        }

        TileType tile = currentDynamicMap->getTile(x, y);
        if (tile != TILE_EMPTY)
        {
            return true;
        }
    }

    return false; // No collision detected
}

bool GhoulsLevel::initializeSprites()
{
    // house
    houseSprite = ENGINE_MEM_NEW Sprite3D();
    if (!houseSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for house sprite");
        return false;
    }
    houseSprite->initializeAsHouse(Vector(), 3.0f, 3.0f, 0.0f, HOUSE_COLOR, WIREFRAME_ENABLED);

    // tree
    treeSprite = ENGINE_MEM_NEW Sprite3D();
    if (!treeSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for tree sprite");
        return false;
    }
    treeSprite->initializeAsTree(Vector(), 4.0f, TREE_COLOR, WIREFRAME_ENABLED);

    // horizontal wall (top/bottom borders: len = MAP_WIDTH, rotation = 0)
    wallSprite = ENGINE_MEM_NEW Sprite3D();
    if (!wallSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for horizontal wall sprite");
        return false;
    }
    wallSprite->setPosition(Vector(0, 0));
    wallSprite->setRotation(0.0f);
    wallSprite->createWall(0, 0.75f, 0, 8.0f, MAP_WALL_HEIGHT, MAP_WALL_DEPTH);
    wallSprite->setActive(true);
    wallSprite->setWireframe(WIREFRAME_ENABLED);

    // vertical wall (left/right borders: segment width = 8, rotation = pi/2)
    vWallSprite = ENGINE_MEM_NEW Sprite3D();
    if (!vWallSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for vertical wall sprite");
        return false;
    }
    vWallSprite->setPosition(Vector(0, 0));
    vWallSprite->setRotation((float)(M_PI / 2.0));
    vWallSprite->createWall(0, 0.75f, 0, 8.0f, MAP_WALL_HEIGHT, MAP_WALL_DEPTH);
    vWallSprite->setActive(true);
    vWallSprite->setWireframe(WIREFRAME_ENABLED);

    return true;
}

void GhoulsLevel::registerSpritePositionsOnMap(DynamicMap *map)
{
    // Houses: fill the full HOUSE_TILE_SIZE x HOUSE_TILE_SIZE footprint centered on spawn
    const int hHalf = HOUSE_TILE_SIZE / 2;
    for (uint8_t i = 0; i < HOUSE_SPAWN_COUNT; i++)
    {
        int cx = (int)housePositions[i].x;
        int cy = (int)housePositions[i].y;
        for (int dy = -hHalf; dy <= hHalf; dy++)
        {
            for (int dx = -hHalf; dx <= hHalf; dx++)
            {
                int tx = cx + dx;
                int ty = cy + dy;
                if (tx >= 0 && ty >= 0)
                    map->setTile((uint8_t)tx, (uint8_t)ty, TILE_HOUSE);
            }
        }
    }

    // Trees: fill the full TREE_TILE_SIZE x TREE_TILE_SIZE footprint centered on spawn
    const int tHalf = TREE_TILE_SIZE / 2;
    for (uint8_t i = 0; i < TREE_SPAWN_COUNT; i++)
    {
        int cx = (int)treePositions[i].x;
        int cy = (int)treePositions[i].y;
        for (int dy = -tHalf; dy <= tHalf; dy++)
        {
            for (int dx = -tHalf; dx <= tHalf; dx++)
            {
                int tx = cx + dx;
                int ty = cy + dy;
                if (tx >= 0 && ty >= 0)
                    map->setTile((uint8_t)tx, (uint8_t)ty, TILE_TREE);
            }
        }
    }

    for (uint8_t i = 0; i < MAP_OUTER_WALLS; i++)
    {
        map->setTile(wallPositions[i].x, wallPositions[i].y, TILE_WALL);
    }
}

void GhoulsLevel::render(Game *game)
{
    Camera *gameCamera = game->getCamera();
    Player *player = ghoulsGame->getPlayer();
    Vector ss = game->draw->getDisplaySize();

    // get third-person camera position
    if (gameCamera->perspective == CAMERA_THIRD_PERSON)
    {
        float dir_length = sqrtf(player->direction.x * player->direction.x +
                                 player->direction.y * player->direction.y);
        if (dir_length < 0.001f)
        {
            dir_length = 1.0f;
            player->direction.x = 1;
            player->direction.y = 0;
        }
        float ndx = player->direction.x / dir_length;
        float ndy = player->direction.y / dir_length;

        gameCamera->position.x = player->position.x - ndx * gameCamera->distance;
        gameCamera->position.y = player->position.y - ndy * gameCamera->distance;
        gameCamera->direction.x = ndx;
        gameCamera->direction.y = ndy;
        gameCamera->plane.x = player->plane.x;
        gameCamera->plane.y = player->plane.y;
    }

    const Vector camPos = gameCamera->position;
    const Vector camDir = gameCamera->direction;

    // Only render the environment during active gameplay
    const bool renderEnv = (player->getCurrentGameState() == GameStatePlaying);

    // sky
#if SKY_RENDER_ALLOWED
    if (renderEnv && sky)
        sky->render(game->draw);
#endif

    // ground
#if GROUND_RENDER_ALLOWED
    if (renderEnv && ground)
        ground->render(game->draw);
#endif

    // build combined render list
    float dists[MAX_RENDER_ITEMS];
    uint8_t indices[MAX_RENDER_ITEMS];
    int count = 0;

    if (renderEnv)
    {
        // Houses (encoded index 0 .. HOUSE_SPAWN_COUNT-1)
        for (int i = 0; i < HOUSE_SPAWN_COUNT && count < MAX_RENDER_ITEMS; i++)
        {
            float dx = housePositions[i].x - camPos.x;
            float dy = housePositions[i].y - camPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > (float)FIELD_OF_VIEW_SQUARED)
                continue;
            dists[count] = d2;
            indices[count] = (uint8_t)i;
            count++;
        }

        // Trees (encoded index RENDER_TREE_OFFSET .. RENDER_WALL_OFFSET-1)
        for (int i = 0; i < TREE_SPAWN_COUNT && count < MAX_RENDER_ITEMS; i++)
        {
            float dx = treePositions[i].x - camPos.x;
            float dy = treePositions[i].y - camPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > (float)FIELD_OF_VIEW_SQUARED)
                continue;
            dists[count] = d2;
            indices[count] = (uint8_t)(RENDER_TREE_OFFSET + i);
            count++;
        }

        // Wall segments (encoded index RENDER_WALL_OFFSET .. RENDER_ENTITY_OFFSET-1)
        for (int i = 0; i < WALL_SEGMENT_COUNT && count < MAX_RENDER_ITEMS; i++)
        {
            float dx = wallSegmentPositions[i].x - camPos.x;
            float dy = wallSegmentPositions[i].y - camPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > (float)FIELD_OF_VIEW_SQUARED)
                continue;
            dists[count] = d2;
            indices[count] = (uint8_t)(RENDER_WALL_OFFSET + i);
            count++;
        }
    }

    // Entities (encoded index RENDER_ENTITY_OFFSET + entity_list_index)
    for (int i = 0; i < getEntityCount() && count < MAX_RENDER_ITEMS; i++)
    {
        Entity *e = getEntity(i);
        if (!e || !e->is_active)
            continue;
        float dx = e->position.x - camPos.x;
        float dy = e->position.y - camPos.y;
        dists[count] = dx * dx + dy * dy;
        indices[count] = (uint8_t)(RENDER_ENTITY_OFFSET + i);
        count++;
    }

    // sort back-to-front (painter's algorithm)
    for (int i = 1; i < count; i++)
    {
        float keyDist = dists[i];
        uint8_t keyIdx = indices[i];
        int j = i - 1;
        while (j >= 0 && dists[j] < keyDist)
        {
            dists[j + 1] = dists[j];
            indices[j + 1] = indices[j];
            j--;
        }
        dists[j + 1] = keyDist;
        indices[j + 1] = keyIdx;
    }

    char healthstr[16];

    // Render back-to-front
    for (int i = 0; i < count; i++)
    {
        const uint8_t idx = indices[i];

        if (idx < RENDER_TREE_OFFSET)
        {
            // House
            houseSprite->setPosition(housePositions[idx]);
            render3DSprite(houseSprite, game->draw, camPos, camDir, gameCamera->height);
        }
        else if (idx < RENDER_WALL_OFFSET)
        {
            // Tree
            uint8_t ti = idx - RENDER_TREE_OFFSET;
            treeSprite->setPosition(treePositions[ti]);
            render3DSprite(treeSprite, game->draw, camPos, camDir, gameCamera->height);
        }
        else if (idx < RENDER_ENTITY_OFFSET)
        {
            // Wall segment
            uint8_t wi = idx - RENDER_WALL_OFFSET;
            Sprite3D *wallSpr = (wi < WALL_H_SEGMENT_COUNT) ? wallSprite : vWallSprite;
            wallSpr->setPosition(wallSegmentPositions[wi]);
            render3DSprite(wallSpr, game->draw, camPos, camDir, gameCamera->height);
        }
        else
        {
            // Level entity
            int ei = idx - RENDER_ENTITY_OFFSET;
            Entity *ent = getEntity(ei);
            if (!ent || !ent->is_active)
                continue;

            ent->render(game->draw, game);

            if (!ent->is_visible)
                continue;

            if (ent->sprite != nullptr)
                ent->sprite->render(game->draw, ent->position.x - game->pos.x, ent->position.y - game->pos.y);

            if (ent->has3DSprite())
            {
                if (gameCamera->perspective == CAMERA_FIRST_PERSON)
                {
                    render3DSprite(ent->sprite_3d, game->draw, player->position, player->direction, gameCamera->height);
                }
                else // CAMERA_THIRD_PERSON
                {
                    render3DSprite(ent->sprite_3d, game->draw, camPos, camDir, gameCamera->height);
                }

                if (ent->type == ENTITY_ENEMY)
                {
                    // Project the head (world y = 2.2) to screen coords
                    float wdx = ent->position.x - gameCamera->position.x;
                    float wdz = ent->position.y - gameCamera->position.y; // position.y = world Z
                    float wdy = 2.2f - gameCamera->height;

                    float cx = wdx * (-gameCamera->direction.y) + wdz * gameCamera->direction.x;
                    float cz = wdx * gameCamera->direction.x + wdz * gameCamera->direction.y;
                    if (cz <= 0.1f)
                        continue; // behind camera

                    float sx = (cx / cz) * ss.y + ss.x * 0.5f;
                    float sy = (-wdy / cz) * ss.y + ss.y * 0.5f;
                    if (sx < 0 || sx >= ss.x || sy < 2 || sy >= ss.y - 2)
                        continue;

                    snprintf(healthstr, sizeof(healthstr), "%d/%d", (uint16_t)ent->health, (uint16_t)ent->max_health);
                    int tx = (int)sx - (int)(strlen(healthstr) * 3);
                    int ty = (int)sy;
                    if (tx < 0)
                        tx = 0;
                    game->draw->setFont(FONT_SIZE_SMALL);
                    game->draw->text(tx, ty, healthstr, ENEMY_MINIMAP_COLOR);
                }
            }
        }
    }
}

void GhoulsLevel::renderMiniMap(Draw *canvas)
{
    const int sw = canvas->getDisplaySize().x;
    const int sh = canvas->getDisplaySize().y;

    if (currentDynamicMap == nullptr)
    {
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(sw * 6 / 128, sh / 2, "No map loaded", 0x0000);
        return; // No map to render
    }

    Vector mapPosition = Vector(sw * 3 / 128, sh * 3 / 64);
    Vector mapSize = Vector(sw * 70 / 128, sh * 57 / 64);

    // Background
    canvas->fillRectangle(mapPosition.x, mapPosition.y, mapSize.x, mapSize.y, 0xFFFF);
    canvas->rectangle(mapPosition.x, mapPosition.y, mapSize.x, mapSize.y, 0x0000);

    const uint8_t width = currentDynamicMap->getWidth();
    const uint8_t height = currentDynamicMap->getHeight();

    // Scale factors: pixels per map tile
    float scale_x = (float)mapSize.x / width;
    float scale_y = (float)mapSize.y / height;

    for (uint8_t ty = 0; ty < height; ty++)
    {
        for (uint8_t tx = 0; tx < width; tx++)
        {
            TileType tile = currentDynamicMap->getTile(tx, ty);
            if (tile == TILE_EMPTY)
                continue;

            uint8_t x = (uint8_t)(mapPosition.x + tx * scale_x);
            uint8_t y = (uint8_t)(mapPosition.y + ty * scale_y);
            uint8_t w = (uint8_t)(scale_x + 0.5f);
            uint8_t h = (uint8_t)(scale_y + 0.5f);
            if (w < 1)
                w = 1;
            if (h < 1)
                h = 1;

            switch (tile)
            {
            case TILE_HOUSE:
                canvas->fillRectangle(x, y, w, h, HOUSE_COLOR);
                break;
            case TILE_TREE:
                canvas->fillRectangle(x, y, w, h, TREE_COLOR);
                break;
            default:
                canvas->fillRectangle(x, y, w, h, 0x0000);
                break;
            }
        }
    }

    for (int i = 0; i < getEntityCount(); i++)
    {
        Entity *e = getEntity(i);
        if (!e)
            continue;
        const EntityType type = (e) ? e->type : ENTITY_ICON;
        if (type == ENTITY_PLAYER || type == ENTITY_ENEMY || type == ENTITY_NPC)
        {
            if (e->position.x >= 0 && e->position.y >= 0)
            {
                uint16_t ppx = (uint16_t)(mapPosition.x + e->position.x * scale_x);
                uint16_t ppy = (uint16_t)(mapPosition.y + e->position.y * scale_y);
                uint16_t color = 0x0000;

                switch (type)
                {
                case ENTITY_PLAYER:
                    color = PLAYER_MINIMAP_COLOR;
                    break;
                case ENTITY_ENEMY:
                    color = ENEMY_MINIMAP_COLOR;
                    break;
                case ENTITY_NPC:
                    color = WEAPON_MINIMAP_COLOR;
                    break;
                default:
                    break;
                }

                // 3x3 white square so the dot is visible over walls
                canvas->fillRectangle(ppx - 1, ppy - 1, 3, 3, 0xFFFF);

                // Direction arrow: line from centre out 4px in facing direction
                if (e->direction.x != 0.0f || e->direction.y != 0.0f)
                {
                    if (type != ENTITY_NPC)
                    {
                        uint16_t tip_x = ppx + (uint16_t)(e->direction.x * 4.0f);
                        uint16_t tip_y = ppy + (uint16_t)(e->direction.y * 4.0f);
                        canvas->line(ppx, ppy, tip_x, tip_y, color);
                        // Arrowhead: two lines from tip back to flanking points
                        uint16_t base_x = tip_x - (uint16_t)(e->direction.x * 2.0f);
                        uint16_t base_y = tip_y - (uint16_t)(e->direction.y * 2.0f);
                        uint16_t perp_x = (uint16_t)(e->direction.y * 2.0f);
                        uint16_t perp_y = (uint16_t)(-e->direction.x * 2.0f);
                        canvas->line(tip_x, tip_y, base_x + perp_x, base_y + perp_y, color);
                        canvas->line(tip_x, tip_y, base_x - perp_x, base_y - perp_y, color);
                    }
                    else
                    {
                        canvas->circle(ppx, ppy, 2, color);
                    }
                }

                // Centre dot on top
                canvas->pixel(ppx, ppy, color);
            }
        }
    }
}

void GhoulsLevel::update(Game *game)
{
#if SKY_RENDER_ALLOWED
    sky->tick();
#endif

#if GROUND_RENDER_ALLOWED
    ground->tick();
#endif

    for (int i = 0; i < getEntityCount(); i++)
    {
        Entity *ent = getEntity(i);

        if (ent != nullptr && ent->is_active)
        {
            ent->update(game);

            for (int j = 0; j < getEntityCount(); j++)
            {
                Entity *other = getEntity(j);
                if (other != nullptr &&
                    other != ent &&
                    is_collision(ent, other))
                {
                    ent->collision(other, game);
                }
            }
        }
    }
}