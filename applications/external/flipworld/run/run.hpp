#pragma once
#include "easy_flipper/easy_flipper.h"
#include "engine/engine.hpp"
#include "run/general.hpp"
#include "run/player.hpp"

class FlipWorldApp;

// Structure to hold queued websocket messages
struct QueuedMessage {
    char* message;
    size_t messageLen;
};

class FlipWorldRun {
private:
    static const size_t MAX_QUEUED_MESSAGES = 35; // Maximum number of queued websocket messages
    //
    std::unique_ptr<Draw> draw; // Draw instance
    std::unique_ptr<GameEngine> engine; // Engine instance
    bool isGameRunning = false; // Flag to check if the game is running
    InputKey lastInput = InputKeyMAX; // Last input key pressed
    QueuedMessage messageQueue[MAX_QUEUED_MESSAGES]; // Queue for websocket messages
    std::unique_ptr<Player> player; // Player instance
    size_t queueHead = 0; // Head of the message queue
    size_t queueTail = 0; // Tail of the message queue
    size_t queueSize = 0; // Current size of the message queue
    bool shouldReturnToMenu = false; // Flag to signal return to menu
    //
    int atoi(const char* nptr) {
        return (int)strtol(nptr, NULL, 10);
    } // convert string to integer
    void handleIncomingMultiplayerData(
        const char* message); // Handle incoming websocket messages (PvE mode only)
    void inputManager(); // manage input for the game, called from updateInput
    void processCompleteMultiplayerMessage(
        const char* message); // Process a complete multiplayer message
    void processWebsocketQueue(FlipWorldApp* app); // Process queued websocket messages
    bool queueWebsocketMessage(const char* message); // Queue a websocket message for sending
    bool safeWebsocketSend(FlipWorldApp* app, const char* message); // Send websocket message
    static void
        pveRender(Entity* entity, Draw* canvas, Game* game, void* ctx); // Callback for PvE entity
public:
    FlipWorldRun();
    ~FlipWorldRun();
    //
    void* appContext =
        nullptr; // Pointer to the application context for accessing app-specific functionality
    IconGroupContext* currentIconGroup = nullptr; // Pointer to the current icon group context
    bool isLobbyHost = false; // Flag to determine if this player controls the game state
    bool isPvEMode = false; // Flag to determine if we're in PvE (multiplayer) mode
    bool shouldDebounce = false; // public for Player access
    //
    bool addRemotePlayer(
        const char* username); // Add a remote player to the current level (PvE mode only)
    void endGame(); // end the game and return to the submenu
    const char* entityToJson(Entity* entity, bool websocketParsing = false)
        const; // Convert entity properties to JSON string
    InputKey getCurrentInput() const {
        return lastInput;
    } // Get the last input key pressed
    const char* getLevelJson(LevelIndex index) const; // Get the JSON data for a level by index
    GameEngine* getEngine() const {
        return engine.get();
    } // Get the game engine instance
    Draw* getDraw() const {
        return draw.get();
    } // Get the Draw instance
    LevelIndex getCurrentLevelIndex() const; // Get the current level index
    IconSpec getIconSpec(const char* name) const; // Get the icon specification by name
    std::unique_ptr<Level>
        getLevel(LevelIndex index, Game* game = nullptr); // Get a level by index
    const char* getLevelName(LevelIndex index) const; // Get the name of a level by index
    size_t getMemoryUsage() const; // Get current memory usage in bytes
    bool isActive() const {
        return shouldReturnToMenu == false;
    } // Check if the game is active
    bool isHost() const {
        return isLobbyHost;
    } // Check if this player is the lobby host
    bool isInPvEMode() const {
        return isPvEMode;
    } // Check if in PvE mode
    bool isRunning() const {
        return isGameRunning;
    } // Check if the game engine is running
    bool parseEntityDataFromJson(
        Entity* entity,
        const char* jsonData); // Parse entity data directly from JSON string
    void processMultiplayerUpdate(); // Process multiplayer updates each frame (PvE mode only)
    bool removeRemotePlayer(
        const char* username); // Remove a remote player from the current level (PvE mode only)
    void resetInput() {
        lastInput = InputKeyMAX;
    } // Reset input after processing
    bool setAppContext(
        void* context); // Set the application context for accessing app-specific functionality
    bool setIconGroup(LevelIndex index); // Set the current icon group based on the level index
    void setIsLobbyHost(bool isHost) {
        isLobbyHost = isHost;
    } // Set if this player is the lobby host
    void setLobbyHost(bool isHost) {
        isLobbyHost = isHost;
    } // Set if this player is the lobby host
    void setPvEMode(bool enabled) {
        isPvEMode = enabled;
    } // Enable/disable PvE mode
    bool shouldProcessEnemyAI()
        const; // Check if enemy AI should be processed (host only in PvE mode)
    bool shouldUpdateEntity(Entity* entity) const; // Check if a specific entity should be updated
    bool startGame(); // start the actual game
    void syncMultiplayerEntity(Entity* entity); // Send entity state updates (PvE mode only)
    void syncMultiplayerLevel(); // Send level state updates (PvE mode only)
    void updateDraw(Canvas* canvas); // update and draw the run
    void updateInput(InputEvent* event); // update input for the run
};
