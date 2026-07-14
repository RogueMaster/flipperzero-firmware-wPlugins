#pragma once
#include "easy_flipper/easy_flipper.h"
#include "loading/loading.hpp"
#include "run/keyboard.hpp"

#define PARSING_CHUNK_SIZE        1024
#define CARRYOVER_BUFFER_SIZE     600
#define PARSING_MAX_CHUNKS        50
#define MAX_CACHED_MESSAGES       100
#define MAX_USERNAME_LEN          24
#define MAX_TEXT_LEN              256 // i think tele allows 4096 actually lol
#define SCREEN_MAX_LINES          6
#define SCREEN_MAX_CHARS_PER_LINE 32

// Cached message structure to avoid repeated parsing
struct CachedMessage {
    char username[MAX_USERNAME_LEN];
    char text[MAX_TEXT_LEN];
};

class FlipTelegramApp;

typedef enum {
    RUN_MENU_KEYBOARD = 0, // keyboard menu
    RUN_MENU_VIEW = 1, // view message menu
    RUN_MENU_MAIN = 2, // main menu
} RunMenuView;

typedef enum {
    MessageKeyboard = 0, // Keyboard view
    MessageSending = 1, // Message is being sent
    MessageSuccess = 2, // Message sent successfully
    MessageRequestError = 3, // Error in message request
} MessageStatus;

typedef enum {
    ViewingWaiting = 0, // Waiting to fetch messages
    ViewingFetching = 1, // Fetching messages
    ViewingSuccess = 2, // Messages fetched successfully
    ViewingRequestError = 3, // Error in fetching messages
    ViewingParseError = 4, // Error in parsing messages
} ViewingStatus;

class FlipTelegramRun {
    void* appContext; // reference to the app context
    RunMenuView currentMenuIndex; // current menu index
    RunMenuView currentView; // current view index
    bool inputHeld; // flag to check if input is held
    std::unique_ptr<Keyboard> keyboard; // keyboard instance for input handling
    InputKey lastInput; // last input key pressed
    std::unique_ptr<Loading> loading; // loading animation instance
    MessageStatus messageStatus; // current message status
    bool shouldDebounce; // flag to indicate if input should be debounced
    bool shouldReturnToMenu; // Flag to signal return to menu
    ViewingStatus viewingStatus; // current viewing status
    int messageCount; // number of messages
    int currentMessageIndex; // current message index for scrolling
    int currentScrollLine; // current scroll line position in feed
    CachedMessage cachedMessages[MAX_CACHED_MESSAGES]; // cached parsed messages
    //
    void drawMainMenuView(Canvas* canvas); // draw main menu view
    void drawMessageView(Canvas* canvas); // draw message view
    void drawFeed(Canvas* canvas); // draw feed view
    void drawMenu(
        Canvas* canvas,
        uint8_t selectedIndex,
        const char** menuItems,
        uint8_t menuCount,
        const char* title = "FlipTelegram");
    void drawViewingView(Canvas* canvas); // draw viewing view

public:
    FlipTelegramRun(void* appContext);
    ~FlipTelegramRun();
    //
    bool isActive() const {
        return shouldReturnToMenu == false;
    } // Check if the run is active
    bool fetchTelegramMessages(); // fetch telegram messages
    void debounceInput(); // debounce input to prevent multiple triggers
    bool httpRequestIsFinished(); // check if the HTTP request is finished
    bool sendTelegramMessage(const char* text); // send telegram message
    void updateDraw(Canvas* canvas); // update and draw the run
    void updateInput(InputEvent* event); // update input for the run
};
