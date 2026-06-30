#include "game/player.hpp"
#include "free_roam_icons.h"
#include "game/game.hpp"
#include "game/general.hpp"
#include "app.hpp"
#include "jsmn/jsmn.h"
#include <math.h>

Player::Player(const char *name) : Entity(name, ENTITY_PLAYER, Vector(10, 10), Vector(1.0f, 2.0f), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false, SPRITE_3D_HUMANOID, 0x0000)
{
    direction = Vector(1, 0);                            // facing east initially (better for 3rd person view)
    plane = Vector(0, 0.66);                             // camera plane perpendicular to direction
    is_player = true;                                    // Mark this entity as a player (so level doesn't delete it)
    end_position = Vector(10, 10);                       // Initialize end position
    start_position = Vector(10, 10);                     // Initialize start position
    strncpy(player_name, name, sizeof(player_name) - 1); // Copy default player name
    player_name[sizeof(player_name) - 1] = '\0';         // Ensure null termination
    name = player_name;                                  // Point Entity's name to our writable buffer
}

Player::~Player()
{
    // nothing to clean up
}

bool Player::collisionMapCheck(Vector new_position)
{
    if (currentDynamicMap == nullptr)
        return false;

    // Check multiple points around the player to prevent clipping through walls
    // This accounts for player size and floating point positions
    float offset = 0.2f; // Small offset to check around player position

    Vector checkPoints[] = {
        new_position,                                             // Center
        Vector(new_position.x - offset, new_position.y - offset), // Top-left
        Vector(new_position.x + offset, new_position.y - offset), // Top-right
        Vector(new_position.x - offset, new_position.y + offset), // Bottom-left
        Vector(new_position.x + offset, new_position.y + offset)  // Bottom-right
    };

    for (int i = 0; i < 5; i++)
    {
        Vector point = checkPoints[i];

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

void Player::drawCurrentView(Draw *canvas)
{
    if (!canvas)
        return;

    switch (currentMainView)
    {
    case GameViewTitle:
        drawTitleView(canvas);
        break;
    case GameViewSystemMenu:
        drawSystemMenuView(canvas);
        break;
    case GameViewLobbyMenu:
        drawLobbyMenuView(canvas);
        break;
    case GameViewGameLocal:
        drawGameLocalView(canvas);
        break;
    case GameViewGameOnline:
        drawGameOnlineView(canvas);
        break;
    case GameViewLobbyBrowser:
        drawLobbyBrowserView(canvas);
        break;
    case GameViewWelcome:
        drawWelcomeView(canvas);
        break;
    case GameViewLogin:
        drawLoginView(canvas);
        break;
    case GameViewRegistration:
        drawRegistrationView(canvas);
        break;
    case GameViewUserInfo:
        drawUserInfoView(canvas);
        break;
    default:
        canvas->fillScreen(0xFFFF);
        canvas->text(0, 10, "Unknown View", 0x0000);
        break;
    }
}

void Player::drawGameLocalView(Draw *canvas)
{
    if (freeRoamGame->isRunning())
    {
        if (freeRoamGame->getEngine())
        {
            if (shouldLeaveGame())
            {
                freeRoamGame->endGame();
                return;
            }
            freeRoamGame->getEngine()->updateGameInput(freeRoamGame->getCurrentInput());
            // Reset the input after processing to prevent it from being continuously pressed
            freeRoamGame->resetInput();
            freeRoamGame->getEngine()->runAsync(false);
        }
        return;
    }
    else
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(25, 32, "Starting Game...", 0x0000);
        bool gameStarted = freeRoamGame->startGame();
        if (gameStarted && freeRoamGame->getEngine())
        {
            freeRoamGame->getEngine()->runAsync(false); // Run the game engine immediately
        }
    }
}

void Player::drawGameOnlineView(Draw *canvas)
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
    furi_check(app);
    if (shouldLeaveGame())
    {
        if (!app->websocketStop())
        {
            FURI_LOG_E("Player", "Failed to stop WebSocket");
        }
        onlineGameState = OnlineStateIdle;
        freeRoamGame->endGame();
        return;
    }

    switch (onlineGameState)
    {
    // ── Phase 1: POST to jblanked.com/game-server/games/create/ ─────────────
    case OnlineStateIdle:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Connecting to server...", 0x0000);
        this->userRequest(RequestTypeGameCreate);
        break;
    // ── Phase 2: Wait for HTTP response, parse port, open WebSocket ──────────
    case OnlineStateFetchingSession:
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        static bool loadingStarted = false;
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            if (loading)
            {
                loading->setText("Creating game...");
            }
            loadingStarted = true;
        }

        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;

            char *response = (char *)malloc(512);
            if (!response)
            {
                onlineGameState = OnlineStateError;
                break;
            }
            if (app->load_char("game_session", response, 512))
            {
                char *port_str = get_json_value("port", response);
                if (port_str)
                {
                    onlinePort = (uint16_t)atoi(port_str);
                    ::free(port_str);

                    char *game_id_str = get_json_value("game_id", response);
                    if (game_id_str)
                    {
                        strncpy(onlineGameId, game_id_str, sizeof(onlineGameId) - 1);
                        onlineGameId[sizeof(onlineGameId) - 1] = '\0';
                        ::free(game_id_str);
                    }
                    char *websocket_url = (char *)malloc(128);
                    if (!websocket_url)
                    {
                        onlineGameState = OnlineStateError;
                        ::free(response);
                        break;
                    }
                    snprintf(websocket_url, 128, "ws://www.jblanked.com/ws/game-server/%s/", onlineGameId);
                    if (app->websocketStart(websocket_url, onlinePort))
                    {
                        onlineGameState = OnlineStateConnecting;
                    }
                    else
                    {
                        onlineGameState = OnlineStateError;
                    }
                    ::free(websocket_url);
                }
                else
                {
                    FURI_LOG_E("Player", "drawGameOnlineView: missing 'port' in game session response");
                    onlineGameState = OnlineStateError;
                }
            }
            else
            {
                FURI_LOG_E("Player", "drawGameOnlineView: failed to load game_session response");
                onlineGameState = OnlineStateError;
            }
            ::free(response);
        }
    }
    break;

    // ── Phase 3: Wait for [CONNECTED] from FlipperHTTP board ─────────────────
    case OnlineStateConnecting:
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);

        const char *lastResp = app->getLastResponse();
        if (lastResp && strstr(lastResp, "[SOCKET/CONNECTED]") != NULL)
        {
            onlineGameState = OnlineStatePlaying;
            if (!freeRoamGame->isRunning())
            {
                freeRoamGame->startGameOnline();
            }
            // Switch to the multiplayer level immediately so the correct map loads
            if (freeRoamGame->getEngine() && freeRoamGame->getEngine()->getGame())
            {
                freeRoamGame->getEngine()->getGame()->level_switch("Online");
            }
        }
        else
        {
            canvas->text(0, 10, "Connecting...", 0x0000);
        }
    }
    break;

    // ── Phase 4: Active online game ───────────────────────────────────────────
    case OnlineStatePlaying:
    {
        if (!freeRoamGame->isRunning())
        {
            canvas->fillScreen(0xFFFF);
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(25, 32, "Starting Game...", 0x0000);
            freeRoamGame->startGameOnline();
            if (freeRoamGame->getEngine() && freeRoamGame->getEngine()->getGame())
            {
                freeRoamGame->getEngine()->getGame()->level_switch("Online");
            }
            return;
        }

        // Send the current button input to the server as an integer string.
        // The server feeds this directly into game->input, so InputKey values
        // map 1-to-1 (0=Up, 1=Down, 2=Left, 3=Right, 4=Ok, 5=Back).
        InputKey currentInput = freeRoamGame->getCurrentInput();
        if (currentInput != InputKeyMAX)
        {
            if (currentInput != InputKeyBack)
            {
                if (gameState == GameStatePlaying)
                {
                    char inputMsg[96]; // {"username": "<username>", "input": <input>}
                    /*
                    up: 0
                    down 1:
                    right: 2
                    left: 3
                    ok: 4
                    */
                    snprintf(inputMsg, sizeof(inputMsg), "{\"username\": \"%s\", \"input\": %d}", player_name, (int)currentInput);
                    FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
                    furi_check(app);
                    app->websocketSend(inputMsg);
                    // Prevent the local engine from also moving the player — the server
                    // position update below is the authoritative source.
                    if (freeRoamGame->getEngine())
                    {
                        freeRoamGame->getEngine()->updateGameInput(InputKeyMAX);
                    }
                }
                else // menu
                {
                    if (freeRoamGame->getEngine())
                    {
                        freeRoamGame->getEngine()->updateGameInput(currentInput);
                    }
                }
            }
            else
            {
                if (freeRoamGame->getEngine())
                {
                    freeRoamGame->getEngine()->updateGameInput(InputKeyBack);
                }
            }
            freeRoamGame->resetInput();
        }

        // Apply server-authoritative entity positions from the latest WebSocket
        // message, then let the local engine render the updated state.
        const char *lastResp = app->getLastResponse();
        if (lastResp && lastResp[0] != '\0')
        {
            if (strcmp(lastResp, "[SOCKET/STOPPED]") == 0)
            {
                FURI_LOG_E("Player", "WebSocket stopped unexpectedly");
                onlineGameState = OnlineStateError;
            }
            else
            {
                updateEntitiesFromServer(lastResp);
                app->clearLastResponse();
            }
        }

        if (freeRoamGame->getEngine())
        {
            freeRoamGame->getEngine()->runAsync(false);

            if (gameState != GameStateMenu)
            {
                // ── Name tag overlay ─────────────────────────────────────────────
                // After the 3D scene is drawn, project each humanoid entity's head
                // position to screen space and draw a small username label above it.
                Game *og = freeRoamGame->getEngine()->getGame();
                if (og && og->current_level)
                {
                    Camera *cam = og->getCamera();
                    Vector ss = og->draw->getDisplaySize(); // 128 x 64
                    float vh = cam->height;
                    int entityCount = og->current_level->getEntityCount();
                    for (int i = 0; i < entityCount; i++)
                    {
                        Entity *ent = og->current_level->getEntity(i);
                        if (!ent || !ent->is_active || !ent->name)
                            continue;
                        if (ent->sprite_3d_type != SPRITE_3D_HUMANOID)
                            continue;
                        // Skip local player when show-player is off
                        if (ent == static_cast<Entity *>(this) && showPlayerToggle == ToggleOff)
                            continue;

                        // Project the head (world y = 2.2) to screen coords
                        float wdx = ent->position.x - cam->position.x;
                        float wdz = ent->position.y - cam->position.y; // position.y = world Z
                        float wdy = 2.2f - vh;

                        float cx = wdx * (-cam->direction.y) + wdz * cam->direction.x;
                        float cz = wdx * cam->direction.x + wdz * cam->direction.y;
                        if (cz <= 0.1f)
                            continue; // behind camera

                        float sx = (cx / cz) * ss.y + ss.x * 0.5f;
                        float sy = (-wdy / cz) * ss.y + ss.y * 0.5f;
                        if (sx < 0 || sx >= ss.x || sy < 2 || sy >= ss.y - 2)
                            continue;

                        int tx = (int)sx - (int)(strlen(ent->name) * 3);
                        int ty = (int)sy;
                        if (tx < 0)
                            tx = 0;
                        og->draw->setFont(FONT_SIZE_SMALL);
                        og->draw->text(tx, ty, ent->name);
                    }
                }
            }
        }
    }
    break;

    // ── Error state ───────────────────────────────────────────────────────────
    case OnlineStateError:
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Connection failed!", 0x0000);
        canvas->text(0, 20, "Check network and", 0x0000);
        canvas->text(0, 30, "try again.", 0x0000);
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(0, 50, "Press BACK to return.", 0x0000);
    }
    break;

    // ── Join existing lobby (skip create, go straight to WS) ─────────────────
    case OnlineStateJoiningExisting:
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Joining game...", 0x0000);

        char *websocket_url = (char *)malloc(128);
        if (!websocket_url)
        {
            onlineGameState = OnlineStateError;
            break;
        }
        snprintf(websocket_url, 128, "ws://www.jblanked.com/ws/game-server/%s/", onlineGameId);
        if (app->websocketStart(websocket_url, onlinePort))
        {
            onlineGameState = OnlineStateConnecting;
        }
        else
        {
            onlineGameState = OnlineStateError;
        }
        ::free(websocket_url);
    }
    break;

    default:
        FURI_LOG_E("Player", "Unknown online game state: %d", onlineGameState);
        break;
    }
}

