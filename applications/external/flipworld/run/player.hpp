#pragma once
#include "engine/draw.hpp"
#include "engine/entity.hpp"
#include "engine/game.hpp"
#include "engine/level.hpp"
#include "flipper_http/flipper_http.h"
#include <furi.h>
#include "run/assets.hpp"
#include "run/general.hpp"
#include "run/loading.hpp"

typedef enum {
    LoginCredentialsMissing = -1, // Credentials missing
    LoginSuccess = 0, // Login successful
    LoginUserNotFound = 1, // User not found
    LoginWrongPassword = 2, // Wrong password
    LoginWaiting = 3, // Waiting for response
    LoginNotStarted = 4, // Login not started
    LoginRequestError = 5, // Request error
} LoginStatus;

typedef enum {
    RegistrationCredentialsMissing = -1, // Credentials missing
    RegistrationSuccess = 0, // Registration successful
    RegistrationUserExists = 1, // User already exists
    RegistrationRequestError = 2, // Request error
    RegistrationNotStarted = 3, // Registration not started
    RegistrationWaiting = 4, // Waiting for response
} RegistrationStatus;

typedef enum {
    UserInfoCredentialsMissing = -1, // Credentials missing
    UserInfoSuccess = 0, // User info fetched successfully
    UserInfoRequestError = 1, // Request error
    UserInfoNotStarted = 2, // User info request not started
    UserInfoWaiting = 3, // Waiting for response
    UserInfoParseError = 4, // Error parsing user info
} UserInfoStatus;

typedef enum {
    LobbiesCredentialsMissing = -1, // Credentials missing
    LobbiesSuccess = 0, // Lobbies fetched successfully
    LobbiesRequestError = 1, // Request error
    LobbiesNotStarted = 2, // Lobbies request not started
    LobbiesWaiting = 3, // Waiting for response
    LobbiesParseError = 4, // Error parsing lobbies
} LobbiesStatus;

typedef enum {
    JoinLobbyCredentialsMissing = -1, // Credentials missing
    JoinLobbySuccess = 0, // Successfully joined lobby
    JoinLobbyRequestError = 1, // Request error
    JoinLobbyNotStarted = 2, // Join lobby request not started
    JoinLobbyWaiting = 3, // Waiting for response
    JoinLobbyParseError = 4, // Error parsing join lobby response
} JoinLobbyStatus;

// Structure to store lobby information
struct LobbyInfo {
    char id[32];
    char name[64];
    int playerCount;
    int maxPlayers;

    LobbyInfo()
        : playerCount(0)
        , maxPlayers(0) {
        id[0] = '\0';
        name[0] = '\0';
    }
};

typedef enum {
    RequestTypeLogin = 0, // Request login (login the user)
    RequestTypeRegistration = 1, // Request registration (register the user)
    RequestTypeUserInfo = 2, // Request user infon (fetch user info)
    RequestTypeLobbies = 3, // Request lobbies (fetch lobbies/servers available)
    RequestTypeJoinLobby = 4, // Request to join a lobby (join a specific lobby)
    RequestTypeStartWebsocket =
        5, // Request to start a websocket connection (for real-time updates)
    RequestTypeStopWebsocket = 6, // Request to stop the websocket connection
    RequestTypeSaveStats =
        7, // Request to save player stats (save the player's stats to the server)
} RequestType;

class FlipWorldRun;

class Player : public Entity {
public:
    Player();
    ~Player();

    char player_name[64] = {0};
    //
    void drawCurrentView(Draw* canvas); // Draw the current view based on the game state
    GameState getCurrentGameState() const noexcept {
        return gameState;
    } // Get the current game state
    GameMainView getCurrentMainView() const {
        return currentMainView;
    } // Get the current main view of the game
    TitleIndex getCurrentTitleIndex() const noexcept {
        return currentTitleIndex;
    } // Get the current title index
    HTTPState getHttpState(); // Get the current HTTP state
    IconGroupContext*
        getIconGroupContext() const; // get the current icon group context from FlipWorldRun
    bool httpRequestIsFinished(); // Check if the HTTP request is finished
    void iconGroupRender(Game* game); // Render the icon group for the current level
    void processInput(); // Process input for all of the views
    void render(Draw* canvas, Game* game) override; // render callback for the player
    void setFlipWorldRun(FlipWorldRun* run) {
        flipWorldRun = run;
    } // Set the FlipWorldRun instance
    void setGameState(GameState state) {
        gameState = state;
    } // Set the current game state
    bool setHttpState(HTTPState state); // Set the HTTP state
    void setInputKey(InputKey key) {
        lastInput = key;
    } // Set the last input key pressed
    bool shouldLeaveGame() const noexcept {
        return leaveGame == ToggleOn;
    } // Check if the player wants to leave the game
    void syncMultiplayerState(); // Sync multiplayer state
    void update(Game* game) override; // update callback for the player
    void userRequest(
        RequestType requestType); // Send a user request to the server based on the request type

private:
    TitleIndex currentTitleIndex =
        TitleIndexStory; // current title index (must be in the GameViewTitle)
    int currentLobbyIndex = 0; // Current selected lobby index
    GameMainView currentMainView = GameViewTitle; // current main view of the game
    MenuIndex currentSystemMenuIndex =
        MenuIndexProfile; // current menu index (must be in the GameViewSystemMenu)
    FlipWorldRun* flipWorldRun = nullptr; // Reference to the main run instance
    GameState gameState = GameStatePlaying; // current game state
    bool hasBeenPositioned =
        false; // Track if player has been positioned to prevent repeated resets
    bool inputHeld = false; // whether input is held
    JoinLobbyStatus joinLobbyStatus = JoinLobbyNotStarted; // current join lobby status
    bool justStarted = true; // whether the player just started the game
    bool justSwitchedLevels = false; // whether the player just switched levels
    float levelCompletionCooldown = 0; // cooldown timer for level completion checks
    InputKey lastInput = InputKeyMAX; // Last input key pressed
    ToggleState leaveGame = ToggleOff; // leave game toggle state
    std::unique_ptr<Loading> loading; // loading animation instance
    LobbyInfo lobbies[4]; // Array to store lobby information (max 4 lobbies)
    LobbiesStatus lobbiesStatus = LobbiesNotStarted; // Current lobbies status
    int lobbyCount = 0; // Number of lobbies loaded
    LoginStatus loginStatus = LoginNotStarted; // Current login status
    float old_xp = 0.0f; // previous xp value for tracking changes
    uint8_t rainFrame = 0; // frame counter for rain effect
    RegistrationStatus registrationStatus = RegistrationNotStarted; // Current registration status
    UserInfoStatus userInfoStatus = UserInfoNotStarted; // Current user info status

    bool areAllEnemiesDead(Game* game); // Check if all enemies in the current level are dead
    void checkForLevelCompletion(
        Game* game); // Check if all enemies are dead and switch to next level
    void drawLobbiesView(Draw* canvas); // draw the lobbies view
    void drawLoginView(Draw* canvas); // draw the login view
    void drawJoinLobbyView(Draw* canvas); // draw the join lobby view
    void drawRainEffect(Draw* canvas); // draw the rain effect
    void drawRegistrationView(Draw* canvas); // draw the registration view
    void drawSystemMenuView(Draw* canvas); // draw the system menu view
    void drawTitleView(Draw* canvas); // draw the title view
    void drawUserInfoView(Draw* canvas); // draw the user info view
    void drawUsername(Vector pos, Game* game); // draw the username at the specified position
    void drawUserStats(Vector pos, Draw* canvas); // draw the user stats at the specified position
    void updateStats(); // update player stats
};
