#include "level.hpp"
#include "game.hpp"
#include "pico-game-engine/engine/game.hpp"
#include "pico-game-engine/engine/camera.hpp"
#include "pico-game-engine/engine/entity.hpp"
#include "pico-game-engine/engine/sprite3d.hpp"
#include <math.h>
#include <stdio.h>

GhoulsLevel::GhoulsLevel(const char *name, const Vector &size, Game *game, GhoulsGame *ghoulsGame)
    : Level(name, size, game), ghoulsGame(ghoulsGame)
{
    if (!setMapPack(defaultMapPack))
    {
        ENGINE_LOG_INFO("[GhoulsLevel:constructor] Failed to set default map pack for the level\n");
        return;
    }
    // else
    // {
    //     mapPackSaveToFile(ASSETS_FOLDER "home.pceg", &mapData);
    // }
    // if (!setMapPack(ASSETS_FOLDER "home.pceg"))
    // {
    //     ENGINE_LOG_INFO("[GhoulsLevel:constructor] Failed to set default map pack for the level\n");
    //     return;
    // }
#if SKY_RENDER_ALLOWED
    sky = ENGINE_MEM_NEW Sky();
    if (!sky)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:constructor] Failed to create Sky instance\n");
        return;
    }
    sky->setSky(mapData.skyDayGradient);
#endif
#if GROUND_RENDER_ALLOWED
    ground = ENGINE_MEM_NEW Ground();
    if (!ground)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:constructor] Failed to create Ground instance\n");
        return;
    }
    ground->setGround(mapData.groundDayGradient);
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
    if (renderDists)
    {
        ENGINE_MEM_DELETE[] renderDists;
        renderDists = nullptr;
    }
    if (renderIndices)
    {
        ENGINE_MEM_DELETE[] renderIndices;
        renderIndices = nullptr;
    }

    ghoulsGame = nullptr;
}

bool GhoulsLevel::collisionMapCheck(Vector new_position)
{
    if (currentDynamicMap == nullptr)
        return false;

    // Check multiple points around the position to prevent clipping through walls
    const float offset = 0.5f;
    const uint8_t width = currentDynamicMap->getWidth();
    const uint8_t height = currentDynamicMap->getHeight();

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
        if (x >= width || y >= height)
        {
            // Out of bounds, treat as collision
            return true;
        }

        if (currentDynamicMap->getTile(x, y) != TILE_EMPTY)
        {
            return true;
        }
    }

    return false; // No collision detected
}

bool GhoulsLevel::initializeSprites()
{
    // cleanup first
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
    // house
    houseSprite = ENGINE_MEM_NEW Sprite3D();
    if (!houseSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for house sprite\n");
        return false;
    }
    houseSprite->initializeAsHouse(Vector(), 3.0f, 3.0f, 0.0f, mapData.houseColor, WIREFRAME_ENABLED);

    // tree
    treeSprite = ENGINE_MEM_NEW Sprite3D();
    if (!treeSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for tree sprite\n");
        return false;
    }
    treeSprite->initializeAsTree(Vector(), 4.0f, mapData.treeColor, WIREFRAME_ENABLED);

    // horizontal wall (top/bottom borders: len = MAP_WIDTH, rotation = 0)
    wallSprite = ENGINE_MEM_NEW Sprite3D();
    if (!wallSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for horizontal wall sprite\n");
        return false;
    }
    wallSprite->setPosition(Vector(0, 0));
    wallSprite->setRotation(0.0f);
    wallSprite->createWall(0, 0.75f, 0, 8.0f, MAP_WALL_HEIGHT, MAP_WALL_DEPTH, mapData.wallColor, WIREFRAME_ENABLED);
    wallSprite->setActive(true);

    // vertical wall (left/right borders: segment width = 8, rotation = pi/2)
    vWallSprite = ENGINE_MEM_NEW Sprite3D();
    if (!vWallSprite)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:initializeSprites] Failed to create Sprite3D instance for vertical wall sprite\n");
        return false;
    }
    vWallSprite->setPosition(Vector(0, 0));
    vWallSprite->setRotation((float)(M_PI / 2.0));
    vWallSprite->createWall(0, 0.75f, 0, 8.0f, MAP_WALL_HEIGHT, MAP_WALL_DEPTH, mapData.wallColor, WIREFRAME_ENABLED);
    vWallSprite->setActive(true);

    return true;
}

