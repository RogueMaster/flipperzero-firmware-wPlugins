#include "game.hpp"
#include "pico-game-engine/engine/draw.hpp"
#include "pico-game-engine/engine/game.hpp"
#include "pico-game-engine/engine/engine.hpp"
#include "enemy.hpp"
#include <math.h>

// clang-format off
const Vector GhoulsGame::housePositions[HOUSE_SPAWN_COUNT] = {
    Vector(12,  8),  // 0
    Vector(12, 36),  // 1
    Vector(36, 36),  // 2
    Vector(72,  8),  // 3
    Vector(72, 28),  // 4
    Vector(84, 28),  // 5
};

const Vector GhoulsGame::treePositions[TREE_SPAWN_COUNT] = {
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
const Vector GhoulsGame::wallPositions[MAP_OUTER_WALLS] = {
    Vector(48.0f,  0.5f),  // 0: top horizontal
    Vector(48.0f, 47.5f),  // 1: bottom horizontal
    Vector( 0.5f, 24.0f),  // 2: left vertical
    Vector(95.5f, 24.0f),  // 3: right vertical
};
// clang-format on

GhoulsGame::GhoulsGame(const char *username, const char *password, bool soundEnabled)
{
    // pass username and password to player
    player = ENGINE_MEM_NEW Player(username, password);
    if (!player)
    {
        ENGINE_LOG_INFO("[GhoulsGame:GhoulsGame] Failed to create Player instance");
        return;
    }
    player->setGhoulsGame(this);
    player->setSoundToggle(soundEnabled ? ToggleOn : ToggleOff);

    gameTime = ENGINE_MEM_NEW Time();
    if (!gameTime)
    {
        ENGINE_LOG_INFO("[GhoulsGame:GhoulsGame] Failed to create Time instance");
        ENGINE_MEM_DELETE player;
        player = nullptr;
        return;
    }
}

GhoulsGame::~GhoulsGame()
{
    this->endGame();
    if (player)
    {
        ENGINE_MEM_DELETE player;
        player = nullptr;
    }
    if (gameTime)
    {
        ENGINE_MEM_DELETE gameTime;
        gameTime = nullptr;
    }
    if (currentDynamicMap)
    {
        ENGINE_MEM_DELETE currentDynamicMap;
        currentDynamicMap = nullptr;
    }
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
    if (draw)
    {
        ENGINE_MEM_DELETE draw;
        draw = nullptr;
    }
    if (engine)
    {
        engine->stop();
        ENGINE_MEM_DELETE engine;
        engine = nullptr;
    }
}

bool GhoulsGame::collisionMapCheck(Vector new_position)
{
    if (currentDynamicMap == nullptr)
        return false;

    // Check multiple points around the position to prevent clipping through walls
    // This accounts for entity size and floating point positions
    float offset = 0.3f; // Small offset to check around entity position

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
        if (tile == TILE_WALL)
        {
            return true; // Wall blocks movement
        }
    }

    return false; // No collision detected
}

void GhoulsGame::endGame()
{
    shouldExit = true;
    isGameRunning = false;
}

bool GhoulsGame::setDynamicMap()
{
    currentDynamicMap = ENGINE_MEM_NEW DynamicMap("Online", MAP_WIDTH, MAP_HEIGHT, false, MAP_WALL_HEIGHT, MAP_WALL_DEPTH);
    if (!currentDynamicMap)
    {
        ENGINE_LOG_INFO("[GhoulsGame:setDynamicMap] Failed to create map instance");
        return false;
    }
    return true;
}

