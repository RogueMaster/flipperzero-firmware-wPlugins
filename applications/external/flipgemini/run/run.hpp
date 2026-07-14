#pragma once
#include "easy_flipper/easy_flipper.h"
#include "loading/loading.hpp"
#include "keyboard/keyboard.hpp"

#define MAX_LINES_PER_SCREEN 6 // 10 height per line
#define MAX_RESPONSE_SIZE    1024 * 4
#define CHUNK_SIZE           256
#define CHARS_PER_LINE       32

typedef enum {
    GeminiRunViewMenu = -1, // main menu view
    GeminiRunViewChat = 0, // chat view
    GeminiRunViewPrompt = 1, // prompt view
} GeminiRunView;

typedef enum {
    ChatViewing = 1, // viewing chat messages
    ChatParseError = 2, // Error parsing chat data
} ChatStatus;

typedef enum {
    PromptWaiting = 0, // while message is sending, we're waiting
    PromptSuccess = 1, // Prompt sent successfully
    PromptParseError = 2, // Error parsing prompt data
    PromptRequestError = 3, // Error in prompt request
    PromptKeyboard = 4, // Keyboard for prompt view (to create a new pre-saved prompt only)
} PromptStatus;

class FlipGeminiApp;

class FlipGeminiRun {
    void* appContext; // reference to the app context
    uint8_t chatIndex; // current chat index (scrolling)
    ChatStatus chatStatus; // current chat status
    uint8_t currentCount; // current count of menu items
    GeminiRunView currentMenuIndex; // current menu index
    GeminiRunView currentView; // current view of the social run
    InputKey lastInput; // last input key pressed
    std::unique_ptr<Keyboard> keyboard; // keyboard instance for input handling
    std::unique_ptr<Loading> loading; // loading animation instance
    uint8_t maxChatLines; // maximum loaded chat lines
    PromptStatus promptStatus; // current prompt status
    bool shouldReturnToMenu; // Flag to signal return to menu
    uint32_t chatScrollOffset; // scroll offset for chat view (in lines)
    uint32_t totalChatLines; // total number of lines in the chat log
    bool chatLinesCounted; // flag to track if we've counted lines
        //
    void drawMainMenuView(Canvas* canvas); // draw the main menu view
    void drawChatView(Canvas* canvas); // draw the chat view
    void countChatLines(); // count total lines in chat log
    void drawPromptView(Canvas* canvas); // draw the prompt view
    bool httpRequestIsFinished(); // check if the HTTP request is finished
    void loadKeyboardSuggestions(); // load suggestions into the keyboard autocomplete
    bool sendPrompt(const char* prompt); // send the prompt to the API
public:
    FlipGeminiRun(void* appContext);
    ~FlipGeminiRun();
    //
    void drawMenu(
        Canvas* canvas,
        uint8_t selectedIndex,
        const char** menuItems,
        uint8_t menuCount); // Generic menu drawer
    bool isActive() const {
        return shouldReturnToMenu == false;
    } // Check if the run is active
    void updateDraw(Canvas* canvas); // update and draw the run
    void updateInput(InputEvent* event); // update input for the run
};