bool GhoulsLevel::isPositionAvailable(Vector position)
{
    // check houses
    for (uint8_t i = 0; i < mapData.houseCount; i++)
    {
        if (position == mapData.housePositions[i])
        {
            return false;
        }
    }
    // check trees
    for (uint8_t i = 0; i < mapData.treeCount; i++)
    {
        if (position == mapData.treePositions[i])
        {
            return false;
        }
    }
    // check walls
    for (uint8_t i = 0; i < mapData.hWallCount; i++)
    {
        if (position == mapData.hWallPositions[i])
        {
            return false;
        }
    }
    for (uint8_t i = 0; i < mapData.vWallCount; i++)
    {
        if (position == mapData.vWallPositions[i])
        {
            return false;
        }
    }
    return true;
}

void GhoulsLevel::registerSpritePositionsOnMap(DynamicMap *map)
{
    // Houses: fill the full HOUSE_TILE_SIZE x HOUSE_TILE_SIZE footprint centered on spawn
    const int hHalf = HOUSE_TILE_SIZE / 2;
    for (uint8_t i = 0; i < mapData.houseCount; i++)
    {
        int cx = (int)mapData.housePositions[i].x;
        int cy = (int)mapData.housePositions[i].y;
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
    for (uint8_t i = 0; i < mapData.treeCount; i++)
    {
        int cx = (int)mapData.treePositions[i].x;
        int cy = (int)mapData.treePositions[i].y;
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

    // wall segments
    for (uint8_t i = 0; i < mapData.hWallCount; i++)
    {
        map->setTile(mapData.hWallPositions[i].x, mapData.hWallPositions[i].y, TILE_WALL);
    }
    for (uint8_t i = 0; i < mapData.vWallCount; i++)
    {
        map->setTile(mapData.vWallPositions[i].x, mapData.vWallPositions[i].y, TILE_WALL);
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

    // Index encoding:
    //   0 .. HOUSE_SPAWN_COUNT-1                              -> house
    //   HOUSE_SPAWN_COUNT .. RENDER_WALL_OFFSET-1             -> tree
    //   RENDER_WALL_OFFSET .. RENDER_ENTITY_OFFSET-1          -> wall segment
    //   RENDER_ENTITY_OFFSET .. 255                           -> level entity
    int RENDER_TREE_OFFSET = mapData.houseCount;
    int RENDER_WALL_OFFSET = (mapData.houseCount + mapData.treeCount);
    int RENDER_ENTITY_OFFSET = (mapData.houseCount + mapData.treeCount + mapData.hWallCount + mapData.vWallCount);
    int MAX_RENDER_ITEMS = (mapData.houseCount + mapData.treeCount + mapData.hWallCount + mapData.vWallCount + 32);

    // build combined render list
    float *dists = renderDists;
    uint8_t *indices = renderIndices;
    int count = 0;

    if (renderEnv)
    {
        // Houses (encoded index 0 .. HOUSE_SPAWN_COUNT-1)
        for (int i = 0; i < mapData.houseCount && count < MAX_RENDER_ITEMS; i++)
        {
            float dx = mapData.housePositions[i].x - camPos.x;
            float dy = mapData.housePositions[i].y - camPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > (float)FIELD_OF_VIEW_SQUARED)
                continue;
            dists[count] = d2;
            indices[count] = (uint8_t)i;
            count++;
        }

        // Trees (encoded index RENDER_TREE_OFFSET .. RENDER_WALL_OFFSET-1)
        for (int i = 0; i < mapData.treeCount && count < MAX_RENDER_ITEMS; i++)
        {
            float dx = mapData.treePositions[i].x - camPos.x;
            float dy = mapData.treePositions[i].y - camPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > (float)FIELD_OF_VIEW_SQUARED)
                continue;
            dists[count] = d2;
            indices[count] = (uint8_t)(RENDER_TREE_OFFSET + i);
            count++;
        }

        // Wall segments (encoded index RENDER_WALL_OFFSET .. RENDER_ENTITY_OFFSET-1)
        for (int i = 0; i < mapData.hWallCount && count < MAX_RENDER_ITEMS; i++)
        {
            float dx = mapData.hWallPositions[i].x - camPos.x;
            float dy = mapData.hWallPositions[i].y - camPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > (float)FIELD_OF_VIEW_SQUARED)
                continue;
            dists[count] = d2;
            indices[count] = (uint8_t)(RENDER_WALL_OFFSET + i); // hWall index
            count++;
        }
        for (int i = 0; i < mapData.vWallCount && count < MAX_RENDER_ITEMS; i++)
        {
            float dx = mapData.vWallPositions[i].x - camPos.x;
            float dy = mapData.vWallPositions[i].y - camPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > (float)FIELD_OF_VIEW_SQUARED)
                continue;
            dists[count] = d2;
            indices[count] = (uint8_t)(RENDER_WALL_OFFSET + mapData.hWallCount + i); // vWall index
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
            houseSprite->setPosition(mapData.housePositions[idx]);
            render3DSprite(houseSprite, game->draw, camPos, camDir, gameCamera->height);
        }
        else if (idx < RENDER_WALL_OFFSET)
        {
            // Tree
            uint8_t ti = idx - RENDER_TREE_OFFSET;
            treeSprite->setPosition(mapData.treePositions[ti]);
            render3DSprite(treeSprite, game->draw, camPos, camDir, gameCamera->height);
        }
        else if (idx < RENDER_ENTITY_OFFSET)
        {
#if (!WALL_RENDER_ALLOWED)
            continue;
#endif
            // Wall segment
            uint8_t wi = idx - RENDER_WALL_OFFSET;
            Sprite3D *wallSpr = (wi < mapData.hWallCount) ? wallSprite : vWallSprite;
            wallSpr->setPosition((wi < mapData.hWallCount) ? mapData.hWallPositions[wi] : mapData.vWallPositions[wi - mapData.hWallCount]);
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

            uint16_t x = (uint16_t)(mapPosition.x + tx * scale_x);
            uint16_t y = (uint16_t)(mapPosition.y + ty * scale_y);
            uint16_t w = (uint16_t)(scale_x + 0.5f);
            uint16_t h = (uint16_t)(scale_y + 0.5f);
            if (w < 1)
                w = 1;
            if (h < 1)
                h = 1;

            switch (tile)
            {
            case TILE_HOUSE:
                canvas->fillRectangle(x, y, w, h, mapData.houseColor);
                break;
            case TILE_TREE:
                canvas->fillRectangle(x, y, w, h, mapData.treeColor);
                break;
            default:
                canvas->fillRectangle(x, y, w, h, mapData.wallColor);
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
                {
                    color = WEAPON_MINIMAP_COLOR;
                    Weapon *weapon = static_cast<Weapon *>(e);
                    if (weapon && weapon->isHeld())
                    {
                        continue;
                    }
                    break;
                }
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

void GhoulsLevel::renderMiniatureMiniMap(Draw *canvas)
{
    if (currentDynamicMap == nullptr)
        return;

    Player *player = ghoulsGame->getPlayer();
    if (!player)
        return;

    // Find player position
    const Vector playerPos = player->position;
    const int sw = canvas->getDisplaySize().x;

    // Small square in the top-left corner
    const int mmLen = sw * 30 / 128; // square side length in pixels
    const int mmX = sw * 2 / 128;    // left margin
    const int mmY = sw * 2 / 128;    // top margin

    // World-tile radius visible around the player
    const float scale = (float)mmLen / (MINIMAP_VIEW_RADIUS * 2.0f);

    // Background + border
    canvas->fillRectangle(mmX, mmY, mmLen, mmLen, 0xFFFF);
    canvas->rectangle(mmX, mmY, mmLen, mmLen, 0x0000);

    const uint8_t mapW = currentDynamicMap->getWidth();
    const uint8_t mapH = currentDynamicMap->getHeight();

    // Draw map tiles within the view window
    for (uint8_t ty = 0; ty < mapH; ty++)
    {
        for (uint8_t tx = 0; tx < mapW; tx++)
        {
            TileType tile = currentDynamicMap->getTile(tx, ty);
            if (tile == TILE_EMPTY)
                continue;

            float relX = tx - playerPos.x;
            float relY = ty - playerPos.y;
            if (relX < -MINIMAP_VIEW_RADIUS || relX > MINIMAP_VIEW_RADIUS || relY < -MINIMAP_VIEW_RADIUS || relY > MINIMAP_VIEW_RADIUS)
                continue;

            uint16_t px = mmX + (uint16_t)((relX + MINIMAP_VIEW_RADIUS) * scale);
            uint16_t py = mmY + (uint16_t)((relY + MINIMAP_VIEW_RADIUS) * scale);
            uint16_t pw = (uint16_t)(scale + 0.5f);
            uint16_t ph = (uint16_t)(scale + 0.5f);
            if (pw < 1)
                pw = 1;
            if (ph < 1)
                ph = 1;

            uint16_t color;
            switch (tile)
            {
            case TILE_HOUSE:
                color = mapData.houseColor;
                break;
            case TILE_TREE:
                color = mapData.treeColor;
                break;
            default:
                color = mapData.wallColor;
                break;
            }
            canvas->fillRectangle(px, py, pw, ph, color);
        }
    }

    // Draw entities within the view window
    for (int i = 0; i < getEntityCount(); i++)
    {
        Entity *e = getEntity(i);
        if (!e)
            continue;
        const EntityType type = e->type;
        if (type != ENTITY_PLAYER && type != ENTITY_ENEMY && type != ENTITY_NPC)
            continue;

        float relX = e->position.x - playerPos.x;
        float relY = e->position.y - playerPos.y;
        if (relX < -MINIMAP_VIEW_RADIUS || relX > MINIMAP_VIEW_RADIUS || relY < -MINIMAP_VIEW_RADIUS || relY > MINIMAP_VIEW_RADIUS)
            continue;

        if (type == ENTITY_NPC)
        {
            Weapon *weapon = static_cast<Weapon *>(e);
            if (weapon && weapon->isHeld())
                continue;
        }

        uint16_t ppx = mmX + (uint16_t)((relX + MINIMAP_VIEW_RADIUS) * scale);
        uint16_t ppy = mmY + (uint16_t)((relY + MINIMAP_VIEW_RADIUS) * scale);

        uint16_t color;
        switch (type)
        {
        case ENTITY_PLAYER:
            color = PLAYER_MINIMAP_COLOR;
            break;
        case ENTITY_ENEMY:
            color = ENEMY_MINIMAP_COLOR;
            break;
        default:
            color = WEAPON_MINIMAP_COLOR;
            break;
        }

        // 3x3 white halo so the dot is visible over walls
        canvas->fillRectangle(ppx - 1, ppy - 1, 3, 3, 0xFFFF);

        // Direction arrow
        if (e->direction.x != 0.0f || e->direction.y != 0.0f)
        {
            if (type != ENTITY_NPC)
            {
                uint16_t tip_x = ppx + (uint16_t)(e->direction.x * 4.0f);
                uint16_t tip_y = ppy + (uint16_t)(e->direction.y * 4.0f);
                canvas->line(ppx, ppy, tip_x, tip_y, color);
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

bool GhoulsLevel::setMapPack(const char *filename)
{
    if (!mapPackLoadFromFile(filename, &mapData))
    {
        ENGINE_LOG_INFO("[GhoulsLevel:setMapPack] Failed to load map pack from file: %s\n", filename);
        return false;
    }
    return setMapPack(mapData);
}

bool GhoulsLevel::setMapPack(const map_data_t &newMapData)
{
    if (currentDynamicMap)
    {
        ENGINE_MEM_DELETE currentDynamicMap;
        currentDynamicMap = nullptr;
    }
    if (renderDists)
    {
        ENGINE_MEM_DELETE[] renderDists;
        renderDists = nullptr;
    }
    if (renderIndices)
    {
        ENGINE_MEM_DELETE[] renderIndices;
        renderIndices = nullptr;
    }
    mapData = newMapData;
    renderItemsMax = mapData.houseCount + mapData.treeCount + mapData.vWallCount + mapData.hWallCount + 32;
    renderDists = ENGINE_MEM_NEW float[renderItemsMax];
    if (!renderDists)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:setMapPack] Failed to allocate memory for render distances array\n");
        return false;
    }
    renderIndices = ENGINE_MEM_NEW uint8_t[renderItemsMax];
    if (!renderIndices)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:setMapPack] Failed to allocate memory for render indices array\n");
        return false;
    }
    currentDynamicMap = ENGINE_MEM_NEW DynamicMap(mapData.width, mapData.height, false);
    if (!currentDynamicMap)
    {
        ENGINE_LOG_INFO("[GhoulsLevel:setMapPack] Failed to create new DynamicMap instance with provided map data\n");
        return false;
    }
    registerSpritePositionsOnMap(currentDynamicMap);
    return initializeSprites();
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