Vector GhoulsGame::getRandomGhoulPosition(Level *level)
{
    // possible ghoul spawns
    Vector spawnPoints[] = {
        Vector(15, 4),
        Vector(15, 11),
        Vector(16, 2),
        Vector(16, 3),
        Vector(16, 5),
        Vector(16, 6),
        Vector(16, 7),
        Vector(16, 8),
        Vector(17, 8),
        Vector(18, 8),
        Vector(19, 4),
        Vector(19, 10),
        Vector(20, 2),
        Vector(20, 4),
        Vector(20, 10),
        Vector(21, 3),
        Vector(21, 8),
        Vector(22, 1),
        Vector(22, 3),
        Vector(22, 10),
    };
    uint8_t randomIndex = 0;
    uint8_t attempts = 0;
    do
    {
        randomIndex = rand() % (sizeof(spawnPoints) / sizeof(spawnPoints[0]));
        attempts++;
        if (attempts > 20) // prevent infinite loop
        {
            break;
        }
    } while (positionExistsInLevel(level, spawnPoints[randomIndex]));
    return Vector(spawnPoints[randomIndex].x * 4, spawnPoints[randomIndex].y * 4); // scale up to map coordinates
}

Vector GhoulsGame::getRandomWeaponPosition(Level *level)
{
    // possible weapon spawns
    Vector spawnPoints[] = {
        Vector(2, 2),
        Vector(2, 9),
        Vector(3, 5),
        Vector(7, 6),
        Vector(8, 1),
        Vector(9, 3),
        Vector(13, 5),
        Vector(13, 11),
        Vector(15, 2),
        Vector(15, 3),
        Vector(15, 8),
        Vector(15, 9),
        Vector(16, 4),
        Vector(18, 4),
        Vector(20, 3),
        Vector(21, 4),
        Vector(22, 2),
        Vector(22, 4),
        Vector(22, 8),
        Vector(22, 11)};

    uint8_t randomIndex = 0;
    uint8_t attempts = 0;
    do
    {
        randomIndex = rand() % (sizeof(spawnPoints) / sizeof(spawnPoints[0]));
        attempts++;
        if (attempts > 20) // prevent infinite loop
        {
            break;
        }
    } while (positionExistsInLevel(level, spawnPoints[randomIndex]));
    return Vector(spawnPoints[randomIndex].x * 4, spawnPoints[randomIndex].y * 4); // scale up to map coordinates
}

WeaponType GhoulsGame::getUniqueWeaponType(Level *level)
{
    const WeaponType allWeaponTypes[] = {WEAPON_RIFLE, WEAPON_SHOTGUN, WEAPON_ROCKET_LAUNCHER, WEAPON_CROSSBOW};

    for (uint8_t i = 0; i < sizeof(allWeaponTypes) / sizeof(allWeaponTypes[0]); i++)
    {
        WeaponType type = allWeaponTypes[i];
        bool found = false;
        for (int j = 0; j < level->getEntityCount(); j++)
        {
            Entity *entity = level->getEntity(j);
            if (entity && entity->type == ENTITY_NPC) // weapons are "NPC"
            {
                Weapon *weapon = static_cast<Weapon *>(entity);
                if (weapon && weapon->getWeaponType() == type)
                {
                    found = true;
                    break;
                }
            }
        }
        if (!found)
        {
            return type; // this type is not yet in the level
        }
    }
    return WEAPON_NONE;
}

void GhoulsGame::increaseDifficulty()
{
    if (currentRound <= 1)
    {
        return; // no difficulty increase on first round
    }
    if (!engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:increaseDifficulty] Game engine instance is null");
        return;
    }
    Level *currentLevel = engine->getGame()->current_level;
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:increaseDifficulty] Current level instance is null");
        return;
    }
    const uint16_t decrement = currentRound - 1;
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(entity);
            enemy->max_health += (ENEMY_HEALTH_INCREMENT * decrement); // increase max health based on current round
            enemy->health = enemy->max_health;                         // restore health to max when stats are updated
            enemy->strength += (ENEMY_STRENGTH_INCREMENT * decrement); // increase strength based on current round
        }
    }
}

bool GhoulsGame::initDraw()
{
    if (!draw)
    {
        draw = ENGINE_MEM_NEW Draw();
        if (!draw)
        {
            ENGINE_LOG_INFO("[GhoulsGame:initDraw] Failed to create Draw instance");
            return false;
        }
    }
    return true;
}

