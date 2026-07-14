#include "run/run.hpp"
#include "app.hpp"

FlipGeminiRun::FlipGeminiRun(void* appContext)
    : appContext(appContext)
    , chatIndex(0)
    , chatStatus(ChatViewing)
    , currentCount(0)
    , currentMenuIndex(GeminiRunViewChat)
    , currentView(GeminiRunViewMenu)
    , lastInput(InputKeyMAX)
    , maxChatLines(0)
    , promptStatus(PromptKeyboard)
    , shouldReturnToMenu(false)
    , chatScrollOffset(0)
    , totalChatLines(0)
    , chatLinesCounted(false) {
}

FlipGeminiRun::~FlipGeminiRun() {
    // nothing to do
}

void FlipGeminiRun::countChatLines() {
    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
    if(!app) {
        totalChatLines = 0;
        return;
    }

    char* buffer = (char*)malloc(CHUNK_SIZE);
    if(!buffer) {
        totalChatLines = 0;
        return;
    }

    totalChatLines = 0;
    uint8_t iteration = 0;
    size_t currentLineChars = 0;

    while(app->loadFileChunk(
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_gemini/data/logs.txt",
        buffer,
        CHUNK_SIZE - 1,
        iteration)) {
        buffer[CHUNK_SIZE - 1] = '\0';
        size_t len = strlen(buffer);

        for(size_t i = 0; i < len; i++) {
            if(buffer[i] == '\n') {
                // Newline in file = new display line
                totalChatLines++;
                currentLineChars = 0;
            } else {
                currentLineChars++;
                // Word wrap: if we've reached max chars, count as new display line
                if(currentLineChars >= CHARS_PER_LINE) {
                    totalChatLines++;
                    currentLineChars = 0;
                }
            }
        }
        iteration++;
    }

    // Count any remaining partial line
    if(currentLineChars > 0) {
        totalChatLines++;
    }

    free(buffer);
    chatLinesCounted = true;
}

