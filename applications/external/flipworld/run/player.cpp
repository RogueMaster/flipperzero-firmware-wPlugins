#include "run/player.hpp"
#include "flip_world_icons.h"
#include "run/run.hpp"
#include "run/general.hpp"
#include "app.hpp"
#include "jsmn/jsmn.h"
#include <math.h>

#ifndef COLOR_WHITE
#define COLOR_WHITE 0xFFFF
#endif

#ifndef COLOR_BLACK
#define COLOR_BLACK 0x0000
#endif

Player::Player()
    : Entity(
          "Player",
          ENTITY_PLAYER,
          Vector(384, 192),
          Vector(15, 11),
          new Image(Vector(15, 11), true, player_left_sword_15x11px),
          new Image(Vector(15, 11), true, player_left_sword_15x11px),
          new Image(Vector(15, 11), true, player_right_sword_15x11px)) {
    is_player = true; // Mark this entity as a player (so level doesn't delete it)
    end_position = Vector(384, 192); // Initialize end position
    start_position = Vector(384, 192); // Initialize start position
    strncpy(player_name, "Player", sizeof(player_name) - 1); // Copy default player name
    player_name[sizeof(player_name) - 1] = '\0'; // Ensure null termination
    name = player_name; // Point Entity's name to our writable buffer
    // Initialize player stats
    level = 1;
    health = 100;
    max_health = 100;
    strength = 10;
    attack_timer = 1;
    health_regen = 1;
}

Player::~Player() {
    // nothing to clean up for now
}

bool Player::areAllEnemiesDead(Game* game) {
    // Ensure we have a valid game and current level
    if(!game || !game->current_level) {
        FURI_LOG_E("Player", "areAllEnemiesDead: Invalid game or current level");
        return false;
    }

    // Get the current level
    Level* currentLevel = game->current_level;
    int totalEnemies = 0;
    int deadEnemies = 0;

    // Check all entities in the current level
    for(int i = 0; i < currentLevel->getEntityCount(); i++) {
        Entity* entity = currentLevel->getEntity(i);

        // Skip null entities
        if(!entity) {
            continue;
        }

        // Check if this is an enemy entity
        if(entity->type == ENTITY_ENEMY) {
            totalEnemies++;

            if(entity->state == ENTITY_DEAD) {
                deadEnemies++;
            } else {
                return false;
            }
        }
    }

    // Only return true if there were enemies to begin with and they're all dead
    if(totalEnemies == 0) {
        FURI_LOG_E("Player", "No enemies found in current level!");
        return false; // Don't switch levels if there are no enemies
    }

    return deadEnemies == totalEnemies;
}

void Player::checkForLevelCompletion(Game* game) {
    // Only check for level completion if we're in the game view and the game is running
    if(!flipWorldRun || !flipWorldRun->isRunning() || currentMainView != GameViewGame ||
       !this->hasChangedPosition()) {
        return;
    }

    // Update cooldown timer
    levelCompletionCooldown -= 1.0 / 60; // 60 fps
    if(levelCompletionCooldown > 0) {
        return; // Still in cooldown, don't check yet
    }

    // Don't check immediately after switching levels
    if(justSwitchedLevels) {
        justSwitchedLevels = false; // Reset the flag
        levelCompletionCooldown = 1.0; // Set a 1 second cooldown
        return;
    }

    // Check if all enemies are dead
    if(areAllEnemiesDead(game)) {
        // Get current level index
        LevelIndex currentLevelIndex = flipWorldRun->getCurrentLevelIndex();

        // Determine next level
        LevelIndex nextLevelIndex = LevelUnknown;
        switch(currentLevelIndex) {
        case LevelHomeWoods:
            nextLevelIndex = LevelRockWorld;
            break;
        case LevelRockWorld:
            nextLevelIndex = LevelForestWorld;
            break;
        case LevelForestWorld:
            // End of available levels
            nextLevelIndex = LevelHomeWoods;
            break;
        default:
            // Unknown level, start from the beginning
            nextLevelIndex = LevelHomeWoods;
            break;
        }

        // Switch to the next level if valid
        if(nextLevelIndex != LevelUnknown && flipWorldRun->getEngine() &&
           flipWorldRun->getEngine()->getGame()) {
            // Set game state to switching levels
            setGameState(GameStateSwitchingLevels);

            // Switch to the next level
            flipWorldRun->getEngine()->getGame()->level_switch((int)nextLevelIndex);

            // send level notice
            flipWorldRun->syncMultiplayerLevel();

            // Set the icon group for the new level
            flipWorldRun->setIconGroup(nextLevelIndex);

            // Reset player position to start position
            position = start_position;
            position_set(start_position);

            // Mark that we just switched levels
            justSwitchedLevels = true;

            // Reset player health to full for new level
            health = max_health;

            // Set cooldown to prevent immediate re-checking (2 seconds)
            levelCompletionCooldown = 2.0;

            // Return to playing state
            setGameState(GameStatePlaying);
        }
    }
}

