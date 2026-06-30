#include "game/game.hpp"
#include "app.hpp"
#include "free_roam_icons.h"
#include "engine/draw.hpp"
#include "engine/game.hpp"
#include "engine/engine.hpp"
#include "game/sprites.hpp"
#include <math.h>
#include "lcd.hpp"

FreeRoamGame::FreeRoamGame()
{
    // nothing to do
}

FreeRoamGame::~FreeRoamGame()
{
    lcd_deinit();
}

void FreeRoamGame::endGame()
{
    shouldReturnToMenu = true;
    isGameRunning = false;

    this->updateSoundToggle();
    this->updateVibrationToggle();

    if (engine)
    {
        engine->stop();
        engine.reset();
    }

    if (draw)
    {
        draw.reset();
    }
}

bool FreeRoamGame::init(void *appContext)
{
    this->appContext = appContext;

    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E("FreeRoamGame", "App context is null");
        return false;
    }

    // Load sound and vibration toggle states from app settings
    soundToggle = app->isSoundEnabled() ? ToggleOn : ToggleOff;
    vibrationToggle = app->isVibrationEnabled() ? ToggleOn : ToggleOff;

    // Check board connection before proceeding
    if (app->isBoardConnected())
    {
        return true;
    }
    else
    {
        FURI_LOG_E("FreeRoamGame", "Board is not connected");
        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
        endGame(); // End the game if board is not connected
        return false;
    }
}

void FreeRoamGame::inputManager()
{
    // Pass input to player for processing
    if (player)
    {
        player->setInputKey(lastInput);
        player->processInput();
    }
}

bool FreeRoamGame::startGame()
{
    if (isGameRunning || engine)
    {
        FURI_LOG_E("FreeRoamGame", "Game already running, skipping start");
        return true;
    }

    // Create the game instance with 3rd person perspective
    // camera.release() transfers ownership to Game so it isn't deleted when startGame() returns
    auto camera = std::make_unique<Camera>(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 0.66f, 0), 1.6f, 2.0f, CAMERA_THIRD_PERSON);
    if (!camera)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create Camera object");
        return false;
    }

    auto game = std::make_unique<Game>("Free Roam", draw->getDisplaySize(), draw.get(), 0x0000, 0xFFFF, camera.release());
    if (!game)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create Game object");
        return false;
    }

    // Create the player instance if it doesn't exist
    if (!player)
    {
        // pass username to player
        FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
        furi_check(app);
        char username[64] = {0};
        if (!app->load_char("user_name", username, sizeof(username), "flipper_http"))
        {
            snprintf(username, sizeof(username), "Player");
        }
        player = std::make_unique<Player>(username);
        if (!player)
        {
            FURI_LOG_E("FreeRoamGame", "Failed to create Player object");
            return false;
        }
    }

    // set sound/vibration toggle states
    player->setSoundToggle(soundToggle);
    player->setVibrationToggle(vibrationToggle);

    draw->fillScreen(0xFFFF);
    draw->text(Vector(0, 10), "Adding levels and player...", 0x0000);

    // add levels and player to the game
    std::unique_ptr<Level> level1 = std::make_unique<Level>("Tutorial", draw->getDisplaySize(), game.get());
    std::unique_ptr<Level> level2 = std::make_unique<Level>("First", draw->getDisplaySize(), game.get());
    std::unique_ptr<Level> level3 = std::make_unique<Level>("Second", draw->getDisplaySize(), game.get());

    level1->entity_add(player.get());
    level2->entity_add(player.get());
    level3->entity_add(player.get());

    // add some 3D sprites
    std::unique_ptr<Entity> tutorialGuard1 = std::make_unique<Sprite>("Tutorial Guard 1", Vector(3, 7), SPRITE_3D_HUMANOID, 1.7f, M_PI / 4, 0.f, Vector(9, 7));
    std::unique_ptr<Entity> tutorialGuard2 = std::make_unique<Sprite>("Tutorial Guard 2", Vector(6, 2), SPRITE_3D_HUMANOID, 1.7f, M_PI / 4, 0.f, Vector(1, 2));
    level1->entity_add(tutorialGuard1.release());
    level1->entity_add(tutorialGuard2.release());

    // Town (level 2) - 4 houses spread across the open map
    std::unique_ptr<Entity> house1 = std::make_unique<Sprite>("House 1", Vector(14, 13), SPRITE_3D_HOUSE, 10.0f, 10.0f, 0.0f);
    std::unique_ptr<Entity> house2 = std::make_unique<Sprite>("House 2", Vector(48, 13), SPRITE_3D_HOUSE, 10.0f, 10.0f, (float)(M_PI / 2.0));
    std::unique_ptr<Entity> house3 = std::make_unique<Sprite>("House 3", Vector(14, 43), SPRITE_3D_HOUSE, 10.0f, 10.0f, (float)(M_PI));
    std::unique_ptr<Entity> house4 = std::make_unique<Sprite>("House 4", Vector(48, 43), SPRITE_3D_HOUSE, 10.0f, 10.0f, (float)(M_PI * 1.5));
    level2->entity_add(house1.release());
    level2->entity_add(house2.release());
    level2->entity_add(house3.release());
    level2->entity_add(house4.release());

    // Forest (level 3) - trees scattered throughout the open map
    const Vector treePositions[] = {
        Vector(8, 8),
        Vector(20, 6),
        Vector(35, 9),
        Vector(50, 7),
        Vector(6, 20),
        Vector(18, 22),
        Vector(30, 18),
        Vector(44, 24),
        Vector(55, 15),
        Vector(10, 35),
        Vector(25, 40),
        Vector(40, 36),
        Vector(52, 42),
        Vector(8, 48),
        Vector(22, 50),
        Vector(38, 48),
    };
    static const char *treeNames[] = {
        "Tree 1",
        "Tree 2",
        "Tree 3",
        "Tree 4",
        "Tree 5",
        "Tree 6",
        "Tree 7",
        "Tree 8",
        "Tree 9",
        "Tree 10",
        "Tree 11",
        "Tree 12",
        "Tree 13",
        "Tree 14",
        "Tree 15",
        "Tree 16",
    };
    for (uint8_t i = 0; i < 16; i++)
    {
        level3->entity_add(new Sprite(treeNames[i], treePositions[i], SPRITE_3D_TREE, 3.0f, 1.0f, 0.0f));
    }

    game->level_add(level1.release());
    game->level_add(level2.release());
    game->level_add(level3.release());

    this->engine = std::make_unique<GameEngine>(game.release(), 240);
    if (!this->engine)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create GameEngine");
        return false;
    }

    draw->fillScreen(0xFFFF);
    draw->text(Vector(0, 10), "Starting game engine...", 0x0000);

    isGameRunning = true; // Set the flag to indicate game is running
    return true;
}

