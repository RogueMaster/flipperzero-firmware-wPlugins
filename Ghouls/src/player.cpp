#include "player.hpp"
#include "game.hpp"
#include "general.hpp"
#include "level.hpp"
#include <math.h>
#include <cinttypes>
#include HTTP_INCLUDE
#include JSON_INCLUDE

Player::Player(const char *user_name, const char *user_pass) : Entity(username, ENTITY_PLAYER, Vector(4, 20), Vector(1.0f, 2.0f), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false, SPRITE_3D_HUMANOID, 0x0000)
{
    sprite_3d->setWireframe(WIREFRAME_ENABLED);
    direction = Vector(1, 0);                                    // facing east initially
    plane = Vector(0, 0.66);                                     // camera plane perpendicular to direction
    is_player = true;                                            // Mark this entity as a player (so level doesn't delete it)
    end_position = Vector(4, 20);                                // Initialize end position
    start_position = Vector(4, 20);                              // Initialize start position
    snprintf(username, sizeof(this->username), "%s", user_name); // set username with bounds checking
    name = this->username;                                       // set name
    snprintf(password, sizeof(this->password), "%s", user_pass); // set password with bounds checking
    this->is_visible = (showPlayerToggle == ToggleOn);           // set visibility based on toggle
}

Player::~Player()
{
    if (equippedWeapon)
    {
        // dont delete the weapon since we dont own it
        equippedWeapon = nullptr;
    }
    if (loading)
    {
        ENGINE_MEM_DELETE loading;
        loading = nullptr;
    }
}

void Player::collision(Entity *other, Game *game)
{
    switch (other->type)
    {
    case ENTITY_ENEMY: // ghouls (take damage)
        // Check if enemy can attack
        if (other->elapsed_attack_timer >= other->attack_timer)
        {
            other->elapsed_attack_timer = 0; // Reset enemy attack timer
            this->health -= other->strength;
            this->state = ENTITY_ATTACKED;
            if (this->health <= 0)
            {
                this->health = 0;
                this->state = ENTITY_DEAD;
                this->showAlert("You were killed by a ghoul!", 50);
                this->pendingStatsUpdate = true;
                this->leaveGame = ToggleOn;
            }
        }
        break;
    case ENTITY_NPC: // weapons (pick up)
    {
        Weapon *weapon = static_cast<Weapon *>(other);
        if (!weapon || !equipWeapon(game->current_level, weapon))
        {
            ENGINE_LOG_INFO("[Player:collision] Failed to equip weapon: %s", weapon ? weapon->name : "unknown");
        }
        char alertMsg[64];
        snprintf(alertMsg, sizeof(alertMsg), "Picked up %s!", weapon ? weapon->name : "unknown");
        showAlert(alertMsg);
        break;
    }
    case ENTITY_3D_SPRITE: // map walls, houses, trees, etc (collide with)
        // this is already handled in collisionMapCheck
        break;
    case ENTITY_ICON: // projectiles
        // already handled in Projectile::collision, so we don't need to do anything here
        break;
    default:
        break;
    };
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
        canvas->text(0, canvas->getDisplaySize().y * 10 / 64, "Unknown View", 0x0000);
        return;
    }
}

void Player::drawGameLocalView(Draw *canvas)
{
    if (ghoulsGame->isRunning())
    {
        if (ghoulsGame->getEngine())
        {
            if (shouldLeaveGame())
            {
                if (pendingStatsUpdate)
                {
                    pendingStatsUpdate = false;
                    userRequest(RequestTypeUpdateStats);
                }
                ghoulsGame->endGame();
                return;
            }
            ghoulsGame->getEngine()->updateGameInput(ghoulsGame->getCurrentInput());
            // Reset the input after processing to prevent it from being continuously pressed
            ghoulsGame->resetInput();
            ghoulsGame->getEngine()->runAsync(false);
        }
        return;
    }
    else if (!shouldLeaveGame())
    {
        const int sw = canvas->getDisplaySize().x;
        const int sh = canvas->getDisplaySize().y;
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(sw * 25 / 128, sh / 2, "Starting Game...", 0x0000);
        canvas->swap();
        bool gameStarted = ghoulsGame->startGame();
        if (gameStarted && ghoulsGame->getEngine())
        {
            ghoulsGame->getEngine()->runAsync(false); // Run the game engine immediately
        }
    }
}