bool GhoulsGame::initializeSprites()
{
    // house
    houseSprite = ENGINE_MEM_NEW Sprite3D();
    if (!houseSprite)
    {
        ENGINE_LOG_INFO("[GhoulsGame:initializeSprites] Failed to create Sprite3D instance for house sprite");
        return false;
    }
    houseSprite->initializeAsHouse(Vector(), 3.0f, 3.0f, 0.0f, 0x0000, WIREFRAME_ENABLED);

    // tree
    treeSprite = ENGINE_MEM_NEW Sprite3D();
    if (!treeSprite)
    {
        ENGINE_LOG_INFO("[GhoulsGame:initializeSprites] Failed to create Sprite3D instance for tree sprite");
        return false;
    }
    treeSprite->initializeAsTree(Vector(), 4.0f, 0x0000, WIREFRAME_ENABLED);

    // horizontal wall (top/bottom borders: len = MAP_WIDTH, rotation = 0)
    wallSprite = ENGINE_MEM_NEW Sprite3D();
    if (!wallSprite)
    {
        ENGINE_LOG_INFO("[GhoulsGame:initializeSprites] Failed to create Sprite3D instance for horizontal wall sprite");
        return false;
    }
    wallSprite->setPosition(Vector(0, 0));
    wallSprite->setRotation(0.0f);
    wallSprite->createWall(0, 0.75f, 0, MAP_WIDTH, MAP_WALL_HEIGHT, MAP_WALL_DEPTH);
    wallSprite->setActive(true);
    wallSprite->setWireframe(WIREFRAME_ENABLED);

    // vertical wall (left/right borders: len = MAP_HEIGHT, rotation = π/2)
    vWallSprite = ENGINE_MEM_NEW Sprite3D();
    if (!vWallSprite)
    {
        ENGINE_LOG_INFO("[GhoulsGame:initializeSprites] Failed to create Sprite3D instance for vertical wall sprite");
        return false;
    }
    vWallSprite->setPosition(Vector(0, 0));
    vWallSprite->setRotation((float)(M_PI / 2.0));
    vWallSprite->createWall(0, 0.75f, 0, MAP_HEIGHT, MAP_WALL_HEIGHT, MAP_WALL_DEPTH);
    vWallSprite->setActive(true);
    vWallSprite->setWireframe(WIREFRAME_ENABLED);

    return true;
}

void GhoulsGame::inputManager()
{
    // Pass input to player for processing
    if (player)
    {
        player->setInputKey(lastInput);
        player->processInput();
    }
}

void GhoulsGame::makeGhoulsGoHome()
{
    if (!engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:makeGhoulsGoHome] Game engine instance is null");
        return;
    }
    Level *currentLevel = engine->getGame()->current_level;
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:makeGhoulsGoHome] Current level instance is null");
        return;
    }
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(entity);
            enemy->state = ENTITY_MOVING_TO_START;
        }
    }
}

void GhoulsGame::makeGhoulsGoToPlayer()
{
    if (!engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:makeGhoulsGoToPlayer] Game engine instance is null");
        return;
    }
    Level *currentLevel = engine->getGame()->current_level;
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:makeGhoulsGoToPlayer] Current level instance is null");
        return;
    }
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(entity);
            enemy->state = ENTITY_MOVING_TO_END;
        }
    }
}

bool GhoulsGame::positionExistsInLevel(Level *level, Vector position)
{
    for (int i = 0; i < level->getEntityCount(); i++)
    {
        Entity *entity = level->getEntity(i);
        if (entity && entity->position == position)
        {
            return true;
        }
    }
    return false;
}

void GhoulsGame::refreshPlayer()
{
    player->health = player->max_health;
    Weapon *equippedWeapon = player->getEquippedWeapon();
    if (equippedWeapon)
    {
        // sets ammo to max and resets cooldown
        // no need to check for level here because
        // if weapon is allocated then level exists
        equippedWeapon->reset(engine->getGame()->current_level);
        equippedWeapon->setDamage(equippedWeapon->getDamage() + player->strength);
    }
}