bool FreeRoamGame::startGameOnline()
{
    if (isGameRunning || engine)
    {
        FURI_LOG_E("FreeRoamGame", "Game already running, skipping start");
        return true;
    }

    // Create the game instance with 3rd person perspective
    // camera.release() transfers ownership to Game so it isn't deleted when startGame() returns
    auto camera = std::make_unique<Camera>(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 0.66f, 0), 1.6f, 2.0f, CAMERA_THIRD_PERSON);
    auto game = std::make_unique<Game>("Free Roam", draw->getDisplaySize(), draw.get(), 0x0000, 0xFFFF, camera.release());
    if (!game)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create Game object");
        return false;
    }

    // Create the player instance if it doesn't exist
    if (!player)
    {
        // pass username to player
        FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
        furi_check(app);
        char username[64] = {0};
        if (!app->load_char("user_name", username, sizeof(username), "flipper_http"))
        {
            snprintf(username, sizeof(username), "Player");
        }
        player = std::make_unique<Player>(username);
        if (!player)
        {
            FURI_LOG_E("FreeRoamGame", "Failed to create Player object");
            return false;
        }
    }

    // set sound/vibration toggle states
    player->setSoundToggle(soundToggle);
    player->setVibrationToggle(vibrationToggle);

    // Online multiplayer level — open 64x57 gathering world
    std::unique_ptr<Level> level_online = std::make_unique<Level>("Online", draw->getDisplaySize(), game.get());
    level_online->entity_add(player.get());

    // 4 houses in the four quadrants (mirrors town layout)
    level_online->entity_add(new Sprite("House 1", Vector(14, 13), SPRITE_3D_HOUSE, 10.0f, 10.0f, 0.0f));
    level_online->entity_add(new Sprite("House 2", Vector(48, 13), SPRITE_3D_HOUSE, 10.0f, 10.0f, (float)(M_PI / 2.0)));
    level_online->entity_add(new Sprite("House 3", Vector(14, 43), SPRITE_3D_HOUSE, 10.0f, 10.0f, (float)(M_PI)));
    level_online->entity_add(new Sprite("House 4", Vector(48, 43), SPRITE_3D_HOUSE, 10.0f, 10.0f, (float)(M_PI * 1.5)));

    // Trees bordering the perimeter and accenting corners/paths
    const Vector onlineTreePositions[] = {
        Vector(8, 5),
        Vector(20, 5),
        Vector(40, 5),
        Vector(55, 5),
        Vector(5, 15),
        Vector(25, 12),
        Vector(38, 12),
        Vector(58, 15),
        Vector(5, 28),
        Vector(58, 28),
        Vector(5, 42),
        Vector(25, 45),
        Vector(38, 45),
        Vector(58, 42),
        Vector(8, 51),
        Vector(55, 51),
    };
    static const char *onlineTreeNames[] = {
        "OTree 1",
        "OTree 2",
        "OTree 3",
        "OTree 4",
        "OTree 5",
        "OTree 6",
        "OTree 7",
        "OTree 8",
        "OTree 9",
        "OTree 10",
        "OTree 11",
        "OTree 12",
        "OTree 13",
        "OTree 14",
        "OTree 15",
        "OTree 16",
    };
    for (uint8_t i = 0; i < 16; i++)
    {
        level_online->entity_add(new Sprite(onlineTreeNames[i], onlineTreePositions[i], SPRITE_3D_TREE, 3.0f, 1.0f, 0.0f));
    }

    game->level_add(level_online.release());

    this->engine = std::make_unique<GameEngine>(game.release(), 240);
    if (!this->engine)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create GameEngine");
        return false;
    }

    draw->fillScreen(0xFFFF);
    draw->text(Vector(0, 10), "Starting game engine...", 0x0000);

    isGameRunning = true; // Set the flag to indicate game is running
    return true;
}

