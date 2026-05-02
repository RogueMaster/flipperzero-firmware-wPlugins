#include "game.hpp"
#include "level.hpp"
#include "pico-game-engine/engine/draw.hpp"
#include "pico-game-engine/engine/game.hpp"
#include "pico-game-engine/engine/engine.hpp"
#include <math.h>
#include <stdio.h>

GhoulsGame::GhoulsGame(const char *username, const char *password, bool soundEnabled)
{
    // pass username and password to player
    player = ENGINE_MEM_NEW Player(username, password);
    if (!player)
    {
        ENGINE_LOG_INFO("[GhoulsGame:GhoulsGame] Failed to create Player instance\n");
        return;
    }
    player->setGhoulsGame(this);
    player->setSoundToggle(soundEnabled ? ToggleOn : ToggleOff);

    gameTime = ENGINE_MEM_NEW Time();
    if (!gameTime)
    {
        ENGINE_LOG_INFO("[GhoulsGame:GhoulsGame] Failed to create Time instance\n");
        ENGINE_MEM_DELETE player;
        player = nullptr;
        return;
    }

    gameSound = ENGINE_MEM_NEW Sound();
    if (!gameSound)
    {
        ENGINE_LOG_INFO("[GhoulsGame:GhoulsGame] Failed to create Sound instance\n");
        ENGINE_MEM_DELETE gameTime;
        gameTime = nullptr;
        ENGINE_MEM_DELETE player;
        player = nullptr;
        return;
    }
}

GhoulsGame::~GhoulsGame()
{
    this->endGame();
    if (engine)
    {
        engine->stop();
        ENGINE_MEM_DELETE engine;
        engine = nullptr;
    }
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
    if (gameSound)
    {
        ENGINE_MEM_DELETE gameSound;
        gameSound = nullptr;
    }
    if (draw)
    {
        ENGINE_MEM_DELETE draw;
        draw = nullptr;
    }
}

void GhoulsGame::endGame()
{
    shouldExit = true;
    isGameRunning = false;
}

GhoulsLevel *GhoulsGame::getCurrentLevel() const
{
    if (!engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:getCurrentLevel] Game engine instance is null\n");
        return nullptr;
    }
    Game *game = engine->getGame();
    if (game)
    {
        return static_cast<GhoulsLevel *>(game->current_level);
    }
    return nullptr;
}

Game *GhoulsGame::getGame() const
{
    if (engine)
    {
        return engine->getGame();
    }
    return nullptr;
}

Vector GhoulsGame::getRandomGhoulPosition(Level *level)
{
    // possible ghoul spawns
    GhoulsLevel *ghoulsLevel = static_cast<GhoulsLevel *>(level);
    map_data_t *mapData = ghoulsLevel->getMapData();

    uint8_t randomIndex = 0;
    uint8_t attempts = 0;
    do
    {
        randomIndex = rand() % (mapData->ghoulCount);
        attempts++;
        if (attempts > mapData->ghoulCount)
        {
            break;
        }
    } while (positionExistsInLevel(level, mapData->ghoulPositions[randomIndex]));
    return mapData->ghoulPositions[randomIndex];
}

EnemyType GhoulsGame::getRandomGhoulType() const
{
    const EnemyType ghoulTypes[] = {ENEMY_BULLY, ENEMY_CREEPER, ENEMY_PUNK};
    const uint8_t randomIndex = rand() % (sizeof(ghoulTypes) / sizeof(ghoulTypes[0]));
    return ghoulTypes[randomIndex];
}