void Player::drawLobbyBrowserView(Draw *canvas)
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
    furi_check(app);

    if (!lobbyFetched)
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);

        static bool lobbyRequestStarted = false;
        if (!lobbyRequestStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            if (loading)
            {
                loading->setText("Fetching lobbies...");
            }
            userRequest(RequestTypeGameList);
            lobbyRequestStarted = true;
        }

        if (!httpRequestIsFinished())
        {
            if (loading)
                loading->animate();
            return;
        }

        if (loading)
            loading->stop();
        lobbyRequestStarted = false;

        // Parse the list of active game sessions
        lobbyCount = 0;
        char *response = (char *)malloc(1024);
        if (response)
        {
            if (app->load_char("game_list", response, 1024))
            {
                for (int i = 0; i < MAX_LOBBY_ENTRIES; i++)
                {
                    char *entry = get_json_array_value("games", i, response);
                    if (!entry)
                        break;

                    char *gid = get_json_value("game_id", entry);
                    char *gname = get_json_value("game_name", entry);

                    if (gid && gname)
                    {
                        strncpy(lobbyEntries[lobbyCount].game_id, gid, 36);
                        lobbyEntries[lobbyCount].game_id[36] = '\0';
                        strncpy(lobbyEntries[lobbyCount].game_name, gname, 63);
                        lobbyEntries[lobbyCount].game_name[63] = '\0';
                        lobbyCount++;
                    }
                    if (gid)
                        ::free(gid);
                    if (gname)
                        ::free(gname);
                    ::free(entry);
                }
            }
            ::free(response);
        }
        lobbyFetched = true;
        lobbySelectedIndex = 0;
        return;
    }

    // ── Draw the lobby list ──────────────────────────────────────────────────
    canvas->fillScreen(0xFFFF);
    drawRainEffect(canvas);

    canvas->setFont(FONT_SIZE_PRIMARY);
    canvas->text(20, 10, "Online Lobbies", 0x0000);

    // Total items: "New Game" at index 0, then existing lobbies
    int totalItems = 1 + lobbyCount;
    int startY = 18;
    int lineH = 10;
    int maxVisible = 4;

    int scrollOffset = 0;
    if (lobbySelectedIndex >= maxVisible)
        scrollOffset = lobbySelectedIndex - maxVisible + 1;

    canvas->setFont(FONT_SIZE_SMALL);
    for (int i = scrollOffset; i < totalItems && (i - scrollOffset) < maxVisible; i++)
    {
        int y = startY + (i - scrollOffset) * lineH;

        if (i == lobbySelectedIndex)
        {
            canvas->fillRectangle(0, y, 128, lineH, 0x0000);
            canvas->setColor(0xFFFF);
        }
        else
        {
            canvas->setColor(0x0000);
        }

        if (i == 0)
        {
            canvas->text(4, y + 8, "> New Game");
        }
        else
        {
            canvas->text(4, y + 8, lobbyEntries[i - 1].game_name);
        }
        canvas->setColor(0x0000);
    }

    // Scroll indicators
    if (scrollOffset > 0)
    {
        canvas->text(120, 20, "^", 0x0000);
    }
    if (scrollOffset + maxVisible < totalItems)
    {
        canvas->text(120, 55, "v", 0x0000);
    }

    canvas->setFont(FONT_SIZE_SMALL);
    canvas->text(4, 62, "OK:Select  BACK:Return", 0x0000);
}

void Player::drawLobbyMenuView(Draw *canvas)
{
    // draw lobby text
    drawMenuType1(canvas, currentLobbyMenuIndex, "Local", "Online");
}