void Player::drawGameOnlineView(Draw *canvas)
{
    if (shouldLeaveGame())
    {
        if (pendingStatsUpdate)
        {
            pendingStatsUpdate = false;
            userRequest(RequestTypeUpdateStats);
        }
        if (!HTTP_WEBSOCKET_STOP())
        {
            ENGINE_LOG_INFO("[Player:drawGameOnlineView] Failed to stop WebSocket");
        }
        onlineGameState = OnlineStateIdle;
        ghoulsGame->endGame();
        return;
    }

    const int sw = canvas->getDisplaySize().x;
    const int sh = canvas->getDisplaySize().y;
    switch (onlineGameState)
    {
    // ── Phase 1: POST to jblanked.com/game-server/games/create/ ─────────────
    case OnlineStateIdle:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "Connecting to server...", 0x0000);
        canvas->swap();
        this->userRequest(RequestTypeGameCreate);
        break;
    // ── Phase 2: Wait for HTTP response, parse port, open WebSocket ──────────
    case OnlineStateFetchingSession:
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        static bool loadingStarted = false;
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = ENGINE_MEM_NEW Loading(canvas);
            }
            if (loading)
            {
                loading->setText("Creating game...");
            }
            loadingStarted = true;
        }

        if (!HTTP_REQUEST_IS_FINISHED())
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

            char *response = (char *)ENGINE_MEM_MALLOC(512);
            if (!response)
            {
                onlineGameState = OnlineStateError;
                break;
            }
            if (HTTP_GET_RESPONSE(response, 512))
            {
                char *port_str = get_json_value("port", response);
                if (port_str)
                {
                    onlinePort = (uint16_t)atoi(port_str);
                    ::ENGINE_MEM_FREE(port_str);

                    char *game_id_str = get_json_value("game_id", response);
                    if (game_id_str)
                    {
                        strncpy(onlineGameId, game_id_str, sizeof(onlineGameId) - 1);
                        onlineGameId[sizeof(onlineGameId) - 1] = '\0';
                        ::ENGINE_MEM_FREE(game_id_str);
                    }
                    char *websocket_url = (char *)ENGINE_MEM_MALLOC(128);
                    if (!websocket_url)
                    {
                        onlineGameState = OnlineStateError;
                        ::ENGINE_MEM_FREE(response);
                        break;
                    }
                    snprintf(websocket_url, 128, "ws://www.jblanked.com/ws/game-server/%s/", onlineGameId);
                    if (HTTP_WEBSOCKET_START(websocket_url, onlinePort))
                    {
                        onlineGameState = OnlineStateConnecting;
                    }
                    else
                    {
                        onlineGameState = OnlineStateError;
                    }
                    ::ENGINE_MEM_FREE(websocket_url);
                }
                else
                {
                    ENGINE_LOG_INFO("[Player:drawGameOnlineView] Missing 'port' in game session response");
                    onlineGameState = OnlineStateError;
                }
            }
            else
            {
                ENGINE_LOG_INFO("[Player:drawGameOnlineView] Failed to load game_session response");
                onlineGameState = OnlineStateError;
            }
            ::ENGINE_MEM_FREE(response);
        }
    }
    break;

    // ── Phase 3: Wait for [CONNECTED] from FlipperHTTP board ─────────────────
    case OnlineStateConnecting:
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);

        if (HTTP_WEBSOCKET_IS_CONNECTED())
        {
            onlineGameState = OnlineStatePlaying;
            if (!ghoulsGame->isRunning())
            {
                ghoulsGame->startGameOnline();
            }
            // Switch to the multiplayer level immediately so the correct map loads
            if (ghoulsGame->getEngine() && ghoulsGame->getEngine()->getGame())
            {
                ghoulsGame->getEngine()->getGame()->level_switch("Online");
            }
        }
        else
        {
            canvas->text(0, sh * 10 / 64, "Connecting...", 0x0000);
            canvas->swap();
        }
    }
    break;

    // ── Phase 4: Active online game ───────────────────────────────────────────
    case OnlineStatePlaying:
    {
        if (!ghoulsGame->isRunning())
        {
            canvas->fillScreen(0xFFFF);
            canvas->setFont(FONT_SIZE_MEDIUM);
            canvas->text(sw * 25 / 128, sh / 2, "Starting Game...", 0x0000);
            canvas->swap();
            ghoulsGame->startGameOnline();
            Game *game = ghoulsGame->getGame();
            if (game)
            {
                game->level_switch("Online");
            }
            return;
        }

        // Send the current button input to the server as an integer string.
        // The server feeds this directly into game->input, so InputKey values
        // map 1-to-1 (0=Up, 1=Down, 2=Left, 3=Right, 4=Ok, 5=Back).
        int currentInput = ghoulsGame->getCurrentInput();
        if (currentInput != -1)
        {
            if (currentInput != INPUT_KEY_BACK)
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
                    snprintf(inputMsg, sizeof(inputMsg), "{\"username\": \"%s\", \"input\": %d}", this->name, currentInput);
                    HTTP_WEBSOCKET_SEND(inputMsg);
                    // Prevent the local engine from also moving the player — the server
                    // position update below is the authoritative source.
                    if (ghoulsGame->getEngine())
                    {
                        ghoulsGame->getEngine()->updateGameInput(-1);
                    }
                }
                else // menu
                {
                    if (ghoulsGame->getEngine())
                    {
                        ghoulsGame->getEngine()->updateGameInput(currentInput);
                    }
                }
            }
            else
            {
                if (ghoulsGame->getEngine())
                {
                    ghoulsGame->getEngine()->updateGameInput(INPUT_KEY_BACK);
                }
            }
            ghoulsGame->resetInput();
        }

        // Apply server-authoritative entity positions from the latest WebSocket
        // message, then let the local engine render the updated state.
        char buffer[256];
        HTTP_GET_WEBSOCKET_RESPONSE(buffer, sizeof(buffer));
        if (buffer[0] != '\0')
        {
            if (strcmp(buffer, "[SOCKET/STOPPED]") == 0)
            {
                ENGINE_LOG_INFO("[Player:drawGameOnlineView] WebSocket stopped unexpectedly");
                onlineGameState = OnlineStateError;
            }
            else
            {
                updateEntitiesFromServer(buffer);
            }
        }
        GameEngine *engine = ghoulsGame->getEngine();
        if (engine)
        {
            engine->runAsync(false);

            if (gameState != GameStateMenu)
            {
                // ── Name tag overlay ─────────────────────────────────────────────
                // After the 3D scene is drawn, project each humanoid entity's head
                // position to screen space and draw a small username label above it.
                Game *og = engine->getGame();
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
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "Connection failed!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Check network and", 0x0000);
        canvas->text(0, sh * 30 / 64, "try again.", 0x0000);
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(0, sh * 50 / 64, "Press BACK to return.", 0x0000);
        canvas->swap();
    }
    break;

    // ── Join existing lobby (skip create, go straight to WS) ─────────────────
    case OnlineStateJoiningExisting:
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "Joining game...", 0x0000);
        canvas->swap();

        char *websocket_url = (char *)ENGINE_MEM_MALLOC(128);
        if (!websocket_url)
        {
            onlineGameState = OnlineStateError;
            break;
        }
        snprintf(websocket_url, 128, "ws://www.jblanked.com/ws/game-server/%s/", onlineGameId);
        if (HTTP_WEBSOCKET_START(websocket_url, onlinePort))
        {
            onlineGameState = OnlineStateConnecting;
        }
        else
        {
            onlineGameState = OnlineStateError;
        }
        ::ENGINE_MEM_FREE(websocket_url);
    }
    break;

    default:
        ENGINE_LOG_INFO("[Player:drawGameOnlineView] Unknown online game state: %d", onlineGameState);
        break;
    }
}

void Player::drawLobbyBrowserView(Draw *canvas)
{

    if (!lobbyFetched)
    {
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);

        static bool lobbyRequestStarted = false;
        if (!lobbyRequestStarted)
        {
            if (!loading)
            {
                loading = ENGINE_MEM_NEW Loading(canvas);
            }
            if (loading)
            {
                loading->setText("Fetching lobbies...");
            }
            userRequest(RequestTypeGameList);
            lobbyRequestStarted = true;
        }

        if (!HTTP_REQUEST_IS_FINISHED())
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
        char *response = (char *)ENGINE_MEM_MALLOC(1024);
        if (response)
        {
            if (HTTP_GET_RESPONSE(response, 1024))
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
                        ::ENGINE_MEM_FREE(gid);
                    if (gname)
                        ::ENGINE_MEM_FREE(gname);
                    ::ENGINE_MEM_FREE(entry);
                }
            }
            ::ENGINE_MEM_FREE(response);
        }
        lobbyFetched = true;
        lobbySelectedIndex = 0;
        return;
    }

    // ── Draw the lobby list ──────────────────────────────────────────────────
    const int sw = canvas->getDisplaySize().x;
    const int sh = canvas->getDisplaySize().y;
    canvas->fillScreen(0xFFFF);
    drawRainEffect(canvas);

    canvas->setFont(FONT_SIZE_MEDIUM);
    canvas->text(sw * 20 / 128, sh * 10 / 64, "Online Lobbies", 0x0000);

    // Total items: "New Game" at index 0, then existing lobbies
    int totalItems = 1 + lobbyCount;
    int startY = sh * 18 / 64;
    int lineH = sh * 10 / 64;
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
            canvas->fillRectangle(0, y, sw, lineH, 0x0000);
            canvas->setColor(0xFFFF);
        }
        else
        {
            canvas->setColor(0x0000);
        }

        if (i == 0)
        {
            canvas->text(sw * 4 / 128, y + sh * 8 / 64, "> New Game");
        }
        else
        {
            canvas->text(sw * 4 / 128, y + sh * 8 / 64, lobbyEntries[i - 1].game_name);
        }
        canvas->setColor(0x0000);
    }

    // Scroll indicators
    if (scrollOffset > 0)
    {
        canvas->text(sw * 120 / 128, sh * 20 / 64, "^", 0x0000);
    }
    if (scrollOffset + maxVisible < totalItems)
    {
        canvas->text(sw * 120 / 128, sh * 55 / 64, "v", 0x0000);
    }

    canvas->setFont(FONT_SIZE_SMALL);
    canvas->text(sw * 4 / 128, sh * 62 / 64, "OK:Select  BACK:Return", 0x0000);
}

