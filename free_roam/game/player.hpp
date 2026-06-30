#pragma once
#include <furi.h>
#include <set>
#include <string>
#include "engine/entity.hpp"
#include "engine/game.hpp"
#include "engine/level.hpp"
#include "game/maps.hpp"
#include "game/general.hpp"
#include "game/loading.hpp"
#include "engine/draw.hpp"
#include "flipper_http/flipper_http.h"

class FreeRoamGame;

typedef enum
{
    GameViewTitle = 0,        // title, start, and menu (menu)
    GameViewSystemMenu = 1,   // profile, settings, about (menu)
    GameViewLobbyMenu = 2,    // local or online (menu)
    GameViewGameLocal = 3,    // game view local (gameplay)
    GameViewGameOnline = 4,   // game view online (gameplay)
    GameViewWelcome = 5,      // welcome view
    GameViewLogin = 6,        // login view
    GameViewRegistration = 7, // registration view
    GameViewUserInfo = 8,     // user info view
    GameViewLobbyBrowser = 9  // browse/join online games
} GameMainView;

typedef enum
{
    LoginCredentialsMissing = -1, // Credentials missing
    LoginSuccess = 0,             // Login successful
    LoginUserNotFound = 1,        // User not found
    LoginWrongPassword = 2,       // Wrong password
    LoginWaiting = 3,             // Waiting for response
    LoginNotStarted = 4,          // Login not started
    LoginRequestError = 5,        // Request error
} LoginStatus;

typedef enum
{
    RegistrationCredentialsMissing = -1, // Credentials missing
    RegistrationSuccess = 0,             // Registration successful
    RegistrationUserExists = 1,          // User already exists
    RegistrationRequestError = 2,        // Request error
    RegistrationNotStarted = 3,          // Registration not started
    RegistrationWaiting = 4,             // Waiting for response
} RegistrationStatus;

typedef enum
{
    UserInfoCredentialsMissing = -1, // Credentials missing
    UserInfoSuccess = 0,             // User info fetched successfully
    UserInfoRequestError = 1,        // Request error
    UserInfoNotStarted = 2,          // User info request not started
    UserInfoWaiting = 3,             // Waiting for response
    UserInfoParseError = 4,          // Error parsing user info
} UserInfoStatus;

typedef enum
{
    RequestTypeLogin = 0,        // Request login
    RequestTypeRegistration = 1, // Request registration
    RequestTypeUserInfo = 2,     // Request user info
    RequestTypeGameCreate = 3,   // Request to create a game
    RequestTypeGameList = 4,     // Request list of active games
} RequestType;

class Player : public Entity
{
public:
    Player(const char *name = "Player");
    ~Player();