void Player::drawLoginView(Draw *canvas)
{
    canvas->fillScreen(0xFFFF);
    canvas->setFont(FONT_SIZE_PRIMARY);
    static bool loadingStarted = false;
    switch (loginStatus)
    {
    case LoginWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Logging in...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            char *response = (char *)malloc(256);
            FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
            if (app && app->load_char("login", response, 256))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    loginStatus = LoginSuccess;
                    currentMainView = GameViewTitle; // switch to title view
                }
                else if (strstr(response, "User not found") != NULL)
                {
                    loginStatus = LoginNotStarted;
                    currentMainView = GameViewRegistration;
                    registrationStatus = RegistrationWaiting;
                    userRequest(RequestTypeRegistration);
                }
                else
                {
                    loginStatus = LoginRequestError;
                }
            }
            else
            {
                loginStatus = LoginRequestError;
            }
            ::free(response);
        }
        break;
    case LoginSuccess:
        canvas->text(0, 10, "Login successful!", 0x0000);
        canvas->text(0, 20, "Press OK to continue.", 0x0000);
        break;
    case LoginCredentialsMissing:
        canvas->text(0, 10, "Missing credentials!", 0x0000);
        canvas->text(0, 20, "Please set your username", 0x0000);
        canvas->text(0, 30, "and password in the app.", 0x0000);
        break;
    case LoginRequestError:
        canvas->text(0, 10, "Login request failed!", 0x0000);
        canvas->text(0, 20, "Check your network and", 0x0000);
        canvas->text(0, 30, "try again later.", 0x0000);
        break;
    default:
        canvas->text(0, 10, "Logging in...", 0x0000);
        break;
    }
}

void Player::drawMenuType1(Draw *canvas, uint8_t selectedIndex, const char *option1, const char *option2)
{
    canvas->fillScreen(0xFFFF);
    canvas->setFont(FONT_SIZE_SECONDARY);

    // rain effect
    drawRainEffect(canvas);

    // draw lobby text
    if (selectedIndex == 0)
    {
        canvas->fillRectangle(36, 16, 56, 16, 0x0000);
        canvas->setColor(0xFFFF);
        canvas->text(54, 27, option1);
        canvas->fillRectangle(36, 32, 56, 16, 0xFFFF);
        canvas->setColor(0x0000);
        canvas->text(54, 42, option2);
    }
    else if (selectedIndex == 1)
    {
        canvas->setColor(0xFFFF);
        canvas->fillRectangle(36, 16, 56, 16, 0xFFFF);
        canvas->setColor(0x0000);
        canvas->text(54, 27, option1);
        canvas->fillRectangle(36, 32, 56, 16, 0x0000);
        canvas->setColor(0xFFFF);
        canvas->text(54, 42, option2);
        canvas->setColor(0x0000);
    }
}