void Player::drawLobbyMenuView(Draw *canvas)
{
    // draw lobby text
    drawMenuType1(canvas, currentLobbyMenuIndex, "Local", "Online");
}

void Player::drawLoginView(Draw *canvas)
{
    const int sh = canvas->getDisplaySize().y;
    canvas->fillScreen(0xFFFF);
    canvas->setFont(FONT_SIZE_MEDIUM);
    static bool loadingStarted = false;
    switch (loginStatus)
    {
    case LoginWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = ENGINE_MEM_NEW Loading(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Logging in...");
            }
        }
        if (!HTTP_REQUEST_IS_FINISHED())
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
            char *response = (char *)ENGINE_MEM_MALLOC(256);
            if (!response)
            {
                loginStatus = LoginRequestError;
                break;
            }
            if (HTTP_GET_RESPONSE(response, 256))
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
            ::ENGINE_MEM_FREE(response);
        }
        break;
    case LoginSuccess:
        canvas->text(0, sh * 10 / 64, "Login successful!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Press OK to continue.", 0x0000);
        break;
    case LoginCredentialsMissing:
        canvas->text(0, sh * 10 / 64, "Missing credentials!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Please set your username", 0x0000);
        canvas->text(0, sh * 30 / 64, "and password in the app.", 0x0000);
        break;
    case LoginRequestError:
        canvas->text(0, sh * 10 / 64, "Login request failed!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Check your network and", 0x0000);
        canvas->text(0, sh * 30 / 64, "try again later.", 0x0000);
        break;
    default:
        canvas->text(0, sh * 10 / 64, "Logging in...", 0x0000);
        break;
    }
}

void Player::drawMenuType1(Draw *canvas, uint8_t selectedIndex, const char *option1, const char *option2)
{
    canvas->fillScreen(0xFFFF);
    canvas->setFont(FONT_SIZE_SMALL);

    // rain effect
    drawRainEffect(canvas);

    const int sw = canvas->getDisplaySize().x;
    const int sh = canvas->getDisplaySize().y;

    // draw lobby text
    if (selectedIndex == 0)
    {
        canvas->fillRectangle(sw * 36 / 128, sh / 4, sw * 56 / 128, sh / 4, 0x0000);
        canvas->setColor(0xFFFF);
        canvas->text(sw * 54 / 128, sh * 27 / 64, option1);
        canvas->fillRectangle(sw * 36 / 128, sh / 2, sw * 56 / 128, sh / 4, 0xFFFF);
        canvas->setColor(0x0000);
        canvas->text(sw * 54 / 128, sh * 42 / 64, option2);
    }
    else if (selectedIndex == 1)
    {
        canvas->setColor(0xFFFF);
        canvas->fillRectangle(sw * 36 / 128, sh / 4, sw * 56 / 128, sh / 4, 0xFFFF);
        canvas->setColor(0x0000);
        canvas->text(sw * 54 / 128, sh * 27 / 64, option1);
        canvas->fillRectangle(sw * 36 / 128, sh / 2, sw * 56 / 128, sh / 4, 0x0000);
        canvas->setColor(0xFFFF);
        canvas->text(sw * 54 / 128, sh * 42 / 64, option2);
        canvas->setColor(0x0000);
    }
}