void GhoulsGame::registerSpritePositionsOnMap()
{
    if (!player)
    {
        ENGINE_LOG_INFO("[GhoulsGame:registerSpritePositionsOnMap] Player instance is null");
        return;
    }
    if (!currentDynamicMap)
    {
        ENGINE_LOG_INFO("[GhoulsGame:registerSpritePositionsOnMap] Player's current dynamic map instance is null");
        return;
    }
    // registering as wall so players dont move through em
    // i think we'll restrict ghouls later too
    for (uint8_t i = 0; i < HOUSE_SPAWN_COUNT; i++)
    {
        currentDynamicMap->setTile(housePositions[i].x, housePositions[i].y, TILE_WALL);
    }
    for (uint8_t i = 0; i < TREE_SPAWN_COUNT; i++)
    {
        currentDynamicMap->setTile(treePositions[i].x, treePositions[i].y, TILE_WALL);
    }
    for (uint8_t i = 0; i < MAP_OUTER_WALLS; i++)
    {
        currentDynamicMap->setTile(wallPositions[i].x, wallPositions[i].y, TILE_WALL);
    }
}
bool GhoulsGame::removeGhoulsFromLevel()
{
    if (!engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:removeGhoulsFromLevel] Game engine instance is null");
        return false;
    }
    Game *game = engine->getGame();
    if (!game)
    {
        ENGINE_LOG_INFO("[GhoulsGame:removeGhoulsFromLevel] Game instance is null");
        return false;
    }
    Level *level = game->current_level;
    if (!level)
    {
        ENGINE_LOG_INFO("[GhoulsGame:removeGhoulsFromLevel] Current level instance is null");
        return false;
    }
    for (int i = 0; i < level->getEntityCount(); i++)
    {
        Entity *entity = level->getEntity(i);
        if (entity && entity->type == ENTITY_ENEMY)
        {
            level->entity_remove(entity);
            i--; // Adjust index after removal
        }
    }
    return true;
}

void GhoulsGame::renderEnvironment(Game *game)
{
    if (game)
    {
        // renderWalls(game);
        renderHouses(game);
        renderTrees(game);
    }
}

void GhoulsGame::renderHouses(Game *game)
{
    if (!houseSprite)
    {
        ENGINE_LOG_INFO("[GhoulsGame:renderHouses] House sprite is not initialized");
        return;
    }
    Level *currentLevel = game->current_level;
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:renderHouses] Current level instance is null");
        return;
    }
    for (int8_t i = HOUSE_SPAWN_COUNT - 1; i >= 0; i--)
    {
        const Vector &pos = housePositions[i];
        float dx = pos.x - player->position.x;
        float dy = pos.y - player->position.y;
        if (dx * dx + dy * dy > (float)(FIELD_OF_VIEW_SQUARED))
            continue;
        houseSprite->setPosition(pos);
        currentLevel->render3DSprite(houseSprite, draw, player->position, player->direction, game->camera->height);
    }
}

void GhoulsGame::renderTrees(Game *game)
{
    if (!treeSprite)
    {
        ENGINE_LOG_INFO("[GhoulsGame:renderTrees] Tree sprite is not initialized");
        return;
    }
    Level *currentLevel = game->current_level;
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:renderTrees] Current level instance is null");
        return;
    }
    for (int8_t i = TREE_SPAWN_COUNT - 1; i >= 0; i--)
    {
        const Vector &pos = treePositions[i];
        float dx = pos.x - player->position.x;
        float dy = pos.y - player->position.y;
        if (dx * dx + dy * dy > (float)(FIELD_OF_VIEW_SQUARED))
            continue;
        treeSprite->setPosition(pos);
        currentLevel->render3DSprite(treeSprite, draw, player->position, player->direction, game->camera->height);
    }
}