Vector GhoulsGame::getRandomWeaponPosition(Level *level)
{
    // possible weapon spawns
    GhoulsLevel *ghoulsLevel = static_cast<GhoulsLevel *>(level);
    map_data_t *mapData = ghoulsLevel->getMapData();

    uint8_t randomIndex = 0;
    uint8_t attempts = 0;
    Vector candidate;
    do
    {
        randomIndex = rand() % (mapData->weaponCount);
        candidate = mapData->weaponPositions[randomIndex];
        candidate.z = 0.5f;
        attempts++;
        if (attempts > mapData->weaponCount)
        {
            break;
        }
    } while (positionExistsInLevel(level, candidate));
    return mapData->weaponPositions[randomIndex];
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
    GhoulsLevel *currentLevel = getCurrentLevel();
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:increaseDifficulty] Current level instance is null\n");
        return;
    }
    const uint16_t decrement = currentRound - 1;
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(entity);
            if (enemy)
            {
                enemy->max_health += (ENEMY_HEALTH_INCREMENT * decrement); // increase max health based on current round
                enemy->health = enemy->max_health;                         // restore health to max when stats are updated
                enemy->strength += (ENEMY_STRENGTH_INCREMENT * decrement); // increase strength based on current round
            }
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
            ENGINE_LOG_INFO("[GhoulsGame:initDraw] Failed to create Draw instance\n");
            return false;
        }
    }
    return true;
}

bool GhoulsGame::isDay() const
{
    if (!gameTime)
    {
        ENGINE_LOG_INFO("[GhoulsGame:isDay] Game time instance is null\n");
        return true; // default to day
    }
    return gameTime->getTimeOfDay() == TIME_DAY;
}

void GhoulsGame::inputManager()
{
    // Pass input to player for processing
    if (player)
    {
        player->setInputKey(lastInput);
        player->processInput();
        resetInput();
    }
}

void GhoulsGame::makeGhoulsGoHome()
{
    GhoulsLevel *currentLevel = getCurrentLevel();
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:makeGhoulsGoHome] Current level instance is null\n");
        return;
    }
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(entity);
            if (enemy)
            {
                enemy->state = ENTITY_MOVING_TO_START;
            }
        }
    }
    ghoulCountCurrent = 0;
    ghoulCountSpawned = 0;
    ghoulCountTotal = 0;
}

void GhoulsGame::makeGhoulsGoToPlayer()
{
    GhoulsLevel *currentLevel = getCurrentLevel();
    if (!currentLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:makeGhoulsGoToPlayer] Current level instance is null\n");
        return;
    }
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(entity);
            if (enemy)
            {
                enemy->state = ENTITY_MOVING_TO_END;
            }
        }
    }
}

void GhoulsGame::onGhoulDied()
{
    if (ghoulCountCurrent > 0)
    {
        ghoulCountCurrent--;
    }
    if (ghoulCountSpawned < ghoulCountTotal)
    {
        spawnOneGhoul();
    }
    if (ghoulCountCurrent == 0 && ghoulCountSpawned >= ghoulCountTotal)
    {
        ghoulCountSpawned = 0;
        ghoulCountTotal = 0;
        if (gameTime->getTimeOfDay() == TIME_NIGHT)
        {
            gameTime->setTimeOfDay(TIME_DAY);
        }
    }
}

bool GhoulsGame::positionExistsInLevel(Level *level, Vector position)
{
    // check entities
    for (int i = 0; i < level->getEntityCount(); i++)
    {
        Entity *entity = level->getEntity(i);
        if (entity && entity->position == position)
        {
            return true;
        }
    }
    // check trees/houses
    GhoulsLevel *currentLevel = static_cast<GhoulsLevel *>(level);
    return currentLevel && (!currentLevel->isPositionAvailable(position) || currentLevel->collisionMapCheck(position));
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
        const uint16_t maxAmmo = equippedWeapon->getAmmoMax();
        equippedWeapon->addMaxAmmo(maxAmmo - equippedWeapon->getAmmo()); // increase max ammo to current + full magazine
        equippedWeapon->addAmmo(maxAmmo);                                // refill to current + full magazine
        equippedWeapon->setDamage(equippedWeapon->getDamage() + (player->strength / 2));
    }
}