void Player::drawMenuType2(Draw *canvas, uint8_t selectedIndexMain, uint8_t selectedIndexSettings)
{
    canvas->fillScreen(0xFFFF);
    canvas->setColor(0x0000);

    const int sw = canvas->getDisplaySize().x;
    const int sh = canvas->getDisplaySize().y;

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

        canvas->setFont(FONT_SIZE_MEDIUM);
        if (this->name == nullptr || strlen(this->name) == 0)
        {
            canvas->text(sw * 6 / 128, sh / 4, "Unknown");
        }
        else
        {
            canvas->text(sw * 6 / 128, sh / 4, this->name);
        }

        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 6 / 128, sh * 30 / 64, level);
        canvas->text(sw * 6 / 128, sh * 37 / 64, health);
        canvas->text(sw * 6 / 128, sh * 44 / 64, xp);
        canvas->text(sw * 6 / 128, sh * 51 / 64, strength);

        // draw a box around the selected option
        canvas->rectangle(sw * 76 / 128, sh * 6 / 64, sw * 46 / 128, sh * 46 / 64, 0x0000);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(sw * 80 / 128, sh / 4, "Profile");
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 80 / 128, sh * 26 / 64, "Map");
        canvas->text(sw * 80 / 128, sh * 36 / 64, "Settings");
        canvas->text(sw * 80 / 128, sh * 46 / 64, "About");
    }
    break;
    case 1: // map
    {
        GhoulsLevel *level = ghoulsGame->getCurrentLevel();
        if (level)
        {
            level->renderMiniMap(canvas);
        }
        canvas->rectangle(sw * 76 / 128, sh * 6 / 64, sw * 46 / 128, sh * 46 / 64, 0x0000);
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 80 / 128, sh / 4, "Profile");
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(sw * 80 / 128, sh * 26 / 64, "Map");
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 80 / 128, sh * 36 / 64, "Settings");
        canvas->text(sw * 80 / 128, sh * 46 / 64, "About");
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
            canvas->setFont(FONT_SIZE_MEDIUM);
            canvas->text(sw * 6 / 128, sh / 4, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(sw * 6 / 128, sh * 27 / 64, soundStatus);
            canvas->text(sw * 6 / 128, sh * 36 / 64, vibrationStatus);
            canvas->text(sw * 6 / 128, sh * 45 / 64, showPlayerStatus);
            canvas->text(sw * 6 / 128, sh * 54 / 64, "Leave Game");
            break;
        case 1: // sound
            canvas->setFont(FONT_SIZE_MEDIUM);
            canvas->text(sw * 6 / 128, sh / 4, "Settings");
            canvas->fillRectangle(sw * 5 / 128, sh * 20 / 64, sw * 41 / 128, sh * 10 / 64, 0x0000);
            canvas->text(sw * 6 / 128, sh * 27 / 64, soundStatus, 0xFFFF);
            canvas->text(sw * 6 / 128, sh * 36 / 64, vibrationStatus, 0x0000);
            canvas->text(sw * 6 / 128, sh * 45 / 64, showPlayerStatus, 0x0000);
            canvas->text(sw * 6 / 128, sh * 54 / 64, "Leave Game", 0x0000);
            break;
        case 2: // vibration
            canvas->setFont(FONT_SIZE_MEDIUM);
            canvas->text(sw * 6 / 128, sh / 4, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(sw * 6 / 128, sh * 27 / 64, soundStatus);
            canvas->fillRectangle(sw * 5 / 128, sh * 29 / 64, sw * 49 / 128, sh * 10 / 64, 0x0000);
            canvas->text(sw * 6 / 128, sh * 36 / 64, vibrationStatus, 0xFFFF);
            canvas->text(sw * 6 / 128, sh * 45 / 64, showPlayerStatus, 0x0000);
            canvas->text(sw * 6 / 128, sh * 54 / 64, "Leave Game", 0x0000);
            break;
        case 3: // show player
            canvas->setFont(FONT_SIZE_MEDIUM);
            canvas->text(sw * 6 / 128, sh / 4, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(sw * 6 / 128, sh * 27 / 64, soundStatus);
            canvas->text(sw * 6 / 128, sh * 36 / 64, vibrationStatus);
            canvas->fillRectangle(sw * 5 / 128, sh * 38 / 64, sw * 49 / 128, sh * 10 / 64, 0x0000);
            canvas->text(sw * 6 / 128, sh * 45 / 64, showPlayerStatus, 0xFFFF);
            canvas->text(sw * 6 / 128, sh * 54 / 64, "Leave Game", 0x0000);
            break;
        case 4: // leave game
            canvas->setFont(FONT_SIZE_MEDIUM);
            canvas->text(sw * 6 / 128, sh / 4, "Settings");
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(sw * 6 / 128, sh * 27 / 64, soundStatus);
            canvas->text(sw * 6 / 128, sh * 36 / 64, vibrationStatus);
            canvas->text(sw * 6 / 128, sh * 45 / 64, showPlayerStatus);
            canvas->fillRectangle(sw * 5 / 128, sh * 47 / 64, sw * 41 / 128, sh * 10 / 64, 0x0000);
            canvas->text(sw * 6 / 128, sh * 54 / 64, "Leave Game", 0xFFFF);
            break;
        default:
            break;
        };
        canvas->rectangle(sw * 76 / 128, sh * 6 / 64, sw * 46 / 128, sh * 46 / 64, 0x0000);
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 80 / 128, sh / 4, "Profile");
        canvas->text(sw * 80 / 128, sh * 26 / 64, "Map");
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(sw * 79 / 128, sh * 36 / 64, "Settings");
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 80 / 128, sh * 46 / 64, "About");
    }
    break;
    case 3: // about
    {
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(sw * 6 / 128, sh / 4, "Ghouls");
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 4 / 128, sh * 25 / 64, "Creator: @JBlanked");
        canvas->text(sw * 6 / 128, sh * 34 / 64, "- GitHub");
        canvas->text(sw * 6 / 128, sh * 43 / 64, "- YouTube");
        canvas->text(sw * 6 / 128, sh * 52 / 64, "- Instagram");
        canvas->text(sw * 6 / 128, sh * 61 / 64, "- jblanked.com");

        // draw a box around the selected option
        canvas->rectangle(sw * 76 / 128, sh * 6 / 64, sw * 46 / 128, sh * 46 / 64, 0x0000);
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(sw * 80 / 128, sh / 4, "Profile");
        canvas->text(sw * 80 / 128, sh * 26 / 64, "Map");
        canvas->text(sw * 80 / 128, sh * 36 / 64, "Settings");
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(sw * 80 / 128, sh * 46 / 64, "About");
    }
    break;
    default:
        canvas->fillScreen(0xFFFF);
        canvas->text(0, sh * 10 / 64, "Unknown Menu", 0x0000);
        break;
    };

    // Show current game time below the navigation rectangle
    if (selectedIndexMain < 4 && ghoulsGame)
    {
        Time *t = ghoulsGame->getGameTime();
        if (t)
        {
            uint8_t hours = 0, minutes = 0;
            t->getTimeIn24HourFormat(hours, minutes);
            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d", hours, minutes);
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->text(sw * 78 / 128, sh * 59 / 64, timeStr, 0x0000);
        }
    }
}

void Player::showAlert(const char *message, uint16_t ticks)
{
    if (!message)
        return;
    snprintf(alertMessage, sizeof(alertMessage), "%s", message);
    alertTimer = ticks; // show for specified duration
}

void Player::drawRainEffect(Draw *canvas)
{
    const int sw = canvas->getDisplaySize().x;
    const int sh = canvas->getDisplaySize().y;
    // rain droplets/star droplets effect
    for (int i = 0; i < 8; i++)
    {
        // Use pseudo-random offsets based on frame and droplet index
        uint8_t seed = (rainFrame + i * 37) & 0xFF;
        int16_t x = (rainFrame + seed * 13) % sw;
        int16_t y = (rainFrame * 2 + seed * 7 + i * 23) % sh;

        // Draw star-like droplet
        canvas->pixel(x, y, 0x0000);
        canvas->pixel(x - 1, y, 0x0000);
        canvas->pixel(x + 1, y, 0x0000);
        canvas->pixel(x, y - 1, 0x0000);
        canvas->pixel(x, y + 1, 0x0000);
    }

    rainFrame += 1;
    if (rainFrame > sw) // reset after a full cycle
    {
        rainFrame = 0;
    }
}