void FlipGeminiRun::drawChatView(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(chatStatus == ChatParseError) {
        canvas_draw_str(canvas, 0, 10, "Error parsing chat data!");
        canvas_draw_str(canvas, 0, 20, "Please try again later.");
        return;
    }

    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
    if(!app) {
        FURI_LOG_E(TAG, "drawChatView: App context is NULL");
        return;
    }

    // Count lines only once when entering the view
    if(!chatLinesCounted) {
        countChatLines();
        // Start at the end of the conversation
        if(totalChatLines > MAX_LINES_PER_SCREEN) {
            chatScrollOffset = totalChatLines - MAX_LINES_PER_SCREEN;
        } else {
            chatScrollOffset = 0;
        }
    }

    if(totalChatLines == 0) {
        canvas_draw_str(canvas, 0, 10, "No chat history yet.");
        canvas_draw_str(canvas, 0, 20, "Send a prompt to start!");
        return;
    }

    // Ensure scroll offset is valid
    if(totalChatLines > MAX_LINES_PER_SCREEN &&
       chatScrollOffset > totalChatLines - MAX_LINES_PER_SCREEN) {
        chatScrollOffset = totalChatLines - MAX_LINES_PER_SCREEN;
    }

    char* buffer = (char*)malloc(CHUNK_SIZE);
    if(!buffer) {
        canvas_draw_str(canvas, 0, 10, "Memory error!");
        return;
    }

    // We'll collect up to 6 display lines
    char displayLines[MAX_LINES_PER_SCREEN][CHARS_PER_LINE + 1];
    for(int i = 0; i < MAX_LINES_PER_SCREEN; i++) {
        displayLines[i][0] = '\0';
    }

    uint8_t displayedLines = 0;
    uint32_t currentDisplayLine = 0;
    uint8_t iteration = 0;
    size_t currentLineChars = 0;
    char currentLineBuffer[CHARS_PER_LINE + 1] = {0};

    // Read through file and extract the lines we need
    bool continueReading = true;
    while(continueReading) {
        bool chunkLoaded = app->loadFileChunk(
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_gemini/data/logs.txt",
            buffer,
            CHUNK_SIZE - 1,
            iteration);

        if(!chunkLoaded) {
            // End of file - add any remaining content as final line
            if(currentLineChars > 0 && currentDisplayLine >= chatScrollOffset &&
               displayedLines < MAX_LINES_PER_SCREEN) {
                currentLineBuffer[currentLineChars] = '\0';
                snprintf(
                    displayLines[displayedLines], CHARS_PER_LINE + 1, "%s", currentLineBuffer);
                displayedLines++;
            }
            break;
        }

        buffer[CHUNK_SIZE - 1] = '\0';
        size_t len = strlen(buffer);

        for(size_t i = 0; i < len && displayedLines < MAX_LINES_PER_SCREEN; i++) {
            if(buffer[i] == '\n') {
                // End of line in file
                if(currentDisplayLine >= chatScrollOffset &&
                   displayedLines < MAX_LINES_PER_SCREEN) {
                    currentLineBuffer[currentLineChars] = '\0';
                    snprintf(
                        displayLines[displayedLines], CHARS_PER_LINE + 1, "%s", currentLineBuffer);
                    displayedLines++;
                }
                currentDisplayLine++;
                currentLineChars = 0;
                currentLineBuffer[0] = '\0';
            } else {
                // Add character to current line
                if(currentLineChars < CHARS_PER_LINE) {
                    currentLineBuffer[currentLineChars] = buffer[i];
                    currentLineChars++;
                }

                // Check if we need to wrap
                if(currentLineChars >= CHARS_PER_LINE) {
                    if(currentDisplayLine >= chatScrollOffset &&
                       displayedLines < MAX_LINES_PER_SCREEN) {
                        currentLineBuffer[currentLineChars] = '\0';
                        snprintf(
                            displayLines[displayedLines],
                            CHARS_PER_LINE + 1,
                            "%s",
                            currentLineBuffer);
                        displayedLines++;
                    }
                    currentDisplayLine++;
                    currentLineChars = 0;
                    currentLineBuffer[0] = '\0';
                }
            }
        }

        // Stop if we have all the lines we need
        if(displayedLines >= MAX_LINES_PER_SCREEN) {
            continueReading = false;
        }

        iteration++;

        // Safety limit
        if(iteration > 200) {
            continueReading = false;
        }
    }

    free(buffer);

    // Display the lines
    for(uint8_t i = 0; i < displayedLines; i++) {
        uint8_t y = 10 + (i * 10);
        canvas_draw_str(canvas, 0, y, displayLines[i]);
    }
}

void FlipGeminiRun::drawMainMenuView(Canvas* canvas) {
    const char* menuItems[] = {"View Chat", "Send Prompt"};
    drawMenu(canvas, (uint8_t)currentMenuIndex, menuItems, 2);
}