void GhoulsGame::renderWalls(Game *game)
{
    if (!wallSprite || !vWallSprite)
    {
        ENGINE_LOG_INFO("[GhoulsGame:renderWalls] Wall sprites are not initialized");
        return;
    }
    Level *currentLevel = game->current_level;
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:renderWalls] Current level instance is null");
        return;
    }
    for (int8_t i = MAP_OUTER_WALLS - 1; i >= 0; i--)
    {
        const Vector &pos = wallPositions[i];
        float dx = pos.x - player->position.x;
        float dy = pos.y - player->position.y;
        if (dx * dx + dy * dy > (float)(FIELD_OF_VIEW_SQUARED))
            continue;
        // indices 0,1 = horizontal (top/bottom); indices 2,3 = vertical (left/right)
        Sprite3D *sprite = (i < 2) ? wallSprite : vWallSprite;
        sprite->setPosition(pos);
        currentLevel->render3DSprite(sprite, draw, player->position, player->direction, game->camera->height);
    }
}

bool GhoulsGame::spawnGhouls()
{
    if (!player)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnGhouls] Player instance is null");
        return false;
    }
    if (!engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnGhouls] Game engine instance is null");
        return false;
    }
    Game *game = engine->getGame();
    if (!game)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnGhouls] Game instance is null");
        return false;
    }
    Level *level = game->current_level;
    if (!level)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnGhouls] Current level instance is null");
        return false;
    }
    for (uint16_t i = 0; i < currentRound; i++)
    {
        if (i >= ENEMY_SPAWN_MAX)
        {
            break;
        }
        Entity *ghoul = ENGINE_MEM_NEW Enemy("Ghoul", getRandomGhoulPosition(level), ENEMY_BULLY, 1.7f, 1.5f, 0.f, player->position);
        if (!ghoul)
        {
            ENGINE_LOG_INFO("[GhoulsGame:spawnGhouls] Failed to create Enemy instance for Ghoul");
            return false;
        }
        level->entity_add(ghoul);
    }
    return true;
}

Level *GhoulsGame::spawnLevel(Game *game)
{
    if (!player)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Player instance is null");
        return nullptr;
    }

    Level *newLevel = ENGINE_MEM_NEW Level("Level", draw->getDisplaySize(), game);
    if (!newLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Failed to create Level instance");
        return nullptr;
    }

    newLevel->entity_add(player);

    if (!spawnWeapons(newLevel))
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Failed to spawn weapons for the level");
        ENGINE_MEM_DELETE newLevel;
        return nullptr;
    }

    if (!setDynamicMap())
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Failed to load map for level");
        player->setLeaveGameToggle(ToggleOn);
        return nullptr;
    }

    if (!initializeSprites())
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Failed to initialize sprites for the level");
        ENGINE_MEM_DELETE newLevel;
        return nullptr;
    }

    registerSpritePositionsOnMap();

    // update 3D sprite position immediately after setting player position
    if (player->has3DSprite())
    {
        player->update3DSpritePosition();

        // Also ensure the sprite rotation and scale are set correctly
        player->set3DSpriteRotation(atan2f(player->direction.y, player->direction.x) + M_PI_2); // Face forward with orientation correction
        player->set3DSpriteScale(1.0f);                                                         // Normal scale
    }

    refreshPlayer();

    return newLevel;
}

bool GhoulsGame::spawnWeapons(Level *level)
{
    for (uint8_t i = 0; i < WEAPON_SPAWN_COUNT; i++)
    {
        Vector weaponPosition = getRandomWeaponPosition(level);
        WeaponType weaponType = getUniqueWeaponType(level);
        if (weaponType == WEAPON_NONE)
        {
            ENGINE_LOG_INFO("[GhoulsGame:spawnWeapons] No unique weapon type available for spawn");
            continue; // Skip spawning if no unique weapon type is available
        }
        Weapon *newWeapon = ENGINE_MEM_NEW Weapon(weaponType, 1.5f, weaponPosition);
        if (newWeapon)
        {
            level->entity_add(newWeapon);
        }
    }
    return true;
}