bool GhoulsGame::removeGhoulsFromLevel()
{
    GhoulsLevel *level = getCurrentLevel();
    if (!level)
    {
        ENGINE_LOG_INFO("[GhoulsGame:removeGhoulsFromLevel] Current level instance is null\n");
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

#if GROUND_RENDER_ALLOWED
bool GhoulsGame::setGroundType(TimeOfDay timeOfDay)
{
    GhoulsLevel *level = getCurrentLevel();
    if (!level)
    {
        ENGINE_LOG_INFO("[GhoulsGame:setGroundType] Current level instance is null\n");
        return false;
    }
    Ground *ground = level->getGround();
    if (!ground)
    {
        ENGINE_LOG_INFO("[GhoulsGame:setGroundType] Ground instance is null\n");
        return false;
    }
    ground->setGround(timeOfDay == TIME_DAY ? level->getMapData()->groundDayGradient : level->getMapData()->groundNightGradient);
    return true;
}
#endif

#if SKY_RENDER_ALLOWED
bool GhoulsGame::setSkyType(TimeOfDay timeOfDay)
{
    GhoulsLevel *level = getCurrentLevel();
    if (!level)
    {
        ENGINE_LOG_INFO("[GhoulsGame:setSkyType] Current level instance is null\n");
        return false;
    }
    Sky *sky = level->getSky();
    if (!sky)
    {
        ENGINE_LOG_INFO("[GhoulsGame:setSkyType] Sky instance is null\n");
        return false;
    }
    sky->setSky(timeOfDay == TIME_DAY ? level->getMapData()->skyDayGradient : level->getMapData()->skyNightGradient);
    return true;
}
#endif

bool GhoulsGame::spawnOneGhoul()
{
    GhoulsLevel *level = getCurrentLevel();
    if (!level)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnOneGhoul] Current level instance is null\n");
        return false;
    }
    Entity *ghoul = ENGINE_MEM_NEW Enemy("Ghoul", getRandomGhoulPosition(level), getRandomGhoulType(), 1.7f, 1.5f, 0.f, player->position);
    if (!ghoul)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnOneGhoul] Failed to create Enemy instance for Ghoul\n");
        return false;
    }
    level->entity_add(ghoul);
    ghoulCountSpawned++;
    ghoulCountCurrent++;
    return true;
}

bool GhoulsGame::spawnGhouls(uint8_t count)
{
    ghoulCountTotal = count;
    ghoulCountSpawned = 0;
    ghoulCountCurrent = 0;
    const uint8_t initialSpawn = count < ENEMY_SPAWN_MAX ? count : ENEMY_SPAWN_MAX;
    for (uint8_t i = 0; i < initialSpawn; i++)
    {
        if (!spawnOneGhoul())
        {
            return false;
        }
    }
    return true;
}

GhoulsLevel *GhoulsGame::spawnLevel(Game *game)
{
    if (!player)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Player instance is null\n");
        return nullptr;
    }

    GhoulsLevel *newLevel = ENGINE_MEM_NEW GhoulsLevel("Level", draw->getDisplaySize(), game, this,
                                                       selectedMapFile[0] != '\0' ? selectedMapFile : ASSETS_FOLDER "home.ghoulsmap");
    if (!newLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Failed to create Level instance\n");
        return nullptr;
    }

    newLevel->entity_add(player);
    newLevel->setClearAllowed(false);

    if (!spawnWeapons(newLevel))
    {
        ENGINE_LOG_INFO("[GhoulsGame:spawnLevel] Failed to spawn weapons for the level\n");
        ENGINE_MEM_DELETE newLevel;
        return nullptr;
    }

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
        weaponPosition.z = 0.5f;
        WeaponType weaponType = getUniqueWeaponType(level);
        if (weaponType == WEAPON_NONE)
        {
            ENGINE_LOG_INFO("[GhoulsGame:spawnWeapons] No unique weapon type available for spawn\n");
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

bool GhoulsGame::soundAllowed() const
{
    return player && player->getSoundToggle() == ToggleOn;
}

void GhoulsGame::setSelectedMapFile(const char *filename)
{
    if (filename && filename[0] != '\0')
    {
        snprintf(selectedMapFile, sizeof(selectedMapFile) - 1, "%s", filename);
    }
    else
    {
        selectedMapFile[0] = '\0';
    }
}

bool GhoulsGame::startGame()
{
    if (isGameRunning || engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Game is already running or engine is already initialized\n");
        return true;
    }

    // Create the game instance with 3rd person perspective
    auto camera = ENGINE_MEM_NEW Camera(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 0.66f, 0), 1.6f, 2.0f, CAMERA_THIRD_PERSON);
    if (!camera)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create Camera instance\n");
        return false;
    }

    auto game = ENGINE_MEM_NEW Game("Ghouls", draw->getDisplaySize(), draw, 0x0000, 0xFFFF, camera);
    if (!game)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create Game instance\n");
        return false;
    }

    // spawn initial level based on currentLevelIndex
    Level *initialLevel = spawnLevel(game);
    if (!initialLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to spawn initial level\n");
        return false;
    }
    game->level_add(initialLevel);

    this->engine = ENGINE_MEM_NEW GameEngine(game, 240);
    if (!this->engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGame] Failed to create GameEngine\n");
        return false;
    }

    isGameRunning = true; // Set the flag to indicate game is running
    gameTime->reset();    // ensure day starts at 0
    if (soundAllowed())
    {
        gameSound->stop();
        gameSound->playWAV(ASSETS_FOLDER "ambience.wav");
    }
    player->showAlert("Find weapons before the night..", 120);
    return true;
}