void FlipGeminiRun::drawMenu(
    Canvas* canvas,
    uint8_t selectedIndex,
    const char** menuItems,
    uint8_t menuCount) {
    canvas_clear(canvas);

    // Draw title
    canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
    const char* title = "FlipGemini";
    int title_width = canvas_string_width(canvas, title);
    int title_x = (128 - title_width) / 2;
    canvas_draw_str(canvas, title_x, 12, title);

    // Draw underline for title
    canvas_draw_line(canvas, title_x, 14, title_x + title_width, 14);

    // Draw decorative horizontal pattern
    for(int i = 0; i < 128; i += 4) {
        canvas_draw_dot(canvas, i, 18);
    }

    // Menu items with word wrapping
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    const char* currentItem = menuItems[selectedIndex];

    const int box_padding = 10;
    const int box_width = 108;
    const int usable_width = box_width - (box_padding * 2); // Text area inside box
    const int line_height = 8; // Typical line height for medium font
    const int max_lines = 2; // Maximum lines to prevent overflow

    int menu_y = 40;

    // Calculate word wrapping
    char lines[max_lines][64];
    int line_count = 0;

    // word wrap
    const char* text = currentItem;
    int text_len = strlen(text);
    int current_pos = 0;

    while(current_pos < text_len && line_count < max_lines) {
        int line_start = current_pos;
        int last_space = -1;
        int current_width = 0;
        int char_pos = 0;

        // Find how much text fits on this line
        while(current_pos < text_len && char_pos < 63) // Leave room for null terminator
        {
            if(text[current_pos] == ' ') {
                last_space = char_pos;
            }

            lines[line_count][char_pos] = text[current_pos];
            char_pos++;

            // Check if adding this character exceeds width
            lines[line_count][char_pos] = '\0'; // Temporary null terminator
            current_width = canvas_string_width_custom(canvas, lines[line_count]);

            if(current_width > usable_width) {
                // Text is too wide, need to break
                if(last_space > 0) {
                    // Break at last space
                    lines[line_count][last_space] = '\0';
                    current_pos = line_start + last_space + 1; // Skip the space
                } else {
                    // No space found, break at previous character
                    char_pos--;
                    lines[line_count][char_pos] = '\0';
                    current_pos = line_start + char_pos;
                }
                break;
            }

            current_pos++;
        }

        // If we reached end of text
        if(current_pos >= text_len) {
            lines[line_count][char_pos] = '\0';
        }

        line_count++;
    }

    // If there's still more text and we're at max lines, add ellipsis
    if(current_pos < text_len && line_count == max_lines) {
        int last_line = line_count - 1;
        int line_len = strlen(lines[last_line]);
        if(line_len > 3) {
            strcpy(&lines[last_line][line_len - 3], "...");
        }
    }

    // Calculate box height based on number of lines, but keep minimum height
    int box_height = (line_count * line_height) + 8;
    if(box_height < 16) box_height = 16;

    // Dynamic box positioning based on content height
    int box_y_offset;
    if(line_count > 1) {
        box_y_offset = -22;
    } else {
        box_y_offset = -12;
    }

    // Draw main selection box
    canvas_draw_rbox(canvas, 10, menu_y + box_y_offset, box_width, box_height, 4);
    canvas_set_color(canvas, ColorWhite);

    // Draw each line of text centered
    for(int i = 0; i < line_count; i++) {
        int line_width = canvas_string_width_custom(canvas, lines[i]);
        int line_x = (128 - line_width) / 2;
        int text_y_offset = (line_count > 1) ? -18 : -4;
        int line_y = menu_y + (i * line_height) + 4 + text_y_offset;
        canvas_draw_str(canvas, line_x, line_y, lines[i]);
    }

    canvas_set_color(canvas, ColorBlack);

    // Draw navigation arrows
    if(selectedIndex > 0) {
        canvas_draw_str(canvas, 2, menu_y + 4, "<");
    }
    if(selectedIndex < (menuCount - 1)) {
        canvas_draw_str(canvas, 122, menu_y + 4, ">");
    }

    const int MAX_DOTS = 15;
    const int dots_spacing = 6;
    int indicator_y = 52;

    if(menuCount <= MAX_DOTS) {
        // Show all dots if they fit
        int dots_start_x = (128 - (menuCount * dots_spacing)) / 2;
        for(int i = 0; i < menuCount; i++) {
            int dot_x = dots_start_x + (i * dots_spacing);
            if(i == selectedIndex) {
                canvas_draw_box(canvas, dot_x, indicator_y, 4, 4);
            } else {
                canvas_draw_frame(canvas, dot_x, indicator_y, 4, 4);
            }
        }
    } else {
        // condensed indicator with current position
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        char position_text[16];
        snprintf(position_text, sizeof(position_text), "%d/%d", selectedIndex + 1, menuCount);
        int pos_width = canvas_string_width_custom(canvas, position_text);
        int pos_x = (128 - pos_width) / 2;
        canvas_draw_str(canvas, pos_x, indicator_y + 3, position_text);

        // progress bar
        int bar_width = 60;
        int bar_x = (128 - bar_width) / 2;
        int bar_y = indicator_y - 6;
        canvas_draw_frame(canvas, bar_x, bar_y, bar_width, 3);
        int progress_width = (selectedIndex * (bar_width - 2)) / (menuCount - 1);
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, progress_width, 1);
    }

    // Draw decorative bottom pattern
    for(int i = 0; i < 128; i += 4) {
        canvas_draw_dot(canvas, i, 58);
    }

    currentCount = menuCount;
}