void Player::drawMenuType2(Draw *canvas, uint8_t selectedIndexMain, uint8_t selectedIndexSettings)
{
    canvas->fillScreen(0xFFFF);
    canvas->setColor(0x0000);

    switch (selectedIndexMain)
    {
    case 0: // profile
    {
        // draw info
        char health[32];
        char xp[32];
        char level[32];
        char strength[32];

        snprintf(level, sizeof(level), "Level   : %d", (int)this->level);
        snprintf(health, sizeof(health), "Health  : %d", (int)this->health);
        snprintf(xp, sizeof(xp), "XP      : %d", (int)this->xp);
        snprintf(strength, sizeof(strength), "Strength: %d", (int)this->strength);

        canvas->setFont(FONT_SIZE_PRIMARY);
        if (this->name == nullptr || strlen(this->name) == 0)
        {
            canvas->text(Vector(6, 16), "Unknown");
        }
        else
        {
            canvas->text(Vector(6, 16), this->name);
        }

        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(6, 30, level);
        canvas->text(6, 37, health);
        canvas->text(6, 44, xp);
        canvas->text(6, 51, strength);

        // draw a box around the selected option
        canvas->rectangle(76, 6, 46, 46, 0x0000);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(80, 16, "Profile");
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(80, 26, "Map");
        canvas->text(80, 36, "Settings");
        canvas->text(80, 46, "About");
    }
    break;
    case 1: // map
    {
        if (currentDynamicMap != nullptr)
        {
            currentDynamicMap->renderMiniMap(canvas, Vector(3, 3), Vector(70, 57), position, direction);
        }
        else
        {
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(6, 32, "No map loaded", 0x0000);
        }

        canvas->rectangle(76, 6, 46, 46, 0x0000);
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(80, 16, "Profile");
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(80, 26, "Map");
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(80, 36, "Settings");
        canvas->text(80, 46, "About");
    }
    break;
    case 2: // settings (sound on/off, vibration on/off, show player, leave game)
    {
        char soundStatus[16];
        char vibrationStatus[16];
        char showPlayerStatus[20];
        snprintf(soundStatus, sizeof(soundStatus), "Sound: %s", toggleToString(soundToggle));
        snprintf(vibrationStatus, sizeof(vibrationStatus), "Vibrate: %s", toggleToString(vibrationToggle));
        snprintf(showPlayerStatus, sizeof(showPlayerStatus), "Show Me: %s", toggleToString(showPlayerToggle));
        // draw settings info
        switch (selectedIndexSettings)
        {
        case 0: // none/default
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(6, 16, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(6, 27, soundStatus);
            canvas->text(6, 36, vibrationStatus);
            canvas->text(6, 45, showPlayerStatus);
            canvas->text(6, 54, "Leave Game");
            break;
        case 1: // sound
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(6, 16, "Settings");
            canvas->setFont(FONT_SIZE_LARGE);
            canvas->text(6, 27, soundStatus);
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(6, 36, vibrationStatus);
            canvas->text(6, 45, showPlayerStatus);
            canvas->text(6, 54, "Leave Game");
            break;
        case 2: // vibration
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(6, 16, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(6, 27, soundStatus);
            canvas->setFont(FONT_SIZE_LARGE);
            canvas->text(6, 36, vibrationStatus);
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(6, 45, showPlayerStatus);
            canvas->text(6, 54, "Leave Game");
            break;
        case 3: // show player
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(6, 16, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(6, 27, soundStatus);
            canvas->text(6, 36, vibrationStatus);
            canvas->setFont(FONT_SIZE_LARGE);
            canvas->text(6, 45, showPlayerStatus);
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(6, 54, "Leave Game");
            break;
        case 4: // leave game
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(6, 16, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(6, 27, soundStatus);
            canvas->text(6, 36, vibrationStatus);
            canvas->text(6, 45, showPlayerStatus);
            canvas->setFont(FONT_SIZE_LARGE);
            canvas->text(6, 54, "Leave Game");
            break;
        default:
            break;
        };
        canvas->rectangle(76, 6, 46, 46, 0x0000);
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(80, 16, "Profile");
        canvas->text(80, 26, "Map");
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(79, 36, "Settings");
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(80, 46, "About");
    }
    break;
    case 3: // about
    {
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(6, 16, "Free Roam");
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(6, 25, "Creator: JBlanked");
        canvas->text(6, 59, "www.github.com/jblanked");

        // draw a box around the selected option
        canvas->rectangle(76, 6, 46, 46, 0x0000);
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(80, 16, "Profile");
        canvas->text(80, 26, "Map");
        canvas->text(80, 36, "Settings");
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(80, 46, "About");
    }
    break;
    default:
        canvas->fillScreen(0xFFFF);
        canvas->text(0, 10, "Unknown Menu", 0x0000);
        break;
    };
}

void Player::drawRainEffect(Draw *canvas)
{
    // rain droplets/star droplets effect
    for (int i = 0; i < 8; i++)
    {
        // Use pseudo-random offsets based on frame and droplet index
        uint8_t seed = (rainFrame + i * 37) & 0xFF;
        uint8_t x = (rainFrame + seed * 13) & 0x7F;
        uint8_t y = (rainFrame * 2 + seed * 7 + i * 23) & 0x3F;

        // Draw star-like droplet
        canvas->pixel(x, y, 0x0000);
        canvas->pixel(x - 1, y, 0x0000);
        canvas->pixel(x + 1, y, 0x0000);
        canvas->pixel(x, y - 1, 0x0000);
        canvas->pixel(x, y + 1, 0x0000);
    }

    rainFrame += 1;
    if (rainFrame > 128)
    {
        rainFrame = 0;
    }
}

void Player::drawRegistrationView(Draw *canvas)
{
    canvas->fillScreen(0xFFFF);
    canvas->setFont(FONT_SIZE_PRIMARY);
    static bool loadingStarted = false;
    switch (registrationStatus)
    {
    case RegistrationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Registering...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            char *response = (char *)malloc(256);
            if (!response)
            {
                FURI_LOG_E("Player", "Failed to allocate memory for registration response");
                registrationStatus = RegistrationRequestError;
                return;
            }
            FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
            furi_check(app);
            if (app->load_char("register", response, 256))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    registrationStatus = RegistrationSuccess;
                    currentMainView = GameViewTitle; // switch to title view
                }
                else if (strstr(response, "Username or password not provided") != NULL)
                {
                    registrationStatus = RegistrationCredentialsMissing;
                }
                else if (strstr(response, "User already exists") != NULL)
                {
                    registrationStatus = RegistrationUserExists;
                }
                else
                {
                    registrationStatus = RegistrationRequestError;
                }
            }
            else
            {
                registrationStatus = RegistrationRequestError;
            }
            ::free(response);
        }
        break;
    case RegistrationSuccess:
        canvas->text(0, 10, "Registration successful!", 0x0000);
        canvas->text(0, 20, "Press OK to continue.", 0x0000);
        break;
    case RegistrationCredentialsMissing:
        canvas->text(0, 10, "Missing credentials!", 0x0000);
        canvas->text(0, 20, "Please update your username", 0x0000);
        canvas->text(0, 30, "and password in the settings.", 0x0000);
        break;
    case RegistrationRequestError:
        canvas->text(0, 10, "Registration request failed!", 0x0000);
        canvas->text(0, 20, "Check your network and", 0x0000);
        canvas->text(0, 30, "try again later.", 0x0000);
        break;
    default:
        canvas->text(0, 10, "Registering...", 0x0000);
        break;
    }
}

void Player::drawSystemMenuView(Draw *canvas)
{
    canvas->fillScreen(0xFFFF);
    canvas->setColor(0x0000);

    drawMenuType2(canvas, currentMenuIndex, currentSettingsIndex);
}

void Player::drawTitleView(Draw *canvas)
{
    // draw title text
    drawMenuType1(canvas, currentTitleIndex, "Start", "Menu");
}

void Player::drawUserInfoView(Draw *canvas)
{
    static bool loadingStarted = false;
    switch (userInfoStatus)
    {
    case UserInfoWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Fetching...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            canvas->text(0, 10, "Loading user info...", 0x0000);
            canvas->text(0, 20, "Please wait...", 0x0000);
            canvas->text(0, 30, "It may take up to 15 seconds.", 0x0000);
            char *response = (char *)malloc(512);
            if (!response)
            {
                FURI_LOG_E("Player", "Failed to allocate memory for user info response");
                userInfoStatus = UserInfoRequestError;
                if (loading)
                {
                    loading->stop();
                }
                loadingStarted = false;
                return;
            }
            FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
            if (app && app->load_char("user_info", response, 512))
            {
                userInfoStatus = UserInfoSuccess;
                // they're in! let's go
                char *game_stats = get_json_value("game_stats", response);
                if (!game_stats)
                {
                    FURI_LOG_E("Player", "Failed to parse game_stats");
                    userInfoStatus = UserInfoParseError;
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    ::free(response);
                    return;
                }
                canvas->fillScreen(0xFFFF);
                canvas->text(0, 10, "User info loaded!", 0x0000);
                char *username = get_json_value("username", game_stats);
                char *level = get_json_value("level", game_stats);
                char *xp = get_json_value("xp", game_stats);
                char *health = get_json_value("health", game_stats);
                char *strength = get_json_value("strength", game_stats);
                char *max_health = get_json_value("max_health", game_stats);
                if (!username || !level || !xp || !health || !strength || !max_health)
                {
                    FURI_LOG_E("Player", "Failed to parse user info");
                    userInfoStatus = UserInfoParseError;
                    if (username)
                        ::free(username);
                    if (level)
                        ::free(level);
                    if (xp)
                        ::free(xp);
                    if (health)
                        ::free(health);
                    if (strength)
                        ::free(strength);
                    if (max_health)
                        ::free(max_health);
                    ::free(game_stats);
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    ::free(response);
                    return;
                }

                // Update player info
                snprintf(player_name, sizeof(player_name), "%s", username);
                name = player_name;
                this->level = atoi(level);
                this->xp = atoi(xp);
                this->health = atoi(health);
                this->strength = atoi(strength);
                this->max_health = atoi(max_health);

                // clean em up gang
                ::free(username);
                ::free(level);
                ::free(xp);
                ::free(health);
                ::free(strength);
                ::free(max_health);
                ::free(game_stats);
                ::free(response);

                if (loading)
                {
                    loading->stop();
                }
                loadingStarted = false;

                if (currentLobbyMenuIndex == LobbyMenuLocal)
                {
                    currentMainView = GameViewGameLocal; // Switch to local game view
                    freeRoamGame->startGame();
                }
                else if (currentLobbyMenuIndex == LobbyMenuOnline)
                {
                    lobbyFetched = false; // Reset so browser fetches fresh data
                    lobbySelectedIndex = 0;
                    currentMainView = GameViewLobbyBrowser; // Show lobby browser instead of creating directly
                }
                return;
            }
            else
            {
                userInfoStatus = UserInfoRequestError;
            }
            ::free(response);
        }
        break;
    case UserInfoSuccess:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "User info loaded successfully!", 0x0000);
        canvas->text(0, 20, "Press OK to continue.", 0x0000);
        break;
    case UserInfoCredentialsMissing:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Missing credentials!", 0x0000);
        canvas->text(0, 20, "Please update your username", 0x0000);
        canvas->text(0, 30, "and password in the settings.", 0x0000);
        break;
    case UserInfoRequestError:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "User info request failed!", 0x0000);
        canvas->text(0, 20, "Check your network and", 0x0000);
        canvas->text(0, 30, "try again later.", 0x0000);
        break;
    case UserInfoParseError:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Failed to parse user info!", 0x0000);
        canvas->text(0, 20, "Try again...", 0x0000);
        break;
    default:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Loading user info...", 0x0000);
        break;
    }
}

void Player::drawWelcomeView(Draw *canvas)
{
    canvas->fillScreen(0xFFFF);

    // rain effect
    drawRainEffect(canvas);

    // Draw welcome text with blinking effect
    // Blink every 15 frames (show for 15, hide for 15)
    canvas->setFont(FONT_SIZE_SMALL);
    if ((welcomeFrame / 15) % 2 == 0)
    {
        canvas->text(34, 60, "Press OK to start", 0x0000);
    }
    welcomeFrame++;

    // Reset frame counter to prevent overflow
    if (welcomeFrame >= 30)
    {
        welcomeFrame = 0;
    }

    // Draw a box around the OK button
    canvas->fillRectangle(40, 25, 56, 16, 0x0000);
    canvas->setColor(0xFFFF);
    canvas->text(56, 35, "Welcome", 0xFFFF);
    canvas->setColor(0x0000);
}

Vector Player::findSafeSpawnPosition(const char *levelName)
{
    Vector defaultPos = start_position;

    if (strcmp(levelName, "Tutorial") == 0)
    {
        defaultPos.x = 10;
        defaultPos.y = 10; // Center of 20x20 tutorial map
    }
    else if (strcmp(levelName, "First") == 0)
    {
        // Try several safe positions in the First level
        Vector candidates[] = {
            Vector(12, 12), // Upper left room
            Vector(8, 15),  // Left side of middle room
            Vector(5, 12),  // Lower in middle room
            Vector(3, 12),  // Even lower
            Vector(20, 12), // Right side of middle room
        };

        for (int i = 0; i < 5; i++)
        {
            if (isPositionSafe(candidates[i]))
            {
                return candidates[i];
            }
        }
        defaultPos.x = 12;
        defaultPos.y = 12; // Fallback
    }
    else if (strcmp(levelName, "Second") == 0)
    {
        // Try several safe positions in the Second level
        Vector candidates[] = {
            Vector(12, 10), // Upper left room
            Vector(8, 8),   // Safe spot in starting room
            Vector(15, 10), // Another spot in starting room
            Vector(10, 12), // Lower in starting room
            Vector(35, 25), // Central hub
        };

        for (int i = 0; i < 5; i++)
        {
            if (isPositionSafe(candidates[i]))
            {
                return candidates[i];
            }
        }
        defaultPos.x = 12;
        defaultPos.y = 10; // Fallback
    }
    else if (strcmp(levelName, "Online") == 0)
    {
        defaultPos.x = 32;
        defaultPos.y = 28; // Center of 64x57 online map
    }

    return defaultPos;
}