void Player::drawRegistrationView(Draw *canvas)
{
    const int sh = canvas->getDisplaySize().y;
    canvas->fillScreen(0xFFFF);
    canvas->setFont(FONT_SIZE_MEDIUM);
    static bool loadingStarted = false;
    switch (registrationStatus)
    {
    case RegistrationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = ENGINE_MEM_NEW Loading(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Registering...");
            }
        }
        if (!HTTP_REQUEST_IS_FINISHED())
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
            char *response = (char *)ENGINE_MEM_MALLOC(256);
            if (!response)
            {
                ENGINE_LOG_INFO("[Player:drawRegistrationView] Failed to allocate memory for registration response");
                registrationStatus = RegistrationRequestError;
                return;
            }
            if (HTTP_GET_RESPONSE(response, 256))
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
            ::ENGINE_MEM_FREE(response);
        }
        break;
    case RegistrationSuccess:
        canvas->text(0, sh * 10 / 64, "Registration successful!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Press OK to continue.", 0x0000);
        break;
    case RegistrationCredentialsMissing:
        canvas->text(0, sh * 10 / 64, "Missing credentials!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Please update your username", 0x0000);
        canvas->text(0, sh * 30 / 64, "and password in the settings.", 0x0000);
        break;
    case RegistrationRequestError:
        canvas->text(0, sh * 10 / 64, "Registration request failed!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Check your network and", 0x0000);
        canvas->text(0, sh * 30 / 64, "try again later.", 0x0000);
        break;
    default:
        canvas->text(0, sh * 10 / 64, "Registering...", 0x0000);
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
    const int sh = canvas->getDisplaySize().y;
    static bool loadingStarted = false;
    switch (userInfoStatus)
    {
    case UserInfoWaiting:
        canvas->fillScreen(0xFFFF);
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = ENGINE_MEM_NEW Loading(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Fetching...");
            }
        }
        if (!HTTP_REQUEST_IS_FINISHED())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            canvas->text(0, sh * 10 / 64, "Loading user info...", 0x0000);
            canvas->text(0, sh * 20 / 64, "Please wait...", 0x0000);
            canvas->text(0, sh * 30 / 64, "It may take up to 15 seconds.", 0x0000);
            char *response = (char *)ENGINE_MEM_MALLOC(512);
            if (!response)
            {
                ENGINE_LOG_INFO("[Player:drawUserInfoView] Failed to allocate memory for user info response");
                userInfoStatus = UserInfoRequestError;
                if (loading)
                {
                    loading->stop();
                }
                loadingStarted = false;
                return;
            }
            if (HTTP_GET_RESPONSE(response, 512))
            {
                userInfoStatus = UserInfoSuccess;
                // they're in! let's go
                char *game_stats = get_json_value("game_stats", response);
                if (!game_stats)
                {
                    ENGINE_LOG_INFO("[Player:drawUserInfoView] Failed to parse game_stats");
                    userInfoStatus = UserInfoParseError;
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    ::ENGINE_MEM_FREE(response);
                    return;
                }
                canvas->fillScreen(0xFFFF);
                canvas->text(0, sh * 10 / 64, "User info loaded!", 0x0000);
                char *username = get_json_value("username", game_stats);
                char *level = get_json_value("level", game_stats);
                char *xp = get_json_value("xp", game_stats);
                char *health = get_json_value("health", game_stats);
                char *strength = get_json_value("strength", game_stats);
                char *max_health = get_json_value("max_health", game_stats);
                if (!username || !level || !xp || !health || !strength || !max_health)
                {
                    ENGINE_LOG_INFO("[Player:drawUserInfoView] Failed to parse user info");
                    userInfoStatus = UserInfoParseError;
                    if (username)
                        ::ENGINE_MEM_FREE(username);
                    if (level)
                        ::ENGINE_MEM_FREE(level);
                    if (xp)
                        ::ENGINE_MEM_FREE(xp);
                    if (health)
                        ::ENGINE_MEM_FREE(health);
                    if (strength)
                        ::ENGINE_MEM_FREE(strength);
                    if (max_health)
                        ::ENGINE_MEM_FREE(max_health);
                    ::ENGINE_MEM_FREE(game_stats);
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    ::ENGINE_MEM_FREE(response);
                    return;
                }

                // Update player info
                this->level = atoi(level);
                this->xp = atoi(xp);
                this->health = atoi(health);
                this->strength = atoi(strength);
                this->max_health = atoi(max_health);

                // clean em up gang
                ::ENGINE_MEM_FREE(username);
                ::ENGINE_MEM_FREE(level);
                ::ENGINE_MEM_FREE(xp);
                ::ENGINE_MEM_FREE(health);
                ::ENGINE_MEM_FREE(strength);
                ::ENGINE_MEM_FREE(max_health);
                ::ENGINE_MEM_FREE(game_stats);
                ::ENGINE_MEM_FREE(response);

                if (loading)
                {
                    loading->stop();
                }
                loadingStarted = false;

                // no online right now
                // just jump into a local game
                // if (currentLobbyMenuIndex == LobbyMenuLocal)
                // {
                currentMainView = GameViewGameLocal; // Switch to local game view
                ghoulsGame->startGame();
                // }
                // else if (currentLobbyMenuIndex == LobbyMenuOnline)
                // {
                //     lobbyFetched = false; // Reset so browser fetches fresh data
                //     lobbySelectedIndex = 0;
                //     currentMainView = GameViewLobbyBrowser; // Show lobby browser instead of creating directly
                // }
                return;
            }
            else
            {
                userInfoStatus = UserInfoRequestError;
            }
            ::ENGINE_MEM_FREE(response);
        }
        break;
    case UserInfoSuccess:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "User info loaded successfully!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Press OK to continue.", 0x0000);
        break;
    case UserInfoCredentialsMissing:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "Missing credentials!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Please update your username", 0x0000);
        canvas->text(0, sh * 30 / 64, "and password in the settings.", 0x0000);
        break;
    case UserInfoRequestError:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "User info request failed!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Check your network and", 0x0000);
        canvas->text(0, sh * 30 / 64, "try again later.", 0x0000);
        break;
    case UserInfoParseError:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "Failed to parse user info!", 0x0000);
        canvas->text(0, sh * 20 / 64, "Try again...", 0x0000);
        break;
    default:
        canvas->fillScreen(0xFFFF);
        canvas->setFont(FONT_SIZE_MEDIUM);
        canvas->text(0, sh * 10 / 64, "Loading user info...", 0x0000);
        break;
    }
}

void Player::drawWelcomeView(Draw *canvas)
{
    const int sw = canvas->getDisplaySize().x;
    const int sh = canvas->getDisplaySize().y;
    canvas->fillScreen(0xFFFF);

    // rain effect
    drawRainEffect(canvas);

    // Draw welcome text with blinking effect
    // Blink every 15 frames (show for 15, hide for 15)
    canvas->setFont(FONT_SIZE_SMALL);
    if ((welcomeFrame / 15) % 2 == 0)
    {
        canvas->text(sw * 40 / 128, sh * 60 / 64, "Press OK to start", 0x0000);
    }
    welcomeFrame++;

    // Reset frame counter to prevent overflow
    if (welcomeFrame >= 30)
    {
        welcomeFrame = 0;
    }

    // Draw a box around the OK button
    canvas->fillRectangle(sw * 36 / 128, sh * 25 / 64, sw * 56 / 128, sh / 4, 0x0000);
    canvas->setColor(0xFFFF);
    canvas->text(sw * 52 / 128, sh * 31 / 64, "Welcome", 0xFFFF);
    canvas->setColor(0x0000);
}