void FlipGeminiRun::drawPromptView(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch(promptStatus) {
    case PromptWaiting:
        if(!loadingStarted) {
            if(!loading) {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if(loading) {
                loading->setText("Sending...");
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
            FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
            furi_check(app);
            if(app->getHttpState() == ISSUE) {
                FURI_LOG_E(TAG, "drawPromptView: HTTP request issue");
                promptStatus = PromptRequestError;
                return;
            }
            char* response = (char*)malloc(MAX_RESPONSE_SIZE);
            if(!response) {
                FURI_LOG_E(TAG, "drawPromptView: Failed to allocate memory for response");
                promptStatus = PromptParseError;
                if(response) free(response);
                return;
            }
            if(!app->loadChar("prompt", response, MAX_RESPONSE_SIZE)) {
                FURI_LOG_E(TAG, "drawPromptView: Failed to load prompt response");
                free(response);
                promptStatus = PromptParseError;
                return;
            }
            // parse response and save to SD card/chat log so the ChatView can load it
            // Structure: candidates[0] -> content -> parts[0] -> text
            promptStatus = PromptKeyboard; // reset to keyboard for next time
            char* candidate = get_json_array_value("candidates", 0, response);
            if(candidate) {
                char* content = get_json_value("content", candidate);
                if(content) {
                    char* part = get_json_array_value("parts", 0, content);
                    if(part) {
                        char* response_text = get_json_value("text", part);
                        if(response_text) {
                            // Log Gemini response to logs.txt
                            size_t responseTextLen = strlen(response_text);
                            size_t geminiLen = strlen("Gemini: \n");
                            char* logEntry = (char*)malloc(responseTextLen + geminiLen + 1);
                            if(logEntry) {
                                snprintf(
                                    logEntry,
                                    responseTextLen + geminiLen + 1,
                                    "Gemini: %s\n",
                                    response_text);
                                app->saveChar("logs", logEntry, APP_ID, false);
                                free(logEntry);
                            }

                            free(response_text);
                        } else {
                            FURI_LOG_E(TAG, "Failed to parse 'text' field");
                            promptStatus = PromptParseError;
                        }
                        free(part);
                    } else {
                        FURI_LOG_E(TAG, "Failed to parse 'parts' array");
                        promptStatus = PromptParseError;
                    }
                    free(content);
                } else {
                    FURI_LOG_E(TAG, "Failed to parse 'content' field");
                    promptStatus = PromptParseError;
                }
                free(candidate);
            } else {
                FURI_LOG_E(TAG, "Failed to parse 'candidates' array");
                promptStatus = PromptParseError;
            }
            free(response);
            currentView = GeminiRunViewChat;
            currentMenuIndex = GeminiRunViewChat;
            chatStatus = ChatViewing;
            chatLinesCounted = false; // Recount after new message
        }
        break;
    case PromptSuccess:
        // unlike other "views", we shouldnt hit here
        // since after prompting, users will be redirected to chat view
        canvas_draw_str(canvas, 0, 10, "Sent successfully!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case PromptRequestError:
        canvas_draw_str(canvas, 0, 10, "Prompt request failed!");
        canvas_draw_str(canvas, 0, 20, "Ensure your message");
        canvas_draw_str(canvas, 0, 30, "follows the rules.");
        break;
    case PromptParseError:
        canvas_draw_str(canvas, 0, 10, "Error parsing prompt!");
        canvas_draw_str(canvas, 0, 20, "Ensure your message");
        canvas_draw_str(canvas, 0, 30, "follows the rules.");
        break;
    case PromptKeyboard:
        if(!keyboard) {
            keyboard = std::make_unique<Keyboard>();
            this->loadKeyboardSuggestions();
        }
        if(keyboard) {
            keyboard->draw(canvas, "Enter prompt:");
        }
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Awaiting...");
        break;
    }
}

bool FlipGeminiRun::httpRequestIsFinished() {
    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
    if(!app) {
        FURI_LOG_E(TAG, "httpRequestIsFinished: App context is NULL");
        return true;
    }
    auto state = app->getHttpState();
    return state == IDLE || state == ISSUE || state == INACTIVE;
}

void FlipGeminiRun::loadKeyboardSuggestions() {
    // Standard words for autocomplete
    if(keyboard) {
        keyboard->addSuggestion("flipper zero");
        keyboard->addSuggestion("how");
        keyboard->addSuggestion("how come");
        keyboard->addSuggestion("how do i");
        keyboard->addSuggestion("how does");
        keyboard->addSuggestion("what is");
        keyboard->addSuggestion("what are");
        keyboard->addSuggestion("why does");
        keyboard->addSuggestion("why is");
        keyboard->addSuggestion("why are");
        keyboard->addSuggestion("where");
        keyboard->addSuggestion("where is");
        keyboard->addSuggestion("where are the");
        keyboard->addSuggestion("when");
        keyboard->addSuggestion("when is");
        keyboard->addSuggestion("when are");
        keyboard->addSuggestion("explain");
        keyboard->addSuggestion("explain to me");
        keyboard->addSuggestion("tell me about");
        keyboard->addSuggestion("give me a");
        keyboard->addSuggestion("can you help");
        keyboard->addSuggestion("i need to know");
    }
}

void FlipGeminiRun::updateDraw(Canvas* canvas) {
    switch(currentView) {
    case GeminiRunViewMenu:
        drawMainMenuView(canvas);
        break;
    case GeminiRunViewChat:
        drawChatView(canvas);
        break;
    case GeminiRunViewPrompt:
        drawPromptView(canvas);
        break;
    default:
        drawMainMenuView(canvas);
        break;
    }
}

void FlipGeminiRun::updateInput(InputEvent* event) {
    lastInput = event->key;
    switch(currentView) {
    case GeminiRunViewMenu: {
        switch(lastInput) {
        case InputKeyBack:
            // return to menu
            shouldReturnToMenu = true;
            break;
        case InputKeyDown:
        case InputKeyLeft:
            if(currentMenuIndex == GeminiRunViewPrompt) {
                currentMenuIndex = GeminiRunViewChat;
            } else if(currentMenuIndex == GeminiRunViewChat) {
                currentMenuIndex = GeminiRunViewPrompt;
            }
            break;
        case InputKeyUp:
        case InputKeyRight:
            if(currentMenuIndex == GeminiRunViewChat) {
                currentMenuIndex = GeminiRunViewPrompt;
            } else if(currentMenuIndex == GeminiRunViewPrompt) {
                currentMenuIndex = GeminiRunViewChat;
            }
            break;
        case InputKeyOk:
            switch(currentMenuIndex) {
            case GeminiRunViewChat:
                currentView = GeminiRunViewChat;
                break;
            case GeminiRunViewPrompt:
                currentView = GeminiRunViewPrompt;
                break;
            default:
                break;
            };
            break;
        default:
            break;
        }
        break;
    }
    case GeminiRunViewChat: {
        switch(lastInput) {
        case InputKeyBack:
            currentView = GeminiRunViewMenu;
            chatIndex = 0;
            chatScrollOffset = 0; // Reset scroll on exit
            chatLinesCounted = false; // Recount on next view
            break;
        case InputKeyUp:
            if(chatScrollOffset > 0) {
                chatScrollOffset--;
            }
            break;
        case InputKeyDown:
            if(totalChatLines > MAX_LINES_PER_SCREEN &&
               chatScrollOffset < totalChatLines - MAX_LINES_PER_SCREEN) {
                chatScrollOffset++;
            }
            break;
        case InputKeyLeft:
            // Page up (scroll up by screen)
            if(chatScrollOffset >= MAX_LINES_PER_SCREEN) {
                chatScrollOffset -= MAX_LINES_PER_SCREEN;
            } else {
                chatScrollOffset = 0;
            }
            break;
        case InputKeyRight:
            // Page down (scroll down by screen)
            if(totalChatLines > MAX_LINES_PER_SCREEN) {
                chatScrollOffset += MAX_LINES_PER_SCREEN;
                if(chatScrollOffset > totalChatLines - MAX_LINES_PER_SCREEN) {
                    chatScrollOffset = totalChatLines - MAX_LINES_PER_SCREEN;
                }
            }
            break;
        default:
            break;
        };
        break;
    }
    case GeminiRunViewPrompt: {
        if(promptStatus == PromptKeyboard) {
            if(keyboard) {
                if(keyboard->handleInput(event)) {
                    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
                    app->saveChar("new_prompt", keyboard->getText(), APP_ID, true);
                    promptStatus = PromptWaiting;
                    if(!sendPrompt(keyboard->getText())) {
                        promptStatus = PromptRequestError;
                        FURI_LOG_E(TAG, "updateInput: Failed to send prompt");
                    }
                    keyboard->clearText();
                    keyboard.reset();
                    return;
                }
                if(lastInput != InputKeyMAX) {
                }
            }
            if(lastInput == InputKeyBack && event->type == InputTypeLong) {
                promptStatus = PromptKeyboard;
                currentView = GeminiRunViewMenu;

                if(keyboard) {
                    keyboard->clearText();
                    keyboard.reset();
                }
            }
        } else if(lastInput == InputKeyBack) {
            currentView = GeminiRunViewMenu;
        }
        break;
    default:
        break;
    }
    };
}

bool FlipGeminiRun::sendPrompt(const char* prompt) {
    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
    furi_check(app);

    // Allocate memory for credentials
    char* apiKey = (char*)malloc(128);
    if(!apiKey) {
        FURI_LOG_E(TAG, "sendPrompt: Failed to allocate memory for API key");
        promptStatus = PromptRequestError;
        return false;
    }

    // Load credentials from storage
    if(!app->loadChar("api_key", apiKey, 128, APP_ID)) {
        FURI_LOG_E(TAG, "Failed to load api_key");
        promptStatus = PromptRequestError;
        free(apiKey);
        return false;
    }

    char* url = (char*)malloc(256);
    char* promptPayload = (char*)malloc(512);
    if(!url || !promptPayload) {
        FURI_LOG_E(TAG, "sendPrompt: Failed to allocate memory for url or promptPayload");
        promptStatus = PromptRequestError;
        free(apiKey);
        if(url) free(url);
        if(promptPayload) free(promptPayload);
        return false;
    }
    snprintf(
        url,
        256,
        "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%s",
        apiKey);
    snprintf(
        promptPayload,
        512,
        "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}],\"generationConfig\":{\"stopSequences\":[\"Title\"],\"temperature\":1.0,\"maxOutputTokens\":512,\"thinkingConfig\":{\"thinkingBudget\":0}}}",
        prompt);

    const bool sent = app->httpRequestAsync(
        "prompt.txt", url, POST, "{\"Content-Type\":\"application/json\"}", promptPayload);
    if(sent) {
        // Log user message to logs.txt
        size_t promptLen = strlen(prompt);
        size_t userLen = strlen("User: \n");
        char* logEntry = (char*)malloc(promptLen + userLen + 1);
        if(logEntry) {
            snprintf(logEntry, promptLen + userLen + 1, "User: %s\n", prompt);
            app->saveChar("logs", logEntry, APP_ID, false);
            free(logEntry);
            chatLinesCounted = false; // Recount after new message
        } else {
            FURI_LOG_E(TAG, "sendPrompt: Failed to allocate memory for log entry");
            promptStatus = PromptRequestError;
        }
    } else {
        FURI_LOG_E(TAG, "sendPrompt: httpRequestAsync failed to send");
        promptStatus = PromptRequestError;
    }
    free(url);
    free(promptPayload);
    free(apiKey);
    return sent;
}