    char player_name[64] = {0};
    //
    bool collisionMapCheck(Vector new_position);
    void drawCurrentView(Draw *canvas);
    void forceMapReload()
    {
        currentDynamicMap.reset();
    }
    uint8_t getCurrentGameState() const noexcept { return gameState; }
    GameMainView getCurrentMainView() const { return currentMainView; }
    HTTPState getHttpState();
    ToggleState getSoundToggle() const noexcept { return soundToggle; }
    ToggleState getVibrationToggle() const noexcept { return vibrationToggle; }
    void handleMenu(Draw *canvas, Game *game);
    bool httpRequestIsFinished();
    void processInput();
    void render(Draw *canvas, Game *game) override;
    void setFreeRoamGame(FreeRoamGame *game) { freeRoamGame = game; }
    void setGameState(GameState state) { gameState = state; }
    bool setHttpState(HTTPState state);
    void setInputKey(InputKey key) { lastInput = key; }
    void setLobbyMenuIndex(LobbyMenuIndex index) { currentLobbyMenuIndex = index; }
    void setLoginStatus(LoginStatus status) { loginStatus = status; }
    void setMainView(GameMainView view) { currentMainView = view; }
    void setRainFrame(uint8_t frame) { rainFrame = frame; }
    void setRegistrationStatus(RegistrationStatus status) { registrationStatus = status; }
    void setSoundToggle(ToggleState state) { soundToggle = state; }
    void setTitleIndex(TitleIndex index) { currentTitleIndex = index; }
    void setVibrationToggle(ToggleState state) { vibrationToggle = state; }
    void setUserInfoStatus(UserInfoStatus status) { userInfoStatus = status; }
    void setWelcomeFrame(uint8_t frame) { welcomeFrame = frame; }
    bool shouldLeaveGame() const noexcept { return leaveGame == ToggleOn; }
    void update(Game *game) override;
    void userRequest(RequestType requestType);

private:
    std::unique_ptr<DynamicMap> currentDynamicMap;             // current dynamic map
    LobbyMenuIndex currentLobbyMenuIndex = LobbyMenuLocal;     // current lobby menu index (must be in the GameViewLobbyMenu)
    MenuIndex currentMenuIndex = MenuIndexProfile;             // current menu index (must be in the GameViewSystemMenu)
    GameMainView currentMainView = GameViewWelcome;            // current main view of the game
    MenuSettingsIndex currentSettingsIndex = MenuSettingsMain; // current settings index (must be in the GameViewSystemMenu in the Settings tab)
    TitleIndex currentTitleIndex = TitleIndexStart;            // current title index (must be in the GameViewTitle)
    FreeRoamGame *freeRoamGame = nullptr;                      // Reference to the main game instance
    GameState gameState = GameStatePlaying;                    // current game state
    bool hasBeenPositioned = false;                            // Track if player has been positioned to prevent repeated resets
    bool justStarted = true;                                   // whether the player just started the game
    bool justSwitchedLevels = false;                           // whether the player just switched levels
    InputKey lastInput = InputKeyMAX;                          // Last input key
    ToggleState leaveGame = ToggleOff;                         // leave game toggle state
    uint8_t levelSwitchCounter = 0;                            // counter for level switch delay
    std::unique_ptr<Loading> loading;                          // loading animation instance
    LoginStatus loginStatus = LoginNotStarted;                 // Current login status
    OnlineGameState onlineGameState = OnlineStateIdle;         // online game connection state
    char onlineGameId[37] = {0};                               // UUID of the active game session
    uint16_t onlinePort = 0;                                   // WebSocket port assigned by the server
    // Lobby browser data
    static const int MAX_LOBBY_ENTRIES = 8;
    struct LobbyEntry
    {
        char game_id[37];
        char game_name[64];
    };
    LobbyEntry lobbyEntries[MAX_LOBBY_ENTRIES];
    int lobbyCount = 0;
    int lobbySelectedIndex = 0;
    bool lobbyFetched = false;
    uint8_t rainFrame = 0;                                          // frame counter for rain effect
    RegistrationStatus registrationStatus = RegistrationNotStarted; // Current registration status
    ToggleState showPlayerToggle = ToggleOn;                        // show/hide local player toggle
    ToggleState soundToggle = ToggleOn;                             // sound toggle state
    UserInfoStatus userInfoStatus = UserInfoNotStarted;             // Current user info status
    ToggleState vibrationToggle = ToggleOn;                         // vibration toggle state
    uint8_t welcomeFrame = 0;                                       // frame counter for welcome animation
    std::set<std::string> remotePlayerNamePool;                     // stable name storage for dynamically spawned remote player entities
    //
    void drawGameLocalView(Draw *canvas);                                                              // draw the local game view
    void drawGameOnlineView(Draw *canvas);                                                             // draw the online game view
    void drawLobbyBrowserView(Draw *canvas);                                                           // draw the lobby browser view (list/join online games)
    void drawLobbyMenuView(Draw *canvas);                                                              // draw the lobby menu view
    void drawLoginView(Draw *canvas);                                                                  // draw the login view
    void drawMenuType1(Draw *canvas, uint8_t selectedIndex, const char *option1, const char *option2); // draw the menu type 1 (used is out-game title, and lobby menu views)
    void drawMenuType2(Draw *canvas, uint8_t selectedIndexMain, uint8_t selectedIndexSettings);        // draw the menu type 2 (used in in-game and out-game system menu views)
    void drawRainEffect(Draw *canvas);                                                                 // draw rain effect on the canvas
    void drawRegistrationView(Draw *canvas);                                                           // draw the registration view
    void drawSystemMenuView(Draw *canvas);                                                             // draw the system menu view
    void drawTitleView(Draw *canvas);                                                                  // draw the title view
    void drawUserInfoView(Draw *canvas);                                                               // draw the user info view
    void drawWelcomeView(Draw *canvas);                                                                // draw the welcome view
    Vector findSafeSpawnPosition(const char *levelName);                                               // find a safe spawn position for a level
    bool isPositionSafe(Vector pos);                                                                   // check if a position is safe (not in a wall)
    void switchLevels(Game *game);                                                                     // switch levels based on the current dynamic map and level name
    void updateEntitiesFromServer(const char *json);                                                   // parse server entity state and update local entity positions
};