bool Player::equipWeapon(Level *level, Weapon *weapon)
{
    if (!weapon)
    {
        ENGINE_LOG_INFO("[Player:equipWeapon] Cannot equip null weapon");
        return false;
    }
    if (!level)
    {
        ENGINE_LOG_INFO("[Player:equipWeapon] Cannot equip weapon without level");
        return false;
    }
    if (equippedWeapon)
    {
        // drop weapon right behind us
        equippedWeapon->setHeld(false);
        equippedWeapon->position_set(this->position.x - 4, this->position.y, this->position.z); // drop slightly behind player
        equippedWeapon->direction = this->direction;
        equippedWeapon->update3DSpritePosition();
        equippedWeapon = nullptr; // drop our reference
    }
    weapon->setHeld(true);
    weapon->position_set(this->position);
    weapon->direction = this->direction;
    if (weapon->has3DSprite())
    {
        weapon->update3DSpritePosition();
        weapon->set3DSpriteRotation(this->sprite_rotation);
    }
    equippedWeapon = weapon;
    // weapon is already added to level, we're just taking ownership here
    return true;
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
        case INPUT_KEY_UP:
            if (currentMenuIndex > MenuIndexProfile)
            {
                currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
            }
            break;
        case INPUT_KEY_DOWN:
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
            case INPUT_KEY_UP:
                if (currentMenuIndex > MenuIndexProfile)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                }
                break;
            case INPUT_KEY_DOWN:
                if (currentMenuIndex < MenuIndexAbout)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                }
                break;
            case INPUT_KEY_LEFT:
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
            case INPUT_KEY_CENTER:
            {
                // Toggle sound on/off
                soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
                // let's just make the game check if state has changed and save it
            }
            break;
            case INPUT_KEY_RIGHT:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case INPUT_KEY_DOWN:
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
            case INPUT_KEY_CENTER:
            {
                // Toggle vibration on/off
                vibrationToggle = vibrationToggle == ToggleOn ? ToggleOff : ToggleOn;
                // let's just make the game check if state has changed and save it
            }
            break;
            case INPUT_KEY_RIGHT:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case INPUT_KEY_UP:
                currentSettingsIndex = MenuSettingsSound; // Switch to sound settings
                break;
            case INPUT_KEY_DOWN:
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
            case INPUT_KEY_CENTER:
                showPlayerToggle = showPlayerToggle == ToggleOn ? ToggleOff : ToggleOn;
                break;
            case INPUT_KEY_RIGHT:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case INPUT_KEY_UP:
                currentSettingsIndex = MenuSettingsVibration; // Switch to vibration settings
                break;
            case INPUT_KEY_DOWN:
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
            case INPUT_KEY_CENTER:
                // Leave game
                pendingStatsUpdate = true; // deferred: sent from shallow stack before endGame()
                leaveGame = ToggleOn;
                break;
            case INPUT_KEY_RIGHT:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                break;
            case INPUT_KEY_UP:
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
    game->input = -1;
    drawMenuType2(draw, currentMenuIndex, currentSettingsIndex);
}

void Player::increaseXP(uint16_t amount)
{
    xp += amount;
    uint16_t old_level = (uint16_t)level;
    // Determine the player's level based on XP
    level = 1;
    uint32_t xp_required = 100; // Base XP for level 2

    while (level < 100 && xp >= xp_required) // Maximum level supported
    {
        level++;
        xp_required = (uint32_t)(xp_required * 1.5); // 1.5 growth factor per level
    }

    // Update strength and max health based on the new level
    strength = 10 + (level * 1);           // 1 strength per level
    max_health = 100 + ((level - 1) * 10); // 10 health per level

    if (level > old_level)
    {
        char levelUpMessage[32];
        snprintf(levelUpMessage, sizeof(levelUpMessage), "Leveled up to %d!", (uint16_t)level);
        showAlert(levelUpMessage, 120);
    }
}

