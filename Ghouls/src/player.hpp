#pragma once
#include "config.hpp"
#include "pico-game-engine/engine/entity.hpp"
#include "pico-game-engine/engine/game.hpp"
#include "pico-game-engine/engine/level.hpp"
#include "pico-game-engine/engine/draw.hpp"
#include "dynamic_map.hpp"
#include "general.hpp"
#include "loading.hpp"
#include "weapon.hpp"

class GhoulsGame;

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
    RequestTypeUpdateStats = 5,  // Request to update game stats on the server
} RequestType;

class Player : public Entity
{
public:
    Player(const char *user_name, const char *user_pass);
    ~Player();
    //
    void collision(Entity *other, Game *game) override;
    void drawCurrentView(Draw *canvas);
    bool equipWeapon(Level *level, Weapon *weapon);
    uint8_t getCurrentGameState() const noexcept { return gameState; }
    GameMainView getCurrentMainView() const { return currentMainView; }
    Weapon *getEquippedWeapon() const { return equippedWeapon; }
    GhoulsGame *getGhoulsGame() const { return ghoulsGame; }
    ToggleState getSoundToggle() const noexcept { return soundToggle; }
    ToggleState getVibrationToggle() const noexcept { return vibrationToggle; }
    void handleMenu(Draw *canvas, Game *game);
    void increaseXP(uint16_t amount);
    void processInput();
    void render(Draw *canvas, Game *game) override;
    void setGhoulsGame(GhoulsGame *game) { ghoulsGame = game; }
    void setGameState(GameState state) { gameState = state; }
    void setInputKey(int key) { lastInput = key; }
    void setLeaveGameToggle(ToggleState state) { leaveGame = state; }
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
    bool shouldLeaveGame() const noexcept { return alertTimer == 0 && leaveGame == ToggleOn; }
    void showAlert(const char *message, uint16_t ticks = 90);
    void update(Game *game) override;
    void userRequest(RequestType requestType);

private:
    LobbyMenuIndex currentLobbyMenuIndex = LobbyMenuLocal;     // current lobby menu index (must be in the GameViewLobbyMenu)
    MenuIndex currentMenuIndex = MenuIndexProfile;             // current menu index (must be in the GameViewSystemMenu)
    GameMainView currentMainView = GameViewWelcome;            // current main view of the game
    MenuSettingsIndex currentSettingsIndex = MenuSettingsMain; // current settings index (must be in the GameViewSystemMenu in the Settings tab)
    TitleIndex currentTitleIndex = TitleIndexStart;            // current title index (must be in the GameViewTitle)
    GhoulsGame *ghoulsGame = nullptr;                          // Reference to the main game instance
    GameState gameState = GameStatePlaying;                    // current game state
    int lastInput = -1;                                        // Last input key
    ToggleState leaveGame = ToggleOff;                         // leave game toggle state
    Loading *loading = nullptr;                                // loading animation instance
    LoginStatus loginStatus = LoginNotStarted;                 // Current login status
    OnlineGameState onlineGameState = OnlineStateIdle;         // online game connection state
    char onlineGameId[37] = {0};                               // UUID of the active game session
    uint16_t onlinePort = 0;                                   // WebSocket port assigned by the server
    bool pendingStatsUpdate = false;                           // deferred stats update flag
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
    char username[64] = {0};                                                // username for login/registeration requests
    char password[64] = {0};                                                // password for login/registration requests (set in constructor)
    uint8_t rainFrame = 0;                                                  // frame counter for rain effect
    RegistrationStatus registrationStatus = RegistrationNotStarted;         // Current registration status
    ToggleState showMiniMapToggle = MINIMAP_DEFAULT ? ToggleOn : ToggleOff; // show/hide on-screen mini maptoggle
    ToggleState soundToggle = ToggleOn;                                     // sound toggle state
    UserInfoStatus userInfoStatus = UserInfoNotStarted;                     // Current user info status
    ToggleState vibrationToggle = ToggleOn;                                 // vibration toggle state
    char alertMessage[64] = {0};                                            // current alert message text
    uint16_t alertTimer = 0;                                                // frames remaining to show the alert
    Weapon *equippedWeapon = nullptr;                                       // currently equipped weapon (nullptr if none)
    uint8_t welcomeFrame = 0;                                               // frame counter for welcome animation
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
    void updateEntitiesFromServer(const char *json);                                                   // parse server entity state and update local entity positions
};