void Player::handleMenu(Draw *draw, Game *game)
{
    if (!draw || !game)
    {
        return;
    }

    if (currentMenuIndex != MenuIndexSettings)
    {
        switch (game->input)
        {
        case InputKeyUp:
            if (currentMenuIndex > MenuIndexProfile)
            {
                currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
            }
            break;
        case InputKeyDown:
            if (currentMenuIndex < MenuIndexAbout)
            {
                currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
            }
            break;
        default:
            break;
        };
    }
    else
    {
        switch (currentSettingsIndex)
        {
        case MenuSettingsMain:
            // back to title, up to profile, down to settings, left to sound
            switch (game->input)
            {
            case InputKeyUp:
                if (currentMenuIndex > MenuIndexProfile)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                }
                break;
            case InputKeyDown:
                if (currentMenuIndex < MenuIndexAbout)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                }
                break;
            case InputKeyLeft:
                currentSettingsIndex = MenuSettingsSound; // Switch to sound settings
                break;
            default:
                break;
            };
            break;
        case MenuSettingsSound:
            // sound on/off (using OK button), down to vibration, right to MainSettingsMain
            switch (game->input)
            {
            case InputKeyOk:
            {
                // Toggle sound on/off
                soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
                // let's just make the game check if state has changed and save it
            }
            break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case InputKeyDown:
                currentSettingsIndex = MenuSettingsVibration; // Switch to vibration settings
                break;
            default:
                break;
            };
            break;
        case MenuSettingsVibration:
            // vibration on/off (using OK button), up to sound, right to MainSettingsMain, down to leave game
            switch (game->input)
            {
            case InputKeyOk:
            {
                // Toggle vibration on/off
                vibrationToggle = vibrationToggle == ToggleOn ? ToggleOff : ToggleOn;
                // let's just make the game check if state has changed and save it
            }
            break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case InputKeyUp:
                currentSettingsIndex = MenuSettingsSound; // Switch to sound settings
                break;
            case InputKeyDown:
                currentSettingsIndex = MenuSettingsShowPlayer; // Switch to show player settings
                break;
            default:
                break;
            };
            break;
        case MenuSettingsShowPlayer:
            // show/hide player (using OK), up to vibration, down to leave game, right to main
            switch (game->input)
            {
            case InputKeyOk:
                showPlayerToggle = showPlayerToggle == ToggleOn ? ToggleOff : ToggleOn;
                break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case InputKeyUp:
                currentSettingsIndex = MenuSettingsVibration; // Switch to vibration settings
                break;
            case InputKeyDown:
                currentSettingsIndex = MenuSettingsLeave; // Switch to leave game settings
                break;
            default:
                break;
            };
            break;
        case MenuSettingsLeave:
            // leave game (using OK button), up to show player, right to MainSettingsMain
            switch (game->input)
            {
            case InputKeyOk:
                // Leave game
                leaveGame = ToggleOn;
                break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case InputKeyUp:
                currentSettingsIndex = MenuSettingsShowPlayer; // Switch to show player settings
                break;
            default:
                break;
            };
            break;
        default:
            break;
        };
    }

    draw->fillScreen(0xFFFF);
    draw->setColor(0x0000);
    game->input = InputKeyMAX;
    drawMenuType2(draw, currentMenuIndex, currentSettingsIndex);
}

bool Player::isPositionSafe(Vector pos)
{
    if (currentDynamicMap == nullptr)
        return true;

    // Check if position is within bounds
    if (pos.x < 0 || pos.y < 0 ||
        pos.x >= currentDynamicMap->getWidth() ||
        pos.y >= currentDynamicMap->getHeight())
    {
        return false;
    }

    // Check if the tile at this position is safe (not a wall)
    TileType tile = currentDynamicMap->getTile((uint8_t)pos.x, (uint8_t)pos.y);
    return (tile != TILE_WALL);
}