bool GhoulsGame::startGame()
{
    if (isGameRunning || engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Game is already running or engine is already initialized");
        return true;
    }

    // Create the game instance with 3rd person perspective
    auto camera = ENGINE_MEM_NEW Camera(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 0.66f, 0), 1.6f, 2.0f, CAMERA_THIRD_PERSON);
    if (!camera)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create Camera instance");
        return false;
    }

    auto game = ENGINE_MEM_NEW Game("Ghouls", draw->getDisplaySize(), draw, 0x0000, 0xFFFF, camera);
    if (!game)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create Game instance");
        return false;
    }

    // spawn initial level based on currentLevelIndex
    Level *initialLevel = spawnLevel(game);
    if (!initialLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to spawn initial level");
        return false;
    }
    game->level_add(initialLevel);

    this->engine = ENGINE_MEM_NEW GameEngine(game, 240);
    if (!this->engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create GameEngine");
        return false;
    }

    isGameRunning = true; // Set the flag to indicate game is running
    gameTime->reset();    // ensure day starts at 0
    return true;
}

bool GhoulsGame::startGameOnline()
{
    if (isGameRunning || engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Game is already running or engine is already initialized");
        return true;
    }

    // Create the game instance with 3rd person perspective
    auto camera = ENGINE_MEM_NEW Camera(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 0.66f, 0), 1.6f, 2.0f, CAMERA_THIRD_PERSON);
    if (!camera)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create Camera instance");
        return false;
    }

    auto game = ENGINE_MEM_NEW Game("Ghouls", draw->getDisplaySize(), draw, 0x0000, 0xFFFF, camera);
    if (!game)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create Game instance");
        return false;
    }

    // spawn initial level based on currentLevelIndex
    Level *initialLevel = spawnLevel(game);
    if (!initialLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to spawn initial level");
        return false;
    }
    game->level_add(initialLevel);

    this->engine = ENGINE_MEM_NEW GameEngine(game, 240);
    if (!this->engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create GameEngine");
        return false;
    }

    isGameRunning = true; // Set the flag to indicate game is running
    gameTime->reset();    // ensure day starts at 0
    return true;
}

// called by the platform in a loop
void GhoulsGame::updateDraw()
{
    gameTime->tick();

    /*
    During the day:
        - ghouls return to spawn (makeGhoulsGoHome)
    During the night:
        - remove previous ghouls
        - spawn new ghouls based on current round (spawnGhouls)
        - increment round (currentRound++)
        - ghouls target player (makeGhoulsGoToPlayer)
    */

    if (gameTime->getTimeOfDay() == TIME_DAY)
    {
        if (!dayJustSwitched)
        {
            dayJustSwitched = true;
            // im not deleting here since I want the player
            // to see the ghouls walking back to their spawns
            makeGhoulsGoHome();
            // we could set sky n stuff here too
            player->showAlert("You survived the night.. for now");
        }
    }
    else
    {
        if (dayJustSwitched)
        {
            dayJustSwitched = false; // switching to night
            // remove old ghouls
            if (!removeGhoulsFromLevel())
            {
                ENGINE_LOG_INFO("[GhoulsGame:updateDraw] Failed to remove ghouls for the night");
                return;
            }
            // spawn new ghouls for the night based on current round
            if (!spawnGhouls())
            {
                ENGINE_LOG_INFO("[GhoulsGame:updateDraw] Failed to spawn ghouls for the night");
                return;
            }
            // increase difficulty of ghouls each night
            increaseDifficulty();
            // make ghouls attack player
            makeGhoulsGoToPlayer();
            // we could set sky n stuff here too
            currentRound++;  // Increment round (for next night)
            refreshPlayer(); // refresh player state to update weapon and health displays after day ends
            player->showAlert("The ghouls are coming...");
        }
    }

    // Let the player handle all drawing
    if (player)
    {
        player->drawCurrentView(draw);
    }
}

// called by the platform when input is received
void GhoulsGame::updateInput(int key, bool held)
{
    (void)held; // not used for now

    this->lastInput = key;

    // Only run inputManager when not in an active game to avoid input conflicts
    if (!(player && (player->getCurrentMainView() == GameViewGameLocal || player->getCurrentMainView() == GameViewGameOnline) && this->isGameRunning))
    {
        this->inputManager();
    }
}