void Player::drawCurrentView(Draw* canvas) {
    if(!canvas) return;

    switch(currentMainView) {
    case GameViewTitle:
        drawTitleView(canvas);
        break;
    case GameViewGame:
        // In-game view, render the game engine
        if(flipWorldRun->isRunning()) {
            if(flipWorldRun->getEngine()) {
                // Handle system menu input first, before the game engine processes input
                InputKey currentInput = flipWorldRun->getCurrentInput();
                if(currentInput == InputKeyBack) {
                    // Switch to system menu
                    currentMainView = GameViewSystemMenu;
                    flipWorldRun->resetInput(); // Reset input after handling
                    return; // Don't process game input this frame
                }

                flipWorldRun->getEngine()->updateGameInput(currentInput);
                // Reset the input after processing to prevent it from being continuously pressed
                flipWorldRun->resetInput();
                flipWorldRun->getEngine()->runAsync(false);
            }
            return;
        } else {
            canvas->fillScreen(COLOR_WHITE);
            canvas->setFont(FONT_SIZE_PRIMARY);
            canvas->text(25, 32, "Starting Game...", COLOR_BLACK);
            bool gameStarted = flipWorldRun->startGame();
            if(gameStarted && flipWorldRun->getEngine()) {
                flipWorldRun->getEngine()->runAsync(false); // Run the game engine immediately
            }
        }
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
    case GameViewLobbies:
        drawLobbiesView(canvas);
        break;
    case GameViewJoinLobby:
        drawJoinLobbyView(canvas);
        break;
    case GameViewSystemMenu:
        // Handle system menu input
        {
            InputKey currentInput = flipWorldRun->getCurrentInput();
            if(currentInput == InputKeyBack) {
                currentMainView = GameViewGame; // Switch back to game
                flipWorldRun->resetInput(); // Reset input after handling
                return; // Don't draw this frame
            } else if(currentInput == InputKeyUp) {
                if(currentSystemMenuIndex > MenuIndexProfile) {
                    currentSystemMenuIndex = static_cast<MenuIndex>(currentSystemMenuIndex - 1);
                }
                flipWorldRun->resetInput(); // Reset input after handling
            } else if(currentInput == InputKeyDown) {
                if(currentSystemMenuIndex < MenuIndexLeaveGame) {
                    currentSystemMenuIndex = static_cast<MenuIndex>(currentSystemMenuIndex + 1);
                }
                flipWorldRun->resetInput(); // Reset input after handling
            } else if(currentInput == InputKeyOk) {
                if(currentSystemMenuIndex == MenuIndexLeaveGame) {
                    // Handle Leave Game option
                    if(currentTitleIndex == TitleIndexPvE) {
                        userRequest(RequestTypeStopWebsocket); // Stop websocket connection
                    }
                    leaveGame = ToggleOn;
                    return;
                }
                flipWorldRun->resetInput(); // Reset input after handling
            } else if(currentInput != InputKeyMAX) {
                // Reset input for any other input that wasn't handled
                flipWorldRun->resetInput();
            }
        }
        drawSystemMenuView(canvas);
        break;
    default:
        canvas->fillScreen(COLOR_WHITE);
        canvas->text(0, 10, "Unknown View", COLOR_BLACK);
        break;
    }
}

void Player::drawJoinLobbyView(Draw* canvas) {
    static bool loadingStarted = false;
    switch(joinLobbyStatus) {
    case JoinLobbyWaiting:
        if(!loadingStarted) {
            if(!loading) {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if(loading) {
                loading->setText("Joining...");
            }
        }
        if(!this->httpRequestIsFinished()) {
            if(loading) {
                loading->animate();
            }
        } else {
            if(loading) {
                loading->stop();
            }
            loadingStarted = false;
            FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
            if(app->getHttpState() == ISSUE) {
                joinLobbyStatus = JoinLobbyRequestError;
                return;
            }
            char* response = (char*)malloc(512);
            if(app && app->loadChar("join_lobby", response, 512)) {
                // no need to check response this time, just join the game if a response is received
                // no issues in testing, but if needed, it does return [SUCCESS] in the message if successful
                joinLobbyStatus = JoinLobbySuccess;
                currentMainView = GameViewGame; // switch to game view
                userRequest(
                    RequestTypeStartWebsocket); // Start websocket connection for real-time updates
                flipWorldRun->setPvEMode(true); // we're in pve mode now!
                flipWorldRun->startGame();
            } else {
                joinLobbyStatus = JoinLobbyRequestError;
            }
            ::free(response);
        }
        break;
    case JoinLobbySuccess:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "User info loaded successfully!", COLOR_BLACK);
        canvas->text(0, 20, "Press OK to continue.", COLOR_BLACK);
        break;
    case JoinLobbyCredentialsMissing:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Missing credentials!", COLOR_BLACK);
        canvas->text(0, 20, "Please update your username", COLOR_BLACK);
        canvas->text(0, 30, "and password in the settings.", COLOR_BLACK);
        break;
    case JoinLobbyRequestError:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "User info request failed!", COLOR_BLACK);
        canvas->text(0, 20, "Check your network and", COLOR_BLACK);
        canvas->text(0, 30, "try again later.", COLOR_BLACK);
        break;
    case JoinLobbyParseError:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Failed to parse user info!", COLOR_BLACK);
        canvas->text(0, 20, "Try again...", COLOR_BLACK);
        break;
    default:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Loading user info...", COLOR_BLACK);
        break;
    }
}