void Player::processInput()
{
    if (!ghoulsGame)
    {
        return;
    }

    int currentInput = lastInput;

    if (currentInput == -1)
    {
        return; // No input to process
    }

    switch (currentMainView)
    {
    case GameViewWelcome:
        if (currentInput == INPUT_KEY_CENTER)
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
        else if (currentInput == INPUT_KEY_BACK)
        {
            // Allow exit from welcome screen
            if (ghoulsGame)
            {
                ghoulsGame->endGame(); // This will set shouldReturnToMenu
            }
        }
        break;

    case GameViewTitle:
        // Handle title view navigation
        switch (currentInput)
        {
        case INPUT_KEY_UP:
            currentTitleIndex = TitleIndexStart;
            break;
        case INPUT_KEY_DOWN:
            currentTitleIndex = TitleIndexMenu;
            break;
        case INPUT_KEY_CENTER:
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
        case INPUT_KEY_BACK:
            ghoulsGame->endGame();
            break;
        default:
            break;
        }
        break;

    case GameViewLobbyMenu:
        // Handle lobby menu navigation with proper selection
        switch (currentInput)
        {
        case INPUT_KEY_UP:
            currentLobbyMenuIndex = LobbyMenuLocal; // Switch to local menu
            break;
        case INPUT_KEY_DOWN:
            currentLobbyMenuIndex = LobbyMenuOnline; // Switch to online menu
            break;
        case INPUT_KEY_CENTER:
            // 1. Switch to GameViewUserInfo
            // 2. Make a userRequest(RequestTypeUserInfo) call
            // 3. Set userInfoStatus = UserInfoWaiting
            // The user info view will then load player stats and transition to the selected game mode
            currentMainView = GameViewUserInfo;
            userInfoStatus = UserInfoWaiting;
            userRequest(RequestTypeUserInfo);
            break;
        case INPUT_KEY_BACK:
            currentMainView = GameViewTitle;
            break;
        default:
            break;
        }
        break;

    case GameViewLobbyBrowser:
        switch (currentInput)
        {
        case INPUT_KEY_UP:
            if (lobbySelectedIndex > 0)
                lobbySelectedIndex--;
            break;
        case INPUT_KEY_DOWN:
            if (lobbySelectedIndex < lobbyCount) // 0 = "New Game", 1..lobbyCount = existing
                lobbySelectedIndex++;
            break;
        case INPUT_KEY_CENTER:
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
        case INPUT_KEY_BACK:
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
            case INPUT_KEY_BACK:
                currentMainView = GameViewTitle;
                break;
            case INPUT_KEY_UP:
                if (currentMenuIndex > MenuIndexProfile)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                }
                break;
            case INPUT_KEY_DOWN:
                if (currentMenuIndex < MenuIndexAbout)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                }
                break;
            case INPUT_KEY_CENTER:
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
                case INPUT_KEY_BACK:
                    currentMainView = GameViewTitle;
                    break;
                case INPUT_KEY_UP:
                    if (currentMenuIndex > MenuIndexProfile)
                    {
                        currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                    }
                    break;
                case INPUT_KEY_DOWN:
                    if (currentMenuIndex < MenuIndexAbout)
                    {
                        currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                    }
                    break;
                case INPUT_KEY_LEFT:
                    currentSettingsIndex = MenuSettingsSound;
                    break;
                default:
                    break;
                }
                break;
            case MenuSettingsSound:
                switch (currentInput)
                {
                case INPUT_KEY_CENTER:
                    soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
                    break;
                case INPUT_KEY_RIGHT:
                    currentSettingsIndex = MenuSettingsMain;
                    break;
                case INPUT_KEY_DOWN:
                    currentSettingsIndex = MenuSettingsVibration;
                    break;
                default:
                    break;
                }
                break;
            case MenuSettingsVibration:
                switch (currentInput)
                {
                case INPUT_KEY_CENTER:
                    break;
                case INPUT_KEY_RIGHT:
                    currentSettingsIndex = MenuSettingsMain;
                    break;
                case INPUT_KEY_UP:
                    currentSettingsIndex = MenuSettingsSound;
                    break;
                case INPUT_KEY_DOWN:
                    currentSettingsIndex = MenuSettingsLeave;
                    break;
                default:
                    break;
                }
                break;
            case MenuSettingsLeave:
                switch (currentInput)
                {
                case INPUT_KEY_CENTER:
                    leaveGame = ToggleOn;
                    break;
                case INPUT_KEY_RIGHT:
                    currentSettingsIndex = MenuSettingsMain;
                    break;
                case INPUT_KEY_UP:
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
        case INPUT_KEY_BACK:
            currentMainView = GameViewWelcome;
            break;
        case INPUT_KEY_CENTER:
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
        case INPUT_KEY_BACK:
            currentMainView = GameViewWelcome;
            break;
        case INPUT_KEY_CENTER:
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
        case INPUT_KEY_BACK:
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

void Player::render(Draw *canvas, Game *game)
{
    if (!canvas || !game || !game->current_level)
    {
        return;
    }

    static uint8_t _state = GameStatePlaying;

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

        // draw ammo count if we have a weapon equipped
        const int sw = canvas->getDisplaySize().x;
        const int sh = canvas->getDisplaySize().y;
#if GROUND_RENDER_ALLOWED
        const uint16_t color = ghoulsGame->isDay() ? 0x0000 : 0xFFFF; // black in day, white in night
#else
        const uint16_t color = 0x0000; // always black if no ground rendering
#endif
        if (equippedWeapon)
        {
            canvas->setFont(FONT_SIZE_SMALL);
            char ammoStr[16];
            uint16_t ammo = equippedWeapon->getAmmo();
            if (ammo > 0)
            {
                snprintf(ammoStr, sizeof(ammoStr), "Ammo: %d", ammo);
            }
            else
            {
                snprintf(ammoStr, sizeof(ammoStr), "Ammo: ∞");
            }
            canvas->text(sw * 4 / 128, sh * 61 / 64, ammoStr, color);
        }

        // draw health
        canvas->setFont(FONT_SIZE_SMALL);
        char healthStr[32];
        snprintf(healthStr, sizeof(healthStr), "HP: %d", (uint16_t)health);
        canvas->text(sw * 96 / 128, sh * 61 / 64, healthStr, color);

        // Draw in-game alert overlay if active
        if (alertTimer > 0 && alertMessage[0] != '\0')
        {
            canvas->setFont(FONT_SIZE_SMALL);
            canvas->fillRectangle(0, 0, sw, sh * 9 / 64, 0x0000);
            canvas->text(sw * 2 / 128, sh * 6 / 64, alertMessage, 0xFFFF);
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

void Player::update(Game *game)
{
    if (!game || !game->is_active || !game->current_level)
    {
        return;
    }

    if (game->input == INPUT_KEY_BACK)
    {
        gameState = gameState == GameStateMenu ? GameStatePlaying : GameStateMenu;
        game->input = -1;
        return;
    }

    if (alertTimer > 0)
    {
        alertTimer--;
    }

    if (gameState == GameStateMenu || state == ENTITY_DEAD)
    {
        return; // Don't update player position in menu or if dead
    }

    float rotSpeed = 0.2f; // Rotation speed in radians

    switch (game->input)
    {
    case INPUT_KEY_UP:
    {
        GhoulsLevel *currentLevel = static_cast<GhoulsLevel *>(game->current_level);
        if (!currentLevel)
        {
            return; // Invalid level type
        }

        rotSpeed = 0.4f;

        // Calculate new position
        Vector new_pos = Vector(
            position.x + direction.x * rotSpeed,
            position.y + direction.y * rotSpeed);

        // Check collision with dynamic map
        if (!currentLevel->collisionMapCheck(new_pos))
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

            // update equipped weapon
            if (equippedWeapon)
            {
                equippedWeapon->position_set(this->position);
                if (equippedWeapon->has3DSprite())
                {
                    equippedWeapon->update3DSpritePosition();
                    equippedWeapon->set3DSpriteRotation(sprite_rotation);
                }
            }
        }
        game->input = -1;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    case INPUT_KEY_DOWN:
    {
        GhoulsLevel *currentLevel = static_cast<GhoulsLevel *>(game->current_level);
        if (!currentLevel)
        {
            return; // Invalid level type
        }

        rotSpeed = 0.4f;

        // Calculate new position
        Vector new_pos = Vector(
            position.x - direction.x * rotSpeed,
            position.y - direction.y * rotSpeed);

        // Check collision with dynamic map
        if (!currentLevel->collisionMapCheck(new_pos))
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

            // update equipped weapon
            if (equippedWeapon)
            {
                equippedWeapon->position_set(this->position);
                if (equippedWeapon->has3DSprite())
                {
                    equippedWeapon->update3DSpritePosition();
                    equippedWeapon->set3DSpriteRotation(sprite_rotation);
                }
            }
        }
        game->input = -1;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    case INPUT_KEY_LEFT:
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

        // update equipped weapon
        if (equippedWeapon)
        {
            equippedWeapon->direction = this->direction;
            equippedWeapon->plane = this->plane;
            if (equippedWeapon->has3DSprite())
            {
                equippedWeapon->set3DSpriteRotation(sprite_rotation);
            }
        }

        game->input = -1;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    case INPUT_KEY_RIGHT:
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

        // update equipped weapon
        if (equippedWeapon)
        {
            equippedWeapon->direction = this->direction;
            equippedWeapon->plane = this->plane;
            if (equippedWeapon->has3DSprite())
            {
                equippedWeapon->set3DSpriteRotation(sprite_rotation);
            }
        }

        game->input = -1;
        is_visible = (showPlayerToggle == ToggleOn);
    }
    break;
    case INPUT_KEY_CENTER:
        if (equippedWeapon)
        {
            if (equippedWeapon->fire(game->current_level))
            {
                char alert_buf[32];
                snprintf(alert_buf, sizeof(alert_buf), "Fired %s!", equippedWeapon->name);
                this->showAlert(alert_buf, 10);
            }
        }
        break;
    default:
        break;
    }
}

void Player::updateEntitiesFromServer(const char *csv)
{
    if (!csv || !ghoulsGame || !ghoulsGame->getEngine())
        return;

    GhoulsLevel *currentLevel = ghoulsGame->getCurrentLevel();
    if (!currentLevel)
    {
        return;
    }

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
        if (strcmp(entity_name, this->name) != 0)
        {
            for (int i = 0; i < currentLevel->getEntityCount(); i++)
            {
                Entity *e = currentLevel->getEntity(i);
                if (e && e->name && strcmp(e->name, entity_name) == 0)
                {
                    currentLevel->entity_remove(e);
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

    if (strcmp(entity_name, this->name) == 0)
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
        for (int i = 0; i < currentLevel->getEntityCount(); i++)
        {
            Entity *e = currentLevel->getEntity(i);
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
            // switch back to holding name (on-release)
            // const char *name_ptr = remotePlayerNamePool.insert(std::string(entity_name)).first->c_str();
            Entity *remote = ENGINE_MEM_NEW Entity(
                entity_name, // name_ptr,
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
            currentLevel->entity_add(remote);
        }
    }
}

void Player::userRequest(RequestType requestType)
{
    // Create JSON payload for login/registration
    char *payload = (char *)ENGINE_MEM_MALLOC(256);
    if (!payload)
    {
        ENGINE_LOG_INFO("[Player:userRequest] Failed to allocate memory for payload");
        return;
    }
    snprintf(payload, 256, "{\"username\":\"%s\",\"password\":\"%s\"}", this->name, this->password);

    switch (requestType)
    {
    case RequestTypeLogin:
        if (!HTTP_SEND_REQUEST("https://www.jblanked.com/flipper/api/user/login/", "POST", "{\"Content-Type\":\"application/json\"}", payload))
        {
            ENGINE_LOG_INFO("[Player:userRequest] Login request failed for user: %s", this->name);
            loginStatus = LoginRequestError;
        }
        break;
    case RequestTypeRegistration:
        if (!HTTP_SEND_REQUEST("https://www.jblanked.com/flipper/api/user/register/", "POST", "{\"Content-Type\":\"application/json\"}", payload))
        {
            registrationStatus = RegistrationRequestError;
        }
        break;
    case RequestTypeUserInfo:
    {
        char *authHeader = (char *)ENGINE_MEM_MALLOC(256);
        if (!authHeader)
        {
            userInfoStatus = UserInfoRequestError;
            break;
        }
        char *url = (char *)ENGINE_MEM_MALLOC(128);
        if (!url)
        {
            ENGINE_LOG_INFO("[Player:userRequest] Failed to allocate memory for url");
            userInfoStatus = UserInfoRequestError;
            ENGINE_MEM_FREE(payload);
            return;
        }
        snprintf(url, 128, "https://www.jblanked.com/flipper/api/user/game-stats/%s/", this->name);
        snprintf(authHeader, 256, "{\"Content-Type\":\"application/json\",\"Username\":\"%s\",\"Password\":\"%s\"}", this->name, this->password);
        if (!HTTP_SEND_REQUEST(url, "GET", authHeader, nullptr))
        {
            userInfoStatus = UserInfoRequestError;
        }
        ENGINE_MEM_FREE(url);
    }
    break;
    case RequestTypeGameCreate:
    {
        char *authHeader = (char *)ENGINE_MEM_MALLOC(256);
        if (!authHeader)
        {
            onlineGameState = OnlineStateError;
            break;
        }
        char *game_payload = (char *)ENGINE_MEM_MALLOC(128);
        if (!game_payload)
        {
            ENGINE_MEM_FREE(authHeader);
            onlineGameState = OnlineStateError;
            break;
        }
        snprintf(game_payload, 128, "{\"game_name\":\"Ghouls\", \"username\":\"%s\"}", this->name);
        snprintf(authHeader, 256, "{\"Content-Type\":\"application/json\",\"Username\":\"%s\",\"Password\":\"%s\"}", this->name, this->password);
        if (!HTTP_SEND_REQUEST("https://www.jblanked.com/game-server/games/create/", "POST", authHeader, game_payload))
        {
            onlineGameState = OnlineStateError;
        }
        else
        {
            onlineGameState = OnlineStateFetchingSession;
        }
        ENGINE_MEM_FREE(authHeader);
        ENGINE_MEM_FREE(game_payload);
    }
    break;
    case RequestTypeGameList:
    {
        char *authHeader = (char *)ENGINE_MEM_MALLOC(256);
        if (!authHeader)
        {
            lobbyFetched = true; // mark as fetched (with 0 results) so UI doesn't hang
            lobbyCount = 0;
            break;
        }
        snprintf(authHeader, 256, "{\"Content-Type\":\"application/json\",\"Username\":\"%s\",\"Password\":\"%s\"}", this->name, this->password);
        if (!HTTP_SEND_REQUEST("https://www.jblanked.com/game-server/games/", "GET", authHeader, nullptr))
        {
            lobbyFetched = true;
            lobbyCount = 0;
        }
        ENGINE_MEM_FREE(authHeader);
    }
    break;
    case RequestTypeUpdateStats:
    {
        char *authHeader = (char *)ENGINE_MEM_MALLOC(256);
        if (!authHeader)
        {
            break;
        }
        char *stats_payload = (char *)ENGINE_MEM_MALLOC(128);
        if (!stats_payload)
        {
            ENGINE_MEM_FREE(authHeader);
            break;
        }
        snprintf(stats_payload, 128, "{\"username\":\"%s\",\"xp\":%" PRIu32 "}", this->name, (uint32_t)this->xp);
        snprintf(authHeader, 256, "{\"Content-Type\":\"application/json\",\"Username\":\"%s\",\"Password\":\"%s\"}", this->name, this->password);
        if (!HTTP_SEND_REQUEST("https://www.jblanked.com/flipper/api/user/update-xp/", "POST", authHeader, stats_payload))
        {
            ENGINE_LOG_INFO("[Player:userRequest] Failed to update user stats");
        }
        ENGINE_MEM_FREE(authHeader);
        ENGINE_MEM_FREE(stats_payload);
    }
    break;
    default:
        ENGINE_LOG_INFO("[Player:userRequest] Unknown request type: %d", requestType);
        loginStatus = LoginRequestError;
        registrationStatus = RegistrationRequestError;
        userInfoStatus = UserInfoRequestError;
        ENGINE_MEM_FREE(payload);
        return;
    }

    ENGINE_MEM_FREE(payload);
}