void FreeRoamGame::switchToLevel(int levelIndex)
{
    if (!isGameRunning || !engine || !engine->getGame())
        return;

    // Ensure the level index is within bounds
    if (levelIndex < 0 || levelIndex >= totalLevels)
    {
        FURI_LOG_E("FreeRoamGame", "Invalid level index: %d", levelIndex);
        return;
    }

    currentLevelIndex = levelIndex;

    // Switch to the specified level using the engine's game instance
    engine->getGame()->level_switch(currentLevelIndex);

    // Force the Player to update its currentDynamicMap on next render
    if (player)
    {
        player->forceMapReload();
    }
}

void FreeRoamGame::switchToNextLevel()
{
    if (!isGameRunning || !engine || !engine->getGame())
    {
        FURI_LOG_W("FreeRoamGame", "Cannot switch level - game not running or engine not ready");
        return;
    }

    // Cycle to next level
    currentLevelIndex = (currentLevelIndex + 1) % totalLevels;

    // Switch to the new level using the engine's game instance
    engine->getGame()->level_switch(currentLevelIndex);

    // Force the Player to update its currentDynamicMap on next render
    if (player)
    {
        player->forceMapReload();
    }
}

void FreeRoamGame::updateDraw(Canvas *canvas)
{
    // set Draw instance
    if (!draw)
    {
        lcd_init_canvas(canvas);
        draw = std::make_unique<Draw>();
    }

    // Initialize player if not already done
    if (!player)
    {
        // pass username to player
        FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
        furi_check(app);
        char username[64] = {0};
        if (!app->load_char("user_name", username, sizeof(username), "flipper_http"))
        {
            snprintf(username, sizeof(username), "Player");
        }
        player = std::make_unique<Player>(username);
        if (player)
        {
            player->setFreeRoamGame(this);
            player->setSoundToggle(soundToggle);
            player->setVibrationToggle(vibrationToggle);
        }
    }

    // Let the player handle all drawing
    if (player)
    {
        player->drawCurrentView(draw.get());
    }
}

void FreeRoamGame::updateInput(InputEvent *event)
{
    if (!event)
    {
        FURI_LOG_E("FreeRoamGame", "updateInput: no event available");
        return;
    }

    this->lastInput = event->key;

    // Only run inputManager when not in an active game to avoid input conflicts
    if (!(player && (player->getCurrentMainView() == GameViewGameLocal || player->getCurrentMainView() == GameViewGameOnline) && this->isGameRunning))
    {
        this->inputManager();
    }
}

void FreeRoamGame::updateSoundToggle()
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (app)
    {
        app->setSoundEnabled(soundToggle == ToggleOn);
    }
}

void FreeRoamGame::updateVibrationToggle()
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (app)
    {
        app->setVibrationEnabled(vibrationToggle == ToggleOn);
    }
}