bool GhoulsGame::startGameOnline()
{
    if (isGameRunning || engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGameOnline] Game is already running or engine is already initialized\n");
        return true;
    }

    // Create the game instance with 3rd person perspective
    auto camera = ENGINE_MEM_NEW Camera(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 0.66f, 0), 1.6f, 2.0f, CAMERA_THIRD_PERSON);
    if (!camera)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGameOnline] Failed to create Camera instance\n");
        return false;
    }

    auto game = ENGINE_MEM_NEW Game("Ghouls", draw->getDisplaySize(), draw, 0x0000, 0xFFFF, camera);
    if (!game)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGameOnline] Failed to create Game instance\n");
        return false;
    }

    // spawn initial level based on currentLevelIndex
    Level *initialLevel = spawnLevel(game);
    if (!initialLevel)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGameOnline] Failed to spawn initial level\n");
        return false;
    }
    game->level_add(initialLevel);

    this->engine = ENGINE_MEM_NEW GameEngine(game, 240);
    if (!this->engine)
    {
        ENGINE_LOG_INFO("[GhoulsGame:startGameOnline] Failed to create GameEngine\n");
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
    gameSound->tick();

    /*
    During the day:
        - ghouls return to spawn (makeGhoulsGoHome)
    During the night:
        - remove previous ghouls
        - spawn new ghouls based on current round (spawnGhouls)
        - increment round (currentRound++)
        - ghouls target player (makeGhoulsGoToPlayer)
    */

    if (isDay())
    {
        if (!dayJustSwitched)
        {
            dayJustSwitched = true;
            // im not deleting here since I want the player
            // to see the ghouls walking back to their spawns
            makeGhoulsGoHome();
#if SKY_RENDER_ALLOWED
            setSkyType(TIME_DAY);
#endif
#if GROUND_RENDER_ALLOWED
            setGroundType(TIME_DAY);
#endif
            if (soundAllowed())
            {
                gameSound->stop();
                gameSound->playWAV(ASSETS_FOLDER "ambience.wav");
            }
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
                ENGINE_LOG_INFO("[GhoulsGame:updateDraw] Failed to remove ghouls for the night\n");
                return;
            }
            // spawn new ghouls for the night based on current round
            if (!spawnGhouls(currentRound))
            {
                ENGINE_LOG_INFO("[GhoulsGame:updateDraw] Failed to spawn ghouls for the night\n");
                return;
            }
            // increase difficulty of ghouls each night
            increaseDifficulty();
            // make ghouls attack player
            makeGhoulsGoToPlayer();
#if SKY_RENDER_ALLOWED
            setSkyType(TIME_NIGHT);
#endif
#if GROUND_RENDER_ALLOWED
            setGroundType(TIME_NIGHT);
#endif
            currentRound++;  // Increment round (for next night)
            refreshPlayer(); // refresh player state to update weapon and health displays after day ends
            if (soundAllowed())
            {
                gameSound->stop();
                gameSound->playWAV(ASSETS_FOLDER "ambience.wav");
                gameSound->playWAV(ASSETS_FOLDER "ghouls-growling.wav");
            }
            player->showAlert("The ghouls are coming...");
        }
    }

    draw->fillScreen(0xFFFF);
    player->drawCurrentView(draw);
    draw->swap();
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