void Player::drawLobbiesView(Draw* canvas) {
    static bool loadingStarted = false;
    switch(lobbiesStatus) {
    case LobbiesWaiting:
        if(!loadingStarted) {
            if(!loading) {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if(loading) {
                loading->setText("Fetching...");
            }
            userRequest(RequestTypeLobbies);
            return;
        }
        if(!this->httpRequestIsFinished()) {
            if(loading) {
                loading->animate();
            }
        } else {
            if(loading) {
                loading->stop();
            }
            loadingStarted = false;
            FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
            if(app->getHttpState() == ISSUE) {
                lobbiesStatus = LobbiesRequestError;
                return;
            }
            char* response = (char*)malloc(512);
            if(app && app->loadChar("lobbies", response, 512)) {
                lobbiesStatus = LobbiesSuccess;
                lobbyCount = 0;
                currentLobbyIndex = 0; // Reset selection to first lobby

                // parse the lobbies and store them
                for(uint32_t i = 0; i < 4; i++) {
                    char* lobby = get_json_array_value("lobbies", i, response);
                    if(!lobby) {
                        FURI_LOG_I(TAG, "No more lobbies found (total: %d)", lobbyCount);
                        break;
                    }

                    char* lobby_id = get_json_value("id", lobby);
                    char* lobby_name = get_json_value("name", lobby);
                    char* player_count = get_json_value("player_count", lobby);
                    char* max_players = get_json_value("max_players", lobby);

                    if(lobby_id && strlen(lobby_id) > 0) {
                        // Store lobby ID
                        strncpy(
                            lobbies[lobbyCount].id, lobby_id, sizeof(lobbies[lobbyCount].id) - 1);
                        lobbies[lobbyCount].id[sizeof(lobbies[lobbyCount].id) - 1] = '\0';

                        // Store lobby name (use ID if name is not available)
                        if(lobby_name && strlen(lobby_name) > 0) {
                            strncpy(
                                lobbies[lobbyCount].name,
                                lobby_name,
                                sizeof(lobbies[lobbyCount].name) - 1);
                            lobbies[lobbyCount].name[sizeof(lobbies[lobbyCount].name) - 1] = '\0';
                        } else {
                            snprintf(
                                lobbies[lobbyCount].name,
                                sizeof(lobbies[lobbyCount].name),
                                "Lobby %s",
                                lobby_id);
                        }

                        // Store player counts
                        lobbies[lobbyCount].playerCount = player_count ? atoi(player_count) : 0;
                        lobbies[lobbyCount].maxPlayers = max_players ? atoi(max_players) : 10;

                        lobbyCount++;
                    } else {
                        FURI_LOG_E(TAG, "Failed to get lobby ID for lobby %lu", (unsigned long)i);
                    }

                    // Free the parsed strings
                    free(lobby);
                    if(lobby_id) free(lobby_id);
                    if(lobby_name) free(lobby_name);
                    if(player_count) free(player_count);
                    if(max_players) free(max_players);
                }

                if(lobbyCount == 0) {
                    FURI_LOG_E(TAG, "No valid lobbies found in response");
                }

                ::free(response);
            } else {
                lobbiesStatus = LobbiesRequestError;
                ::free(response);
            }
        }
        break;
    case LobbiesSuccess:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(5, 10, "Select a Lobby:", COLOR_BLACK);

        if(lobbyCount == 0) {
            canvas->text(5, 25, "No lobbies available", COLOR_BLACK);
            canvas->setFont(FONT_SIZE_SECONDARY);
            canvas->text(5, 40, "Press Back to return", COLOR_BLACK);
        } else {
            // Display lobbies in a selectable menu format
            int startY = 20;
            int itemHeight = 12;
            int maxDisplayItems = 4; // Maximum items to display at once

            // Calculate which lobbies to display (scrolling if needed)
            int displayStart = 0;
            if(lobbyCount > maxDisplayItems) {
                displayStart = currentLobbyIndex - maxDisplayItems / 2;
                if(displayStart < 0) displayStart = 0;
                if(displayStart + maxDisplayItems > lobbyCount)
                    displayStart = lobbyCount - maxDisplayItems;
            }

            for(int i = 0; i < maxDisplayItems && (displayStart + i) < lobbyCount; i++) {
                int lobbyIndex = displayStart + i;
                int y = startY + i * itemHeight;

                // Highlight the selected lobby
                if(lobbyIndex == currentLobbyIndex) {
                    canvas->fillRectangle(3, y - 2, 122, itemHeight, COLOR_BLACK);
                }

                // Display lobby name and player count
                char lobbyText[80];
                // Truncate lobby name if it's too long
                char truncatedName[32];
                strncpy(truncatedName, lobbies[lobbyIndex].name, sizeof(truncatedName) - 1);
                truncatedName[sizeof(truncatedName) - 1] = '\0';

                snprintf(
                    lobbyText,
                    sizeof(lobbyText),
                    "%s (%d/%d)",
                    truncatedName,
                    lobbies[lobbyIndex].playerCount,
                    lobbies[lobbyIndex].maxPlayers);

                canvas->setFont(FONT_SIZE_SECONDARY);
                canvas->text(
                    5,
                    y + 7,
                    lobbyText,
                    lobbyIndex == currentLobbyIndex ? COLOR_WHITE : COLOR_BLACK);
            }
        }
        break;
    case LobbiesCredentialsMissing:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Missing credentials!", COLOR_BLACK);
        canvas->text(0, 20, "Please update your username", COLOR_BLACK);
        canvas->text(0, 30, "and password in the settings.", COLOR_BLACK);
        break;
    case LobbiesRequestError:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "User info request failed!", COLOR_BLACK);
        canvas->text(0, 20, "Check your network and", COLOR_BLACK);
        canvas->text(0, 30, "try again later.", COLOR_BLACK);
        break;
    case LobbiesParseError:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Failed to parse user info!", COLOR_BLACK);
        canvas->text(0, 20, "Try again...", COLOR_BLACK);
        break;
    default:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Loading user info...", COLOR_BLACK);
        break;
    }
}

void Player::drawLoginView(Draw* canvas) {
    canvas->fillScreen(COLOR_WHITE);
    canvas->setFont(FONT_SIZE_PRIMARY);
    static bool loadingStarted = false;
    switch(loginStatus) {
    case LoginWaiting:
        if(!loadingStarted) {
            if(!loading) {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if(loading) {
                loading->setText("Logging in...");
            }
        }
        if(!this->httpRequestIsFinished()) {
            if(loading) {
                loading->animate();
            }
        } else {
            if(loading) {
                loading->stop();
            }
            loadingStarted = false;
            FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
            if(app->getHttpState() == ISSUE) {
                loginStatus = LoginRequestError;
                return;
            }
            char response[256];
            if(app && app->loadChar("login", response, sizeof(response))) {
                if(strstr(response, "[SUCCESS]") != NULL) {
                    loginStatus = LoginSuccess;
                    currentMainView = GameViewUserInfo; // switch to user info view
                    userInfoStatus = UserInfoWaiting; // set user info status to waiting
                    userRequest(RequestTypeUserInfo);
                } else if(strstr(response, "User not found") != NULL) {
                    loginStatus = LoginNotStarted;
                    currentMainView = GameViewRegistration;
                    registrationStatus = RegistrationWaiting;
                    userRequest(RequestTypeRegistration);
                } else if(strstr(response, "Incorrect password") != NULL) {
                    loginStatus = LoginWrongPassword;
                } else if(strstr(response, "Username or password is empty.") != NULL) {
                    loginStatus = LoginCredentialsMissing;
                } else {
                    loginStatus = LoginRequestError;
                }
            } else {
                loginStatus = LoginRequestError;
            }
        }
        break;
    case LoginSuccess:
        canvas->text(0, 10, "Login successful!", COLOR_BLACK);
        canvas->text(0, 20, "Press OK to continue.", COLOR_BLACK);
        break;
    case LoginCredentialsMissing:
        canvas->text(0, 10, "Missing credentials!", COLOR_BLACK);
        canvas->text(0, 20, "Please set your username", COLOR_BLACK);
        canvas->text(0, 30, "and password in the app.", COLOR_BLACK);
        break;
    case LoginRequestError:
        canvas->text(0, 10, "Login request failed!", COLOR_BLACK);
        canvas->text(0, 20, "Check your network and", COLOR_BLACK);
        canvas->text(0, 30, "try again later.", COLOR_BLACK);
        break;
    case LoginWrongPassword:
        canvas->text(0, 10, "Wrong password!", COLOR_BLACK);
        canvas->text(0, 20, "Please check your password", COLOR_BLACK);
        canvas->text(0, 30, "and try again.", COLOR_BLACK);
        break;
    default:
        canvas->text(0, 10, "Logging in...", COLOR_BLACK);
        break;
    }
}

void Player::drawRainEffect(Draw* canvas) {
    // rain droplets/star droplets effect
    for(int i = 0; i < 8; i++) {
        // Use pseudo-random offsets based on frame and droplet index
        uint8_t seed = (rainFrame + i * 37) & 0xFF;
        uint8_t x = (rainFrame + seed * 13) & 0x7F;
        uint8_t y = (rainFrame * 2 + seed * 7 + i * 23) & 0x3F;

        // Draw star-like droplet
        canvas->pixel(x, y, COLOR_BLACK);
        canvas->pixel(x - 1, y, COLOR_BLACK);
        canvas->pixel(x + 1, y, COLOR_BLACK);
        canvas->pixel(x, y - 1, COLOR_BLACK);
        canvas->pixel(x, y + 1, COLOR_BLACK);
    }

    rainFrame += 1;
    if(rainFrame > 128) {
        rainFrame = 0;
    }
}

void Player::drawRegistrationView(Draw* canvas) {
    canvas->fillScreen(COLOR_WHITE);
    canvas->setFont(FONT_SIZE_PRIMARY);
    static bool loadingStarted = false;
    switch(registrationStatus) {
    case RegistrationWaiting:
        if(!loadingStarted) {
            if(!loading) {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if(loading) {
                loading->setText("Registering...");
            }
        }
        if(!this->httpRequestIsFinished()) {
            if(loading) {
                loading->animate();
            }
        } else {
            if(loading) {
                loading->stop();
            }
            loadingStarted = false;
            FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
            if(app->getHttpState() == ISSUE) {
                registrationStatus = RegistrationRequestError;
                return;
            }
            char response[256];
            if(app && app->loadChar("register", response, sizeof(response))) {
                if(strstr(response, "[SUCCESS]") != NULL) {
                    registrationStatus = RegistrationSuccess;
                    currentMainView = GameViewUserInfo; // switch to user info view
                    userInfoStatus = UserInfoWaiting; // set user info status to waiting
                    userRequest(RequestTypeUserInfo);
                } else if(strstr(response, "Username or password not provided") != NULL) {
                    registrationStatus = RegistrationCredentialsMissing;
                } else if(strstr(response, "User already exists") != NULL) {
                    registrationStatus = RegistrationUserExists;
                } else {
                    registrationStatus = RegistrationRequestError;
                }
            } else {
                registrationStatus = RegistrationRequestError;
            }
        }
        break;
    case RegistrationSuccess:
        canvas->text(0, 10, "Registration successful!", COLOR_BLACK);
        canvas->text(0, 20, "Press OK to continue.", COLOR_BLACK);
        break;
    case RegistrationCredentialsMissing:
        canvas->text(0, 10, "Missing credentials!", COLOR_BLACK);
        canvas->text(0, 20, "Please update your username", COLOR_BLACK);
        canvas->text(0, 30, "and password in the settings.", COLOR_BLACK);
        break;
    case RegistrationRequestError:
        canvas->text(0, 10, "Registration request failed!", COLOR_BLACK);
        canvas->text(0, 20, "Check your network and", COLOR_BLACK);
        canvas->text(0, 30, "try again later.", COLOR_BLACK);
        break;
    default:
        canvas->text(0, 10, "Registering...", COLOR_BLACK);
        break;
    }
}

void Player::drawSystemMenuView(Draw* canvas) {
    switch(currentSystemMenuIndex) {
    case MenuIndexProfile:
        // draw info
        // first option is highlighted
        char health_str[32];
        char xp_str[32];
        char level_str[32];
        char strength_str[32];

        snprintf(level_str, sizeof(level_str), "Level   : %.0f", (double)level);
        snprintf(health_str, sizeof(health_str), "Health  : %.0f", (double)health);
        snprintf(xp_str, sizeof(xp_str), "XP      : %.0f", (double)xp);
        snprintf(strength_str, sizeof(strength_str), "Strength: %.0f", (double)strength);

        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(7, 16, name);
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(7, 30, level_str);
        canvas->text(7, 37, health_str);
        canvas->text(7, 44, xp_str);
        canvas->text(7, 51, strength_str);

        canvas->rectangle(80, 12, 36, 42, COLOR_BLACK);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(86, 22, "Info");
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(86, 32, "More");
        canvas->text(86, 42, "Quit");
        break;
    case MenuIndexAbout:
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(7, 16, VERSION_TAG);
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(7, 25, "Developed by");
        canvas->text(7, 32, "JBlanked and Derek");
        canvas->text(7, 39, "Jamison. Graphics");
        canvas->text(7, 46, "from Pr3!");
        canvas->text(7, 60, "www.github.com/jblanked");

        // draw a box around the selected option
        canvas->rectangle(80, 12, 36, 42, COLOR_BLACK);
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(86, 22, "Info");
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(86, 32, "More");
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(86, 42, "Quit");
        break;
    case MenuIndexLeaveGame:
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(7, 16, "Leave Game");
        canvas->setFont(FONT_SIZE_SMALL);
        canvas->text(7, 32, "Are you sure you");
        canvas->text(7, 39, "want to leave");
        canvas->text(7, 46, "the game?");

        // draw a box around the selected option
        canvas->rectangle(80, 12, 36, 42, COLOR_BLACK);
        canvas->setFont(FONT_SIZE_SECONDARY);
        canvas->text(86, 22, "Info");
        canvas->text(86, 32, "More");
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(86, 42, "Quit");
        break;
    default:
        break;
    }
}

void Player::drawTitleView(Draw* canvas) {
    canvas->fillScreen(COLOR_WHITE);

    // rain effect
    drawRainEffect(canvas);

    // draw lobby text
    if(currentTitleIndex == 0) {
        canvas->fillRectangle(36, 16, 56, 16, COLOR_BLACK);
        canvas->text(54, 27, "Story", COLOR_WHITE);
        canvas->fillRectangle(36, 32, 56, 16, COLOR_WHITE);
        canvas->text(54, 42, "PvE", COLOR_BLACK);
    } else if(currentTitleIndex == 1) {
        canvas->fillRectangle(36, 16, 56, 16, COLOR_WHITE);
        canvas->text(54, 27, "Story", COLOR_BLACK);
        canvas->fillRectangle(36, 32, 56, 16, COLOR_BLACK);
        canvas->text(54, 42, "PvE", COLOR_WHITE);
    }
}

void Player::drawUsername(Vector pos, Game* game) {
    // Calculate screen position after applying camera offset
    float screen_x = pos.x - game->pos.x;
    float screen_y = pos.y - game->pos.y;

    // Check if the username would be visible on the 128x64 screen
    float text_width = strlen(name) * 4 + 1; // Approximate text width
    if(screen_x - text_width / 2 < 0 || screen_x + text_width / 2 > 128 || screen_y - 10 < 0 ||
       screen_y > 64) {
        return;
    }

    // draw box around the name
    game->draw->fillRectangle(
        screen_x - (strlen(name) * 2) - 1, screen_y - 7, strlen(name) * 4 + 1, 8, COLOR_WHITE);

    // draw name over player's head
    game->draw->text(screen_x - (strlen(name) * 2), screen_y - 2, name, COLOR_BLACK);
}

void Player::drawUserInfoView(Draw* canvas) {
    static bool loadingStarted = false;
    switch(userInfoStatus) {
    case UserInfoWaiting:
        if(!loadingStarted) {
            if(!loading) {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if(loading) {
                loading->setText("Syncing...");
            }
        }
        if(!this->httpRequestIsFinished()) {
            if(loading) {
                loading->animate();
            }
        } else {
            canvas->text(0, 10, "Loading user info...", COLOR_BLACK);
            canvas->text(0, 20, "Please wait...", COLOR_BLACK);
            canvas->text(0, 30, "It may take up to 15 seconds.", COLOR_BLACK);
            FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
            if(app->getHttpState() == ISSUE) {
                userInfoStatus = UserInfoRequestError;
                if(loading) {
                    loading->stop();
                }
                loadingStarted = false;
                return;
            }
            char response[512];
            if(app && app->loadChar("user_info", response, sizeof(response))) {
                userInfoStatus = UserInfoSuccess;
                // they're in! let's go
                char* game_stats = get_json_value("game_stats", response);
                if(!game_stats) {
                    FURI_LOG_E("Player", "Failed to parse game_stats");
                    userInfoStatus = UserInfoParseError;
                    if(loading) {
                        loading->stop();
                    }
                    loadingStarted = false;
                    return;
                }
                canvas->fillScreen(COLOR_WHITE);
                canvas->text(0, 10, "User info loaded!", COLOR_BLACK);
                char* username = get_json_value("username", game_stats);
                char* level = get_json_value("level", game_stats);
                char* xp = get_json_value("xp", game_stats);
                char* health = get_json_value("health", game_stats);
                char* strength = get_json_value("strength", game_stats);
                char* max_health = get_json_value("max_health", game_stats);
                if(!username || !level || !xp || !health || !strength || !max_health) {
                    FURI_LOG_E("Player", "Failed to parse user info");
                    userInfoStatus = UserInfoParseError;
                    if(username) ::free(username);
                    if(level) ::free(level);
                    if(xp) ::free(xp);
                    if(health) ::free(health);
                    if(strength) ::free(strength);
                    if(max_health) ::free(max_health);
                    ::free(game_stats);
                    if(loading) {
                        loading->stop();
                    }
                    loadingStarted = false;
                    return;
                }

                canvas->fillScreen(COLOR_WHITE);
                canvas->text(0, 10, "User data found!", COLOR_BLACK);

                // Update player info
                snprintf(player_name, sizeof(player_name), "%s", username);
                name = player_name;
                this->level = atoi(level);
                this->xp = atoi(xp);
                this->health = atoi(health);
                this->strength = atoi(strength);
                this->max_health = atoi(max_health);

                canvas->fillScreen(COLOR_WHITE);
                canvas->text(0, 10, "Player info updated!", COLOR_BLACK);

                // clean em up gang
                ::free(username);
                ::free(level);
                ::free(xp);
                ::free(health);
                ::free(strength);
                ::free(max_health);
                ::free(game_stats);

                if(loading) {
                    loading->stop();
                }
                loadingStarted = false;

                // if story, start immediately, otherwise load the lobbies
                if(currentTitleIndex == TitleIndexStory) {
                    currentMainView = GameViewGame; // switch to game view
                    flipWorldRun->startGame();
                } else {
                    currentMainView = GameViewLobbies; // switch to lobbies view
                    lobbiesStatus = LobbiesWaiting; // set lobbies status to waiting
                }
                return;
            } else {
                userInfoStatus = UserInfoRequestError;
            }
        }
        break;
    case UserInfoSuccess:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "User info loaded successfully!", COLOR_BLACK);
        canvas->text(0, 20, "Press OK to continue.", COLOR_BLACK);
        break;
    case UserInfoCredentialsMissing:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Missing credentials!", COLOR_BLACK);
        canvas->text(0, 20, "Please update your username", COLOR_BLACK);
        canvas->text(0, 30, "and password in the settings.", COLOR_BLACK);
        break;
    case UserInfoRequestError:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "User info request failed!", COLOR_BLACK);
        canvas->text(0, 20, "Check your network and", COLOR_BLACK);
        canvas->text(0, 30, "try again later.", COLOR_BLACK);
        break;
    case UserInfoParseError:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Failed to parse user info!", COLOR_BLACK);
        canvas->text(0, 20, "Try again...", COLOR_BLACK);
        break;
    default:
        canvas->fillScreen(COLOR_WHITE);
        canvas->setFont(FONT_SIZE_PRIMARY);
        canvas->text(0, 10, "Loading user info...", COLOR_BLACK);
        break;
    }
}

void Player::drawUserStats(Vector pos, Draw* canvas) {
    // first draw a white rectangle to make the text more readable
    canvas->fillRectangle(pos.x - 1, pos.y - 7, 34, 21, COLOR_WHITE);

    char health_str[32];
    char xp_str[32];
    char level_str[32];

    snprintf(health_str, sizeof(health_str), "HP : %.0f", (double)health);
    snprintf(level_str, sizeof(level_str), "LVL: %.0f", (double)level);

    if(xp < 10000)
        snprintf(xp_str, sizeof(xp_str), "XP : %.0f", (double)xp);
    else
        snprintf(xp_str, sizeof(xp_str), "XP : %.0fK", (double)xp / 1000);

    // draw items
    canvas->setFont(FONT_SIZE_SMALL);
    canvas->text(pos.x, pos.y, health_str, COLOR_BLACK);
    canvas->text(pos.x, pos.y + 7, xp_str, COLOR_BLACK);
    canvas->text(pos.x, pos.y + 14, level_str, COLOR_BLACK);
}

HTTPState Player::getHttpState() {
    if(!flipWorldRun) {
        return INACTIVE;
    }

    // Get app context to check HTTP state
    FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
    if(!app) {
        return INACTIVE;
    }

    return app->getHttpState();
}

IconGroupContext* Player::getIconGroupContext() const {
    if(!flipWorldRun) {
        return nullptr;
    }
    return flipWorldRun->currentIconGroup;
}

bool Player::httpRequestIsFinished() {
    if(!flipWorldRun) {
        return true; // Default to finished if no game context
    }

    // Get app context to check HTTP state
    FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
    if(!app) {
        return true; // Default to finished if no app context
    }

    // Check if HTTP request is finished
    auto state = app->getHttpState();
    return state == IDLE || state == ISSUE || state == INACTIVE;
}

void Player::iconGroupRender(Game* game) {
    if(!flipWorldRun || !game || !game->draw) {
        return; // Ensure we have a valid game and draw context
    }
    auto iconGroupContext = getIconGroupContext();
    for(int i = 0; i < iconGroupContext->count; i++) {
        auto spec = &iconGroupContext->icons[i];
        int x_pos = spec->pos.x - game->pos.x - (spec->size.x / 2);
        int y_pos = spec->pos.y - game->pos.y - (spec->size.y / 2);
        if(x_pos + spec->size.x < 0 || x_pos > 128 || y_pos + spec->size.y < 0 || y_pos > 64) {
            continue;
        }
        game->draw->image(x_pos, y_pos, spec->icon, spec->size.x, spec->size.y);
    }
}

void Player::processInput() {
    if(!flipWorldRun) {
        return;
    }

    InputKey currentInput = lastInput;

    if(currentInput == InputKeyMAX) {
        return;
    }

    switch(currentMainView) {
    case GameViewTitle:
        // Handle title view navigation
        switch(currentInput) {
        case InputKeyUp:
            currentTitleIndex = TitleIndexStory;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyDown:
            currentTitleIndex = TitleIndexPvE;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyOk:
            flipWorldRun->shouldDebounce = true;
            currentMainView = GameViewLogin; // Switch to login view
            loginStatus = LoginWaiting;
            userRequest(RequestTypeLogin);
            break;
        case InputKeyBack:
            leaveGame = ToggleOn;
            flipWorldRun->shouldDebounce = true;
            break;
        default:
            break;
        }
        break;
    case GameViewLogin:
        switch(currentInput) {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyOk:
            if(loginStatus == LoginSuccess) {
                currentMainView = GameViewUserInfo;
                userInfoStatus = UserInfoWaiting;
                userRequest(RequestTypeUserInfo);
                flipWorldRun->shouldDebounce = true;
            }
            break;
        default:
            break;
        }
        break;

    case GameViewRegistration:
        switch(currentInput) {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyOk:
            if(registrationStatus == RegistrationSuccess) {
                currentMainView = GameViewUserInfo;
                userInfoStatus = UserInfoWaiting;
                userRequest(RequestTypeUserInfo);
                flipWorldRun->shouldDebounce = true;
            }
            break;
        default:
            break;
        }
        break;

    case GameViewUserInfo:
        switch(currentInput) {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            flipWorldRun->shouldDebounce = true;
            break;
        default:
            break;
        }
        break;

    case GameViewGame:
        // Input handling for GameViewGame is done in update() when the game is running
        break;

    case GameViewLobbies:
        switch(currentInput) {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyUp:
            if(lobbiesStatus == LobbiesSuccess && lobbyCount > 0) {
                currentLobbyIndex = (currentLobbyIndex - 1 + lobbyCount) % lobbyCount;
                flipWorldRun->shouldDebounce = true;
            }
            break;
        case InputKeyDown:
            if(lobbiesStatus == LobbiesSuccess && lobbyCount > 0) {
                currentLobbyIndex = (currentLobbyIndex + 1) % lobbyCount;
                flipWorldRun->shouldDebounce = true;
            }
            break;
        case InputKeyOk:
            if(lobbiesStatus == LobbiesSuccess && lobbyCount > 0 &&
               currentLobbyIndex < lobbyCount) {
                currentMainView = GameViewJoinLobby;
                flipWorldRun->shouldDebounce = true;
                joinLobbyStatus = JoinLobbyWaiting;
                // if no one is in lobby, then we are the host
                flipWorldRun->setIsLobbyHost(lobbies[currentLobbyIndex].playerCount == 0);
                userRequest(RequestTypeJoinLobby);
            } else if(lobbiesStatus != LobbiesSuccess) {
                // If lobbies failed to load, go back to title
                currentMainView = GameViewTitle;
                flipWorldRun->shouldDebounce = true;
            }
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}

void Player::render(Draw* canvas, Game* game) {
    if(currentMainView != GameViewGame) {
        // If not in game view, skip player rendering
        return;
    }
    iconGroupRender(game);
    drawUsername(position, game);
    drawUserStats(Vector(0, 50), canvas);
}

bool Player::setHttpState(HTTPState state) {
    if(!flipWorldRun) {
        return false;
    }

    // Get app context to set HTTP state
    FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
    if(!app) {
        return false;
    }

    return app->setHttpState(state);
}

void Player::syncMultiplayerState() {
    if(!flipWorldRun) {
        return;
    }

    flipWorldRun->syncMultiplayerEntity(this);
}

void Player::update(Game* game) {
    if(currentMainView != GameViewGame) {
        // If not in game view, skip player updates
        return;
    }

    // Apply health regeneration
    elapsed_health_regen += 0.05f;
    if(elapsed_health_regen >= 1 && health < max_health) {
        health += health_regen;
        elapsed_health_regen = 0;
        if(health > max_health) {
            health = max_health;
        }
    }

    // Increment the elapsed_attack_timer for the player
    elapsed_attack_timer += 0.05f;

    // update player traits
    updateStats();

    // Check if all enemies are dead and switch to next level if needed
    checkForLevelCompletion(game);

    Vector oldPos = position;
    Vector newPos = oldPos;
    bool shouldSetPosition = false;

    // Handle input based on current view
    if(game->input == InputKeyUp) {
        newPos.y -= 5;
        direction = ENTITY_UP;
        shouldSetPosition = true;
    } else if(game->input == InputKeyDown) {
        newPos.y += 5;
        direction = ENTITY_DOWN;
        shouldSetPosition = true;
    } else if(game->input == InputKeyLeft) {
        newPos.x -= 5;
        direction = ENTITY_LEFT;
        shouldSetPosition = true;
    } else if(game->input == InputKeyRight) {
        newPos.x += 5;
        direction = ENTITY_RIGHT;
        shouldSetPosition = true;
    }

    // reset input
    game->input = InputKeyMAX;

    // check if new position is within the level boundaries
    if(newPos.x < 0 || newPos.x + size.x > game->size.x || newPos.y < 0 ||
       newPos.y + size.y > game->size.y) {
        // restore old position
        shouldSetPosition = false;
    }

    // update player sprite based on direction
    if(direction == ENTITY_LEFT) {
        sprite = sprite_left;
    } else if(direction == ENTITY_RIGHT) {
        sprite = sprite_right;
    }

    // Only check for collisions if we're actually trying to move
    if(shouldSetPosition) {
        bool hasCollision = false;

        // Loop over all icon specifications in the current icon group.
        for(int i = 0; i < getIconGroupContext()->count; i++) {
            IconSpec* spec = &getIconGroupContext()->icons[i];

            // Calculate the difference between the NEW position and the icon's center.
            float dx = newPos.x - spec->pos.x;
            float dy = newPos.y - spec->pos.y;

            // approximate collision radius:
            float radius = (spec->size.x + spec->size.y) / 4.0f;

            // Collision: if player's distance to the icon center is less than the collision radius.
            if((dx * dx + dy * dy) < (radius * radius)) {
                hasCollision = true;
                break;
            }
        }

        // Only update position if there's no collision
        if(!hasCollision) {
            position_set(newPos);
            syncMultiplayerState();
        }
        // If there's a collision, we simply don't move (stay at current position)
    }

    // Store the current camera position before updating
    game->old_pos = game->pos;

    // Update camera position to center the player
    // The viewport is 128x64, so we want the player to be at the center of this viewport
    float viewport_width = 128.0f;
    float viewport_height = 64.0f;

    float camera_x = position.x - (viewport_width / 2);
    float camera_y = position.y - (viewport_height / 2);

    // Clamp camera position to ensure we don't show areas outside the world
    // Camera position represents the top-left corner of what we see
    camera_x = constrain(camera_x, 0, game->size.x - viewport_width);
    camera_y = constrain(camera_y, 0, game->size.y - viewport_height);

    // Set the new camera position
    game->pos.x = camera_x;
    game->pos.y = camera_y;
}

void Player::updateStats() {
    if(this->xp == this->old_xp) {
        return; // No change in XP, no need to update stats
    }

    // Determine the player's level based on XP
    level = 1;
    uint32_t xp_required = 100; // Base XP for level 2

    while(level < 100 && xp >= xp_required) // Maximum level supported
    {
        level++;
        xp_required = (uint32_t)(xp_required * 1.5); // 1.5 growth factor per level
    }

    // Update strength and max health based on the new level
    strength = 10 + (level * 1); // 1 strength per level
    max_health = 100 + ((level - 1) * 10); // 10 health per level

    this->old_xp = this->xp;
}

void Player::userRequest(RequestType requestType) {
    if(!flipWorldRun) {
        FURI_LOG_E("Player", "userRequest: FlipWorldRun instance is null");
        return;
    }

    // Get app context to access HTTP functionality
    FlipWorldApp* app = static_cast<FlipWorldApp*>(flipWorldRun->appContext);
    if(!app) {
        FURI_LOG_E("Player", "userRequest: App context is null");
        return;
    }

    // Allocate memory for credentials
    char* username = (char*)malloc(64);
    char* password = (char*)malloc(64);
    if(!username || !password) {
        FURI_LOG_E("Player", "userRequest: Failed to allocate memory for credentials");
        if(username) free(username);
        if(password) free(password);
        return;
    }

    // Load credentials from storage
    bool credentialsLoaded = true;
    if(!app->loadChar("user_name", username, 64, "flipper_http")) {
        FURI_LOG_E("Player", "Failed to load user_name");
        credentialsLoaded = false;
    }
    if(!app->loadChar("user_pass", password, 64, "flipper_http")) {
        FURI_LOG_E("Player", "Failed to load user_pass");
        credentialsLoaded = false;
    }

    if(!credentialsLoaded) {
        switch(requestType) {
        case RequestTypeLogin:
            loginStatus = LoginCredentialsMissing;
            break;
        case RequestTypeRegistration:
            registrationStatus = RegistrationCredentialsMissing;
            break;
        case RequestTypeUserInfo:
            userInfoStatus = UserInfoCredentialsMissing;
            break;
        case RequestTypeLobbies:
            lobbiesStatus = LobbiesCredentialsMissing;
            break;
        default:
            FURI_LOG_E("Player", "Unknown request type: %d", requestType);
            loginStatus = LoginRequestError;
            registrationStatus = RegistrationRequestError;
            userInfoStatus = UserInfoRequestError;
            lobbiesStatus = LobbiesRequestError;
            break;
        }
        free(username);
        free(password);
        return;
    }

    // Create JSON payload for login/registration
    char* payload = (char*)malloc(256);
    if(!payload) {
        FURI_LOG_E("Player", "userRequest: Failed to allocate memory for payload");
        free(username);
        free(password);
        return;
    }
    snprintf(payload, 256, "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);

    switch(requestType) {
    case RequestTypeLogin:
        if(!app->httpRequestAsync(
               "login.txt",
               "https://www.jblanked.com/flipper/api/user/login/",
               POST,
               "{\"Content-Type\":\"application/json\"}",
               payload)) {
            loginStatus = LoginRequestError;
        }
        break;
    case RequestTypeRegistration:
        if(!app->httpRequestAsync(
               "register.txt",
               "https://www.jblanked.com/flipper/api/user/register/",
               POST,
               "{\"Content-Type\":\"application/json\"}",
               payload)) {
            registrationStatus = RegistrationRequestError;
        }
        break;
    case RequestTypeUserInfo: {
        char* url = (char*)malloc(128);
        if(!url) {
            FURI_LOG_E("Player", "userRequest: Failed to allocate memory for url");
            userInfoStatus = UserInfoRequestError;
            free(username);
            free(password);
            free(payload);
            return;
        }
        snprintf(url, 128, "https://www.jblanked.com/flipper/api/user/game-stats/%s/", username);
        if(!app->httpRequestAsync(
               "user_info.txt", url, GET, "{\"Content-Type\":\"application/json\"}")) {
            userInfoStatus = UserInfoRequestError;
        }
        free(url);
    } break;
    case RequestTypeLobbies: {
        // 10 max players, 4 max lobbies
        if(!app->httpRequestAsync(
               "lobbies.txt",
               "https://www.jblanked.com/flipper/api/world/pve/lobbies/10/4/",
               GET,
               "{\"Content-Type\":\"application/json\"}")) {
            lobbiesStatus = LobbiesRequestError;
        }
    } break;
    case RequestTypeJoinLobby: {
        char* payload2 = (char*)malloc(256);
        if(!payload2) {
            FURI_LOG_E("Player", "userRequest: Failed to allocate memory for payload2");
            joinLobbyStatus = JoinLobbyRequestError;
            free(username);
            free(password);
            free(payload);
            return;
        }
        snprintf(
            payload2,
            256,
            "{\"username\":\"%s\", \"game_id\":\"%s\"}",
            username,
            lobbies[currentLobbyIndex].id);
        if(!app->httpRequestAsync(
               "join_lobby.txt",
               "https://www.jblanked.com/flipper/api/world/pve/lobby/join/",
               POST,
               "{\"Content-Type\":\"application/json\"}",
               payload2)) {
            joinLobbyStatus = JoinLobbyRequestError;
        }
    } break;
    case RequestTypeStartWebsocket: {
        char* websocket_url = (char*)malloc(128);
        snprintf(
            websocket_url, 128, "ws://www.jblanked.com/ws/game/%s/", lobbies[currentLobbyIndex].id);
        // Start the WebSocket connection for the lobby
        if(!app->websocketStart(websocket_url)) {
            joinLobbyStatus = JoinLobbyRequestError;
        }
        free(websocket_url);
    } break;
    case RequestTypeStopWebsocket:
        // Stop the WebSocket connection
        if(!app->websocketStop()) {
            FURI_LOG_E("Player", "Failed to stop WebSocket connection");
        }
        break;
    case RequestTypeSaveStats: {
        const char* playerJson = flipWorldRun->entityToJson(this);
        if(!app->httpRequestAsync(
               "save_stats.txt",
               "https://www.jblanked.com/flipper/api/user/update-game-stats/",
               POST,
               "{\"Content-Type\":\"application/json\"}",
               playerJson)) {
            FURI_LOG_E("Player", "Failed to save player stats");
        }
        free((void*)playerJson);
    } break;
    default:
        FURI_LOG_E("Player", "Unknown request type: %d", requestType);
        loginStatus = LoginRequestError;
        registrationStatus = RegistrationRequestError;
        userInfoStatus = UserInfoRequestError;
        lobbiesStatus = LobbiesRequestError;
        free(username);
        free(password);
        free(payload);
        return;
    }

    free(username);
    free(password);
    free(payload);
}