void Player::processInput()
{
    if (!freeRoamGame)
    {
        return;
    }

    InputKey currentInput = lastInput;

    if (currentInput == InputKeyMAX)
    {
        return; // No input to process
    }

    switch (currentMainView)
    {
    case GameViewWelcome:
        if (currentInput == InputKeyOk)
        {
            // Check if we should attempt login or skip to title
            if (loginStatus != LoginSuccess)
            {
                // Try to login first
                currentMainView = GameViewLogin;
                loginStatus = LoginWaiting;
                userRequest(RequestTypeLogin);
            }
            else
            {
                // Already logged in, go to title
                currentMainView = GameViewTitle;
            }
        }
        else if (currentInput == InputKeyBack)
        {
            // Allow exit from welcome screen
            if (freeRoamGame)
            {
                freeRoamGame->endGame(); // This will set shouldReturnToMenu
            }
        }
        break;

    case GameViewTitle:
        // Handle title view navigation
        switch (currentInput)
        {
        case InputKeyUp:
            currentTitleIndex = TitleIndexStart;
            break;
        case InputKeyDown:
            currentTitleIndex = TitleIndexMenu;
            break;
        case InputKeyOk:
            switch (currentTitleIndex)
            {
            case TitleIndexStart:
                // Start button pressed - go to lobby menu
                currentMainView = GameViewLobbyMenu;
                break;
            case TitleIndexMenu:
                // Menu button pressed - go to system menu
                currentMainView = GameViewSystemMenu;
                break;
            default:
                break;
            }
            break;
        case InputKeyBack:
            freeRoamGame->endGame();
            break;
        default:
            break;
        }
        break;

    case GameViewLobbyMenu:
        // Handle lobby menu navigation with proper selection
        switch (currentInput)
        {
        case InputKeyUp:
            currentLobbyMenuIndex = LobbyMenuLocal; // Switch to local menu
            break;
        case InputKeyDown:
            currentLobbyMenuIndex = LobbyMenuOnline; // Switch to online menu
            break;
        case InputKeyOk:
            // 1. Switch to GameViewUserInfo
            // 2. Make a userRequest(RequestTypeUserInfo) call
            // 3. Set userInfoStatus = UserInfoWaiting
            // The user info view will then load player stats and transition to the selected game mode
            currentMainView = GameViewUserInfo;
            userInfoStatus = UserInfoWaiting;
            userRequest(RequestTypeUserInfo);
            break;
        case InputKeyBack:
            currentMainView = GameViewTitle;
            break;
        default:
            break;
        }
        break;

    case GameViewLobbyBrowser:
        switch (currentInput)
        {
        case InputKeyUp:
            if (lobbySelectedIndex > 0)
                lobbySelectedIndex--;
            break;
        case InputKeyDown:
            if (lobbySelectedIndex < lobbyCount) // 0 = "New Game", 1..lobbyCount = existing
                lobbySelectedIndex++;
            break;
        case InputKeyOk:
            if (lobbySelectedIndex == 0)
            {
                // Create a new game — use existing create flow
                onlineGameState = OnlineStateIdle;
                onlinePort = 0;
                onlineGameId[0] = '\0';
                currentMainView = GameViewGameOnline;
            }
            else
            {
                // Join an existing lobby
                int idx = lobbySelectedIndex - 1;
                strncpy(onlineGameId, lobbyEntries[idx].game_id, sizeof(onlineGameId) - 1);
                onlineGameId[sizeof(onlineGameId) - 1] = '\0';
                onlinePort = 80;
                onlineGameState = OnlineStateJoiningExisting;
                currentMainView = GameViewGameOnline;
            }
            break;
        case InputKeyBack:
            lobbyFetched = false; // allow re-fetch next time
            currentMainView = GameViewLobbyMenu;
            break;
        default:
            break;
        }
        break;

    case GameViewSystemMenu:
        // Handle system menu with full original navigation logic
        if (currentMenuIndex != MenuIndexSettings)
        {
            switch (currentInput)
            {
            case InputKeyBack:
                currentMainView = GameViewTitle;
                break;
            case InputKeyUp:
                if (currentMenuIndex > MenuIndexProfile)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                }
                break;
            case InputKeyDown:
                if (currentMenuIndex < MenuIndexAbout)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                }
                break;
            case InputKeyOk:
                // Enter the selected menu item
                if (currentMenuIndex == MenuIndexSettings)
                {
                    // Entering settings - this doesn't change the main menu, just shows settings details
                    currentSettingsIndex = MenuSettingsMain;
                }
                break;
            default:
                break;
            }
        }
        else // currentMenuIndex == MenuIndexSettings
        {
            switch (currentSettingsIndex)
            {
            case MenuSettingsMain:
                switch (currentInput)
                {
                case InputKeyBack:
                    currentMainView = GameViewTitle;
                    break;
                case InputKeyUp:
                    if (currentMenuIndex > MenuIndexProfile)
                    {
                        currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                    }
                    break;
                case InputKeyDown:
                    if (currentMenuIndex < MenuIndexAbout)
                    {
                        currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                    }
                    break;
                case InputKeyLeft:
                    currentSettingsIndex = MenuSettingsSound;
                    break;
                default:
                    break;
                }
                break;
            case MenuSettingsSound:
                switch (currentInput)
                {
                case InputKeyOk:
                    soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
                    // Update the game's sound settings
                    if (freeRoamGame)
                    {
                        freeRoamGame->updateSoundToggle();
                    }
                    break;
                case InputKeyRight:
                    currentSettingsIndex = MenuSettingsMain;
                    break;
                case InputKeyDown:
                    currentSettingsIndex = MenuSettingsVibration;
                    break;
                default:
                    break;
                }
                break;
            case MenuSettingsVibration:
                switch (currentInput)
                {
                case InputKeyOk:
                    vibrationToggle = vibrationToggle == ToggleOn ? ToggleOff : ToggleOn;
                    // Update the game's vibration settings
                    if (freeRoamGame)
                    {
                        freeRoamGame->updateVibrationToggle();
                    }
                    break;
                case InputKeyRight:
                    currentSettingsIndex = MenuSettingsMain;
                    break;
                case InputKeyUp:
                    currentSettingsIndex = MenuSettingsSound;
                    break;
                case InputKeyDown:
                    currentSettingsIndex = MenuSettingsLeave;
                    break;
                default:
                    break;
                }
                break;
            case MenuSettingsLeave:
                switch (currentInput)
                {
                case InputKeyOk:
                    leaveGame = ToggleOn;
                    break;
                case InputKeyRight:
                    currentSettingsIndex = MenuSettingsMain;
                    break;
                case InputKeyUp:
                    currentSettingsIndex = MenuSettingsVibration;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
        break;

    case GameViewLogin:
        switch (currentInput)
        {
        case InputKeyBack:
            currentMainView = GameViewWelcome;
            break;
        case InputKeyOk:
            if (loginStatus == LoginSuccess)
            {
                currentMainView = GameViewTitle;
            }
            break;
        default:
            break;
        }
        break;

    case GameViewRegistration:
        switch (currentInput)
        {
        case InputKeyBack:
            currentMainView = GameViewWelcome;
            break;
        case InputKeyOk:
            if (registrationStatus == RegistrationSuccess)
            {
                currentMainView = GameViewTitle;
            }
            break;
        default:
            break;
        }
        break;

    case GameViewUserInfo:
        switch (currentInput)
        {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            break;
        default:
            break;
        }
        break;
    case GameViewGameLocal:
    case GameViewGameOnline:
        // In game views, we need to handle input differently
        // The game engine itself will handle input through its update() method
        // We don't intercept input here to avoid conflicts with the in-game menu system
        // The original handleMenu() method in the Player::render()
        // and Player::update() methods will handle the in-game system menu correctly
        break;
    default:
        break;
    }
}

void Player::switchLevels(Game *game)
{
    if (currentDynamicMap == nullptr || strcmp(currentDynamicMap->getName(), game->current_level->name) != 0)
    {
        currentDynamicMap.reset(); // reset so we can delete the old map if it exists
        Vector posi = start_position;

        if (strcmp(game->current_level->name, "Tutorial") == 0)
        {
            currentDynamicMap = mapsTutorial();
        }
        else if (strcmp(game->current_level->name, "First") == 0)
        {
            currentDynamicMap = mapsFirst();
        }
        else if (strcmp(game->current_level->name, "Second") == 0)
        {
            currentDynamicMap = mapsSecond();
        }
        else if (strcmp(game->current_level->name, "Online") == 0)
        {
            currentDynamicMap = mapsOnline();
        }

        if (currentDynamicMap != nullptr)
        {
            // Find a safe spawn position for the new level
            posi = findSafeSpawnPosition(game->current_level->name);

            // Always set position when switching levels to avoid being stuck
            position_set(posi);
            hasBeenPositioned = true;

            // update 3D sprite position immediately after setting player position
            if (has3DSprite())
            {
                update3DSpritePosition();

                // Also ensure the sprite rotation and scale are set correctly
                set3DSpriteRotation(atan2f(direction.y, direction.x) + M_PI_2); // Face forward with orientation correction
                set3DSpriteScale(1.0f);                                         // Normal scale
            }

            // Register wall Sprite3Ds as Entity objects so Level renders them automatically.
            // Only do this once per level – if any "Wall" entities already exist, skip.
            bool wallsAlreadyRegistered = false;
            for (int i = 0; i < game->current_level->getEntityCount(); i++)
            {
                Entity *e = game->current_level->getEntity(i);
                if (e && e->name && strcmp(e->name, "Wall") == 0 && e->type == ENTITY_3D_SPRITE)
                {
                    wallsAlreadyRegistered = true;
                    break;
                }
            }

            if (!wallsAlreadyRegistered)
            {
                // Transfer Sprite3D ownership from DynamicMap to new Entity objects
                Sprite3D *walls[MAX_RENDER_WALLS];
                uint8_t wallCount = currentDynamicMap->releaseRenderWalls(walls, MAX_RENDER_WALLS);
                for (uint8_t i = 0; i < wallCount; i++)
                {
                    if (walls[i] == nullptr)
                        continue;
                    Entity *wallEntity = new Entity(
                        "Wall", ENTITY_3D_SPRITE,
                        walls[i]->getPosition(), Vector(1, 1),
                        nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr, nullptr, nullptr,
                        false, SPRITE_3D_NONE, 0x0000);
                    // Assign the Sprite3D directly – entity now owns it
                    wallEntity->sprite_3d = walls[i];
                    wallEntity->sprite_3d_type = SPRITE_3D_CUSTOM; // makes has3DSprite() return true
                    wallEntity->is_visible = true;
                    game->current_level->entity_add(wallEntity);
                }
            }

            justSwitchedLevels = true; // Indicate that we just switched levels
            levelSwitchCounter = 0;    // Reset counter for level switch delay
        }
    }
}

void Player::update(Game *game)
{
    if (game->input == InputKeyBack)
    {
        gameState = gameState == GameStateMenu ? GameStatePlaying : GameStateMenu;
        game->input = InputKeyMAX;
        return;
    }

    if (gameState == GameStateMenu)
    {
        return; // Don't update player position in menu
    }

    const float rotSpeed = 0.2f; // Rotation speed in radians

    switch (game->input)
    {
    case InputKeyUp:
    {
        // Calculate new position
        Vector new_pos = Vector(
            position.x + direction.x * rotSpeed,
            position.y + direction.y * rotSpeed);

        // Check collision with dynamic map
        if (currentDynamicMap == nullptr || !collisionMapCheck(new_pos))
        {
            // Move forward in the direction the player is facing
            this->position_set(new_pos);

            // Update 3D sprite position and rotation to match camera direction
            if (has3DSprite())
            {
                update3DSpritePosition();
                // Make sprite face forward (add π/2 to correct orientation)
                float rotation_angle = atan2f(direction.y, direction.x) + M_PI_2;
                set3DSpriteRotation(rotation_angle);
            }
        }
        game->input = InputKeyMAX;
        justStarted = false;
        justSwitchedLevels = false;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    case InputKeyDown:
    {
        // Calculate new position
        Vector new_pos = Vector(
            position.x - direction.x * rotSpeed,
            position.y - direction.y * rotSpeed);

        // Check collision with dynamic map
        if (currentDynamicMap == nullptr || !collisionMapCheck(new_pos))
        {
            // Move backward (opposite to the direction)
            this->position_set(new_pos);

            // Update 3D sprite position and rotation to match camera direction
            if (has3DSprite())
            {
                update3DSpritePosition();
                // Make sprite face forward (add π/2 to correct orientation)
                float rotation_angle = atan2f(direction.y, direction.x) + M_PI_2;
                set3DSpriteRotation(rotation_angle);
            }
        }
        game->input = InputKeyMAX;
        justStarted = false;
        justSwitchedLevels = false;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    case InputKeyLeft:
    {
        float old_dir_x = direction.x;
        float old_plane_x = plane.x;

        direction.x = direction.x * cos(-rotSpeed) - direction.y * sin(-rotSpeed);
        direction.y = old_dir_x * sin(-rotSpeed) + direction.y * cos(-rotSpeed);
        plane.x = plane.x * cos(-rotSpeed) - plane.y * sin(-rotSpeed);
        plane.y = old_plane_x * sin(-rotSpeed) + plane.y * cos(-rotSpeed);

        // Update sprite rotation to match new camera direction
        if (has3DSprite())
        {
            float rotation_angle = atan2f(direction.y, direction.x) + M_PI_2;
            set3DSpriteRotation(rotation_angle);
        }

        game->input = InputKeyMAX;
        justStarted = false;
        justSwitchedLevels = false;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    case InputKeyRight:
    {
        float old_dir_x = direction.x;
        float old_plane_x = plane.x;

        direction.x = direction.x * cos(rotSpeed) - direction.y * sin(rotSpeed);
        direction.y = old_dir_x * sin(rotSpeed) + direction.y * cos(rotSpeed);
        plane.x = plane.x * cos(rotSpeed) - plane.y * sin(rotSpeed);
        plane.y = old_plane_x * sin(rotSpeed) + plane.y * cos(rotSpeed);

        // Update sprite rotation to match new camera direction
        if (has3DSprite())
        {
            float rotation_angle = atan2f(direction.y, direction.x) + M_PI_2;
            set3DSpriteRotation(rotation_angle);
        }

        game->input = InputKeyMAX;
        justStarted = false;
        justSwitchedLevels = false;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    default:
        break;
    }

    // if at teleport, then switch levels
    if (currentDynamicMap != nullptr && currentDynamicMap->getTile(position.x, position.y) == TILE_TELEPORT)
    {
        // Switch to the next level or map
        if (game->current_level != nullptr)
        {
            if (strcmp(game->current_level->name, "Tutorial") == 0)
            {
                game->level_switch("First"); // Switch to First level
            }
            else if (strcmp(game->current_level->name, "First") == 0)
            {
                game->level_switch("Second"); // Switch to Second level
            }
            else if (strcmp(game->current_level->name, "Second") == 0)
            {
                game->level_switch("Tutorial"); // Go back to Tutorial or main menu
            }
        }
    }
}

void Player::render(Draw *canvas, Game *game)
{
    if (!canvas || !game || !game->current_level)
    {
        return;
    }

    static uint8_t _state = GameStatePlaying;
    if (justSwitchedLevels && !justStarted)
    {
        // show message after switching levels
        game->draw->fillScreen(0xFFFF);
        game->draw->setColor(0x0000);
        game->draw->setFont(FONT_SIZE_PRIMARY);
        game->draw->text(5, 15, "New Level");
        game->draw->setFont(FONT_SIZE_SMALL);
        game->draw->text(5, 30, game->current_level->name);
        game->draw->text(5, 58, "Tip: BACK opens the menu.");
        is_visible = false; // hide player entity during level switch
        if (levelSwitchCounter < 50)
        {
            levelSwitchCounter++;
        }
        else
        {
            justSwitchedLevels = false;
            levelSwitchCounter = 0;                      // reset counter
            is_visible = (showPlayerToggle == ToggleOn); // restore per toggle
        }
        return;
    }

    this->switchLevels(game);

    if (gameState == GameStatePlaying)
    {
        if (_state != GameStatePlaying)
        {
            // make entities active again
            for (int i = 0; i < game->current_level->getEntityCount(); i++)
            {
                Entity *entity = game->current_level->getEntity(i);
                if (entity && !entity->is_active && !entity->is_player)
                {
                    entity->is_active = true; // activate all entities
                }
            }
            this->is_visible = (showPlayerToggle == ToggleOn); // restore per toggle
            _state = GameStatePlaying;
        }
        if (currentDynamicMap != nullptr)
        {
            // Update player 3D sprite orientation for 3rd person perspective
            if (game->getCamera()->perspective == CAMERA_THIRD_PERSON)
            {
                float dir_length = sqrtf(direction.x * direction.x + direction.y * direction.y);
                if (has3DSprite())
                {
                    update3DSpritePosition();
                    float camera_direction_angle = atan2f(direction.y / dir_length, direction.x / dir_length) + M_PI_2;
                    set3DSpriteRotation(camera_direction_angle);
                }
            }
        }
    }
    else if (gameState == GameStateMenu)
    {
        if (_state != GameStateMenu)
        {
            // make entities inactive
            for (int i = 0; i < game->current_level->getEntityCount(); i++)
            {
                Entity *entity = game->current_level->getEntity(i);
                if (entity && entity->is_active && !entity->is_player)
                {
                    entity->is_active = false; // deactivate all entities
                }
            }
            this->is_visible = false; // hide player entity in menu
            _state = GameStateMenu;
        }
        handleMenu(canvas, game);
    }
}

void Player::updateEntitiesFromServer(const char *csv)
{
    if (!csv || !freeRoamGame || !freeRoamGame->getEngine())
        return;

    Game *game = freeRoamGame->getEngine()->getGame();
    if (!game || !game->current_level)
        return;

    // CSV format from server:
    //   Entity update: name,x,y,z,dir_x,dir_y,plane_x,plane_y  (8 fields)
    //   Removal:       name,R                                    (2 fields)
    //

    // Field 0: name (up to first comma)
    const char *p = csv;
    const char *comma = strchr(p, ',');
    if (!comma || comma == p)
        return; // no comma or empty name

    char entity_name[64];
    size_t nameLen = (size_t)(comma - p);
    if (nameLen >= sizeof(entity_name))
        nameLen = sizeof(entity_name) - 1;
    memcpy(entity_name, p, nameLen);
    entity_name[nameLen] = '\0';

    p = comma + 1; // advance past first comma

    // Check for removal marker: "R" as the second field
    if (*p == 'R' && (*(p + 1) == '\0' || *(p + 1) == '\n' || *(p + 1) == '\r'))
    {
        if (strcmp(entity_name, player_name) != 0)
        {
            for (int i = 0; i < game->current_level->getEntityCount(); i++)
            {
                Entity *e = game->current_level->getEntity(i);
                if (e && e->name && strcmp(e->name, entity_name) == 0)
                {
                    game->current_level->entity_remove(e);
                    break;
                }
            }
        }
        return;
    }

    // Parse 7 float fields: x,y,z,dir_x,dir_y,plane_x,plane_y
    float vals[7];
    for (int fi = 0; fi < 7; fi++)
    {
        char *end = nullptr;
        vals[fi] = strtof(p, &end);
        if (end == p)
            return; // parse failure
        if (fi < 6)
        {
            if (*end != ',')
                return;
            p = end + 1;
        }
    }

    float ex = vals[0];
    float ey = vals[1];
    float ez = vals[2];
    float e_dir_x = vals[3];
    float e_dir_y = vals[4];
    float e_pl_x = vals[5];
    float e_pl_y = vals[6];

    if (strcmp(entity_name, player_name) == 0)
    {
        position_set(ex, ey, ez);
        direction.x = e_dir_x;
        direction.y = e_dir_y;
        plane.x = e_pl_x;
        plane.y = e_pl_y;
        if (has3DSprite())
        {
            float rotation_angle = atan2f(direction.y, direction.x) + M_PI_2;
            set3DSpriteRotation(rotation_angle);
            update3DSpritePosition();
        }
    }
    else
    {
        bool found = false;
        for (int i = 0; i < game->current_level->getEntityCount(); i++)
        {
            Entity *e = game->current_level->getEntity(i);
            if (e && e->name && strcmp(e->name, entity_name) == 0)
            {
                found = true;
                e->position_set(ex, ey, ez);
                e->direction.x = e_dir_x;
                e->direction.y = e_dir_y;
                e->plane.x = e_pl_x;
                e->plane.y = e_pl_y;
                if (e->has3DSprite())
                {
                    float rotation_angle = atan2f(e->direction.y, e->direction.x) + M_PI_2;
                    e->set3DSpriteRotation(rotation_angle);
                    e->update3DSpritePosition();
                }
                break;
            }
        }

        if (!found)
        {
            const char *name_ptr = remotePlayerNamePool.insert(std::string(entity_name)).first->c_str();
            Entity *remote = new Entity(
                name_ptr,
                ENTITY_PLAYER,
                Vector(ex, ey, ez),
                Vector(1.0f, 2.0f),
                nullptr,
                nullptr,
                nullptr,
                {},
                {},
                {},
                {},
                {},
                false,
                SPRITE_3D_HUMANOID,
                0x0000);
            remote->is_player = false;
            remote->direction.x = e_dir_x;
            remote->direction.y = e_dir_y;
            remote->plane.x = e_pl_x;
            remote->plane.y = e_pl_y;
            game->current_level->entity_add(remote);
        }
    }
}

void Player::userRequest(RequestType requestType)
{
    if (!freeRoamGame)
    {
        FURI_LOG_E("Player", "userRequest: FreeRoamGame instance is null");
        return;
    }

    // Get app context to access HTTP functionality
    FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
    furi_check(app);

    // Allocate memory for credentials
    char *username = (char *)malloc(64);
    char *password = (char *)malloc(64);
    if (!username || !password)
    {
        FURI_LOG_E("Player", "userRequest: Failed to allocate memory for credentials");
        if (username)
            free(username);
        if (password)
            free(password);
        return;
    }

    // Load credentials from storage
    bool credentialsLoaded = true;
    if (!app->load_char("user_name", username, 64, "flipper_http"))
    {
        FURI_LOG_E("Player", "Failed to load user_name");
        credentialsLoaded = false;
    }
    if (!app->load_char("user_pass", password, 64, "flipper_http"))
    {
        FURI_LOG_E("Player", "Failed to load user_pass");
        credentialsLoaded = false;
    }

    if (!credentialsLoaded)
    {
        switch (requestType)
        {
        case RequestTypeLogin:
            loginStatus = LoginCredentialsMissing;
            break;
        case RequestTypeRegistration:
            registrationStatus = RegistrationCredentialsMissing;
            break;
        case RequestTypeUserInfo:
            userInfoStatus = UserInfoCredentialsMissing;
            break;
        case RequestTypeGameCreate:
            onlineGameState = OnlineStateError;
            break;
        case RequestTypeGameList:
            lobbyFetched = true;
            lobbyCount = 0;
            break;
        default:
            FURI_LOG_E("Player", "userRequest: Unknown request type %d", requestType);
            loginStatus = LoginRequestError;
            registrationStatus = RegistrationRequestError;
            userInfoStatus = UserInfoRequestError;
            onlineGameState = OnlineStateError;
            break;
        }
        free(username);
        free(password);
        return;
    }

    // Create JSON payload for login/registration
    char *payload = (char *)malloc(256);
    if (!payload)
    {
        FURI_LOG_E("Player", "userRequest: Failed to allocate memory for payload");
        free(username);
        free(password);
        return;
    }
    snprintf(payload, 256, "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);

    switch (requestType)
    {
    case RequestTypeLogin:
        if (!app->httpRequestAsync("login.txt",
                                   "https://www.jblanked.com/flipper/api/user/login/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            loginStatus = LoginRequestError;
        }
        break;
    case RequestTypeRegistration:
        if (!app->httpRequestAsync("register.txt",
                                   "https://www.jblanked.com/flipper/api/user/register/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            registrationStatus = RegistrationRequestError;
        }
        break;
    case RequestTypeUserInfo:
    {
        char *authHeader = (char *)malloc(256);
        if (!authHeader)
        {
            userInfoStatus = UserInfoRequestError;
            break;
        }
        char *url = (char *)malloc(128);
        if (!url)
        {
            FURI_LOG_E("Player", "userRequest: Failed to allocate memory for url");
            userInfoStatus = UserInfoRequestError;
            free(username);
            free(password);
            free(payload);
            return;
        }
        snprintf(url, 128, "https://www.jblanked.com/flipper/api/user/game-stats/%s/", username);
        snprintf(authHeader, 256, "{\"Content-Type\":\"application/json\",\"Username\":\"%s\",\"Password\":\"%s\"}", username, password);
        if (!app->httpRequestAsync("user_info.txt", url, GET, authHeader))
        {
            userInfoStatus = UserInfoRequestError;
        }
        free(url);
    }
    break;
    case RequestTypeGameCreate:
    {
        char *authHeader = (char *)malloc(256);
        if (!authHeader)
        {
            onlineGameState = OnlineStateError;
            break;
        }
        char *game_payload = (char *)malloc(128);
        if (!game_payload)
        {
            free(authHeader);
            onlineGameState = OnlineStateError;
            break;
        }
        snprintf(game_payload, 128, "{\"game_name\":\"Free Roam\", \"username\":\"%s\"}", player_name);
        snprintf(authHeader, 256, "{\"Content-Type\":\"application/json\",\"Username\":\"%s\",\"Password\":\"%s\"}", username, password);
        if (!app->httpRequestAsync("game_session.txt", "https://www.jblanked.com/game-server/games/create/", POST, authHeader, game_payload))
        {
            onlineGameState = OnlineStateError;
        }
        else
        {
            onlineGameState = OnlineStateFetchingSession;
        }
        free(authHeader);
        free(game_payload);
    }
    break;
    case RequestTypeGameList:
    {
        char *authHeader = (char *)malloc(256);
        if (!authHeader)
        {
            lobbyFetched = true; // mark as fetched (with 0 results) so UI doesn't hang
            lobbyCount = 0;
            break;
        }
        snprintf(authHeader, 256, "{\"Content-Type\":\"application/json\",\"Username\":\"%s\",\"Password\":\"%s\"}", username, password);
        if (!app->httpRequestAsync("game_list.txt", "https://www.jblanked.com/game-server/games/", GET, authHeader))
        {
            lobbyFetched = true;
            lobbyCount = 0;
        }
        free(authHeader);
    }
    break;
    default:
        FURI_LOG_E("Player", "Unknown request type: %d", requestType);
        loginStatus = LoginRequestError;
        registrationStatus = RegistrationRequestError;
        userInfoStatus = UserInfoRequestError;
        free(username);
        free(password);
        free(payload);
        return;
    }

    free(username);
    free(password);
    free(payload);
}

bool Player::httpRequestIsFinished()
{
    if (!freeRoamGame)
    {
        return true; // Default to finished if no game context
    }

    // Get app context to check HTTP state
    FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
    if (!app)
    {
        return true; // Default to finished if no app context
    }

    // Check if HTTP request is finished (state is IDLE)
    return app->getHttpState() == IDLE;
}

HTTPState Player::getHttpState()
{
    if (!freeRoamGame)
    {
        return INACTIVE;
    }

    // Get app context to check HTTP state
    FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
    if (!app)
    {
        return INACTIVE;
    }

    return app->getHttpState();
}

bool Player::setHttpState(HTTPState state)
{
    if (!freeRoamGame)
    {
        return false;
    }

    // Get app context to set HTTP state
    FreeRoamApp *app = static_cast<FreeRoamApp *>(freeRoamGame->appContext);
    if (!app)
    {
        return false;
    }

    return app->setHttpState(state);
}
