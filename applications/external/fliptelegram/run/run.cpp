#include "run/run.hpp"
#include "app.hpp"

FlipTelegramRun::FlipTelegramRun(void* appContext)
    : appContext(appContext)
    , currentMenuIndex(RUN_MENU_KEYBOARD)
    , currentView(RUN_MENU_MAIN)
    , inputHeld(false)
    , lastInput(InputKeyMAX)
    , messageStatus(MessageKeyboard)
    , shouldDebounce(false)
    , shouldReturnToMenu(false)
    , viewingStatus(ViewingWaiting)
    , messageCount(0)
    , currentMessageIndex(0)
    , currentScrollLine(0) {
}

FlipTelegramRun::~FlipTelegramRun() {
    // nothing to do
}

void FlipTelegramRun::debounceInput() {
    static uint8_t debounceCounter = 0;
    if(shouldDebounce) {
        lastInput = InputKeyMAX;
        debounceCounter++;
        if(debounceCounter < 2) {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

void FlipTelegramRun::drawFeed(Canvas* canvas) {
    if(messageCount > 0) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontSecondary);

        // Calculate total lines needed for all messages
        int currentLine = 0;
        int displayLine = 0;
        const int START_Y = 10;
        const int LINE_HEIGHT = 10;

        // Loop through cached messages
        for(int msgIdx = 0; msgIdx < messageCount && msgIdx < MAX_CACHED_MESSAGES; msgIdx++) {
            const char* username = cachedMessages[msgIdx].username;
            const char* text = cachedMessages[msgIdx].text;

            if(username[0] != '\0' && text[0] != '\0') {
                // Username line
                if(currentLine >= currentScrollLine && displayLine < SCREEN_MAX_LINES) {
                    char userLine[32];
                    snprintf(userLine, sizeof(userLine), "@%s:", username);
                    canvas_draw_str(canvas, 2, START_Y + (displayLine * LINE_HEIGHT), userLine);
                    displayLine++;
                }
                currentLine++;

                // Text lines (word wrap)
                int textLen = strlen(text);
                int textPos = 0;
                while(textPos < textLen && displayLine <= SCREEN_MAX_LINES) {
                    char line[SCREEN_MAX_CHARS_PER_LINE + 1];
                    int lineLen = 0;
                    int lastSpace = -1;

                    // Build line with word wrap
                    while(lineLen < SCREEN_MAX_CHARS_PER_LINE && textPos < textLen) {
                        if(text[textPos] == ' ') {
                            lastSpace = lineLen;
                        }
                        line[lineLen++] = text[textPos++];
                    }

                    // If we didn't reach end and last char isn't space, backtrack to last space
                    if(textPos < textLen && lastSpace > 0) {
                        textPos -= (lineLen - lastSpace);
                        lineLen = lastSpace;
                    }

                    line[lineLen] = '\0';

                    // Draw if within visible range
                    if(currentLine >= currentScrollLine && displayLine < SCREEN_MAX_LINES) {
                        canvas_draw_str(canvas, 6, START_Y + (displayLine * LINE_HEIGHT), line);
                        displayLine++;
                    }
                    currentLine++;
                }

                // Blank line between messages
                currentLine++;
            }
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 10, "No messages found!");
    }
}

void FlipTelegramRun::drawMainMenuView(Canvas* canvas) {
    const char* menuItems[] = {"Send", "View"};
    drawMenu(canvas, (uint8_t)currentMenuIndex, menuItems, 2);
}

void FlipTelegramRun::drawMenu(
    Canvas* canvas,
    uint8_t selectedIndex,
    const char** menuItems,
    uint8_t menuCount,
    const char* title) {
    canvas_clear(canvas);

    // Draw title
    canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
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
            current_width = canvas_string_width(canvas, lines[line_count]);

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
        int line_width = canvas_string_width(canvas, lines[i]);
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
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        char position_text[16];
        snprintf(position_text, sizeof(position_text), "%d/%d", selectedIndex + 1, menuCount);
        int pos_width = canvas_string_width(canvas, position_text);
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
}

void FlipTelegramRun::drawMessageView(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch(messageStatus) {
    case MessageSuccess:
        canvas_draw_str(canvas, 0, 10, "Message sent!");
        break;
    case MessageRequestError:
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 10, "Message request failed!");
        canvas_draw_str(canvas, 0, 50, "Check your network, token,");
        canvas_draw_str(canvas, 0, 60, "Chat Id, and try again later!");
        break;
    case MessageKeyboard:
        if(!keyboard) {
            keyboard = std::make_unique<Keyboard>();
        }
        if(keyboard) {
            keyboard->draw(canvas, "Enter message:");
        }
        break;
    case MessageSending:
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
            FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
            if(app->getHttpState() == ISSUE) {
                messageStatus = MessageRequestError;
                return;
            }
            char* response = (char*)malloc(16); // load the first 16 bytes of the response
            if(response && app->loadChar("message", response, 16) &&
               strstr(response, "\"ok\":true") != NULL) {
                messageStatus = MessageSuccess;
            } else {
                FURI_LOG_E(TAG, "Failed to send message or invalid response");
                FURI_LOG_E(TAG, "Response: %s", response ? response : "NULL");
                messageStatus = MessageRequestError;
            }
            if(response) {
                free(response);
            }
        }
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Sending message...");
        break;
    }
}

void FlipTelegramRun::drawViewingView(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch(viewingStatus) {
    case ViewingSuccess:
        drawFeed(canvas);
        break;
    case ViewingRequestError:
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 10, "Message fetch failed!");
        canvas_draw_str(canvas, 0, 50, "Check your network, token,");
        canvas_draw_str(canvas, 0, 60, "and try again later!");
        break;
    case ViewingWaiting:
        if(!loadingStarted) {
            if(!fetchTelegramMessages()) {
                viewingStatus = ViewingRequestError;
                return;
            }
            if(!loading) {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if(loading) {
                loading->setText("Fetching...");
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
            FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
            if(app->getHttpState() == ISSUE) {
                viewingStatus = ViewingRequestError;
                return;
            }
            char* response = (char*)malloc(16); // load the first 16 bytes of the response
            if(!response || !app->loadChar("messages", response, 16) ||
               strstr(response, "\"ok\":true") == NULL) {
                FURI_LOG_E(TAG, "Failed to fetch messages or invalid response");
                FURI_LOG_E(TAG, "Response: %s", response ? response : "NULL");
                if(response) {
                    free(response);
                }
                viewingStatus = ViewingRequestError;
                return;
            }
            if(response) {
                free(response);
            }

            // Clear cached messages first
            messageCount = 0;
            for(int i = 0; i < MAX_CACHED_MESSAGES; i++) {
                cachedMessages[i].username[0] = '\0';
                cachedMessages[i].text[0] = '\0';
            }

            // Allocate buffer for chunked reading (chunk + carry-over space)
            char* chunk = (char*)malloc(PARSING_CHUNK_SIZE + CARRYOVER_BUFFER_SIZE + 1);
            if(!chunk) {
                FURI_LOG_E(TAG, "Failed to allocate chunk buffer");
                viewingStatus = ViewingParseError;
                return;
            }

            // Allocate carry-over buffer for incomplete data at chunk boundaries
            char* carryover = (char*)malloc(CARRYOVER_BUFFER_SIZE + 1);
            if(!carryover) {
                FURI_LOG_E(TAG, "Failed to allocate carryover buffer");
                free(chunk);
                viewingStatus = ViewingParseError;
                return;
            }
            carryover[0] = '\0';
            size_t carryoverLen = 0;

            messageCount = 0;

            // Process file chunk by chunk
            for(uint8_t iteration = 0;
                iteration < PARSING_MAX_CHUNKS && messageCount < MAX_CACHED_MESSAGES;
                iteration++) {
                // Start with carryover from previous chunk
                if(carryoverLen > 0) {
                    memcpy(chunk, carryover, carryoverLen);
                }

                // Read new chunk data after the carryover
                char* tempChunk = (char*)malloc(PARSING_CHUNK_SIZE + 1);
                if(!tempChunk) {
                    FURI_LOG_E(TAG, "Failed to allocate temp chunk buffer");
                    free(chunk);
                    free(carryover);
                    viewingStatus = ViewingParseError;
                    return;
                }

                bool chunkLoadSuccess = app->loadFileChunk(
                    STORAGE_EXT_PATH_PREFIX "/apps_data/flip_telegram/data/messages.txt",
                    tempChunk,
                    PARSING_CHUNK_SIZE,
                    iteration);

                if(!chunkLoadSuccess && iteration == 0) {
                    FURI_LOG_E(TAG, "Failed to load first chunk");
                    free(chunk);
                    free(carryover);
                    free(tempChunk);
                    viewingStatus = ViewingRequestError;
                    return;
                }

                size_t tempLen = strlen(tempChunk);

                // If no data was loaded and no carryover, we're done
                if(tempLen == 0 && carryoverLen == 0) {
                    free(tempChunk);
                    break;
                }

                // Append new chunk data to carryover
                memcpy(chunk + carryoverLen, tempChunk, tempLen);
                chunk[carryoverLen + tempLen] = '\0';
                size_t chunkLen = carryoverLen + tempLen;

                free(tempChunk);

                // Reset carryover for this iteration
                carryoverLen = 0;
                carryover[0] = '\0';

                // Look for username and text patterns in this chunk
                char* pos = chunk;
                char* lastProcessed = chunk;
                char* lastIncompleteUsername = NULL;

                int messagesFoundThisChunk = 0;

                while(pos < chunk + chunkLen && messageCount < MAX_CACHED_MESSAGES) {
                    // Try to find "username" field first, then fall back to "title"
                    char* username_key = strstr(pos, "\"username\":\"");
                    char* title_key = NULL;
                    char* name_start = NULL;
                    char* name_end = NULL;

                    if(username_key) {
                        // Extract username
                        name_start = username_key + 12; // Skip "username":"
                    } else {
                        // No username found, try title
                        title_key = strstr(pos, "\"title\":\"");
                        if(!title_key) {
                            break;
                        }
                        name_start = title_key + 9; // Skip "title":"
                    }

                    name_end = strchr(name_start, '"');
                    if(!name_end || name_end >= chunk + chunkLen) {
                        // Name might be incomplete at chunk boundary
                        lastIncompleteUsername = username_key ? username_key : title_key;
                        break;
                    }

                    if(name_end - name_start >= MAX_USERNAME_LEN) {
                        pos = (username_key ? username_key : title_key) + 1;
                        continue;
                    }

                    // Look for "text" field after username/title
                    char* search_start = name_end;
                    char* search_end = name_end + 500; // Search next 500 chars for text
                    if(search_end > chunk + chunkLen) search_end = chunk + chunkLen;

                    char* text_key = strstr(search_start, "\"text\":\"");
                    if(!text_key || text_key >= search_end) {
                        // Text field might be in next chunk, save from username/title start
                        lastIncompleteUsername = username_key ? username_key : title_key;
                        break;
                    }

                    // Extract text
                    char* text_start = text_key + 8; // Skip "text":"
                    char* text_end = strchr(text_start, '"');
                    if(!text_end || text_end >= chunk + chunkLen) {
                        // Text might be incomplete at chunk boundary
                        lastIncompleteUsername = username_key ? username_key : title_key;
                        break;
                    }

                    if(text_end - text_start >= MAX_TEXT_LEN) {
                        pos = (username_key ? username_key : title_key) + 1;
                        continue;
                    }

                    // Copy username/title and text to cached message
                    size_t name_len = name_end - name_start;
                    size_t text_len = text_end - text_start;
                    snprintf(
                        cachedMessages[messageCount].username,
                        MAX_USERNAME_LEN,
                        "%.*s",
                        (int)name_len,
                        name_start);
                    snprintf(
                        cachedMessages[messageCount].text,
                        MAX_TEXT_LEN,
                        "%.*s",
                        (int)text_len,
                        text_start);

                    messagesFoundThisChunk++;
                    messageCount++;
                    pos = text_end + 1;
                    lastProcessed = pos;
                }

                // Save unprocessed tail to carryover buffer if there's an incomplete message
                // or if we haven't processed everything in this chunk
                if(lastProcessed < chunk + chunkLen) {
                    // If we found an incomplete message, carry over from there
                    char* carryStartPoint = lastIncompleteUsername ? lastIncompleteUsername :
                                                                     lastProcessed;
                    size_t remainingLen = (chunk + chunkLen) - carryStartPoint;

                    // Only carry over if there's data and it fits in carryover buffer
                    if(remainingLen > 0 && remainingLen < CARRYOVER_BUFFER_SIZE) {
                        memcpy(carryover, carryStartPoint, remainingLen);
                        carryover[remainingLen] = '\0';
                        carryoverLen = remainingLen;
                    } else if(remainingLen >= CARRYOVER_BUFFER_SIZE) {
                        // If remaining data is too large, just carry the max we can
                        memcpy(carryover, carryStartPoint, CARRYOVER_BUFFER_SIZE - 1);
                        carryover[CARRYOVER_BUFFER_SIZE - 1] = '\0';
                        carryoverLen = CARRYOVER_BUFFER_SIZE - 1;
                    }
                }

                // If we reached EOF, break after processing this chunk
                if(!chunkLoadSuccess) {
                    break;
                }
            }

            // Process any remaining data in carryover buffer (last message in file)
            if(carryoverLen > 0 && messageCount < MAX_CACHED_MESSAGES) {
                char* pos = carryover;
                while(pos < carryover + carryoverLen && messageCount < MAX_CACHED_MESSAGES) {
                    // Try to find "username" field first, then fall back to "title"
                    char* username_key = strstr(pos, "\"username\":\"");
                    char* title_key = NULL;
                    char* name_start = NULL;
                    char* name_end = NULL;

                    if(username_key) {
                        // Extract username
                        name_start = username_key + 12; // Skip "username":"
                    } else {
                        // No username found, try title
                        title_key = strstr(pos, "\"title\":\"");
                        if(!title_key) {
                            break;
                        }
                        name_start = title_key + 9; // Skip "title":"
                    }

                    name_end = strchr(name_start, '"');
                    if(!name_end) {
                        FURI_LOG_W(TAG, "Name not terminated in carryover");
                        break;
                    }

                    if(name_end - name_start >= MAX_USERNAME_LEN) {
                        pos = (username_key ? username_key : title_key) + 1;
                        continue;
                    }

                    // Look for "text" field after username/title
                    char* text_key = strstr(name_end, "\"text\":\"");
                    if(!text_key) {
                        FURI_LOG_W(TAG, "No text field found after name in carryover");
                        pos = (username_key ? username_key : title_key) + 1;
                        continue;
                    }

                    // Extract text
                    char* text_start = text_key + 8;
                    char* text_end = strchr(text_start, '"');
                    if(!text_end) {
                        FURI_LOG_W(TAG, "Text not terminated in carryover");
                        break;
                    }

                    if(text_end - text_start >= MAX_TEXT_LEN) {
                        pos = (username_key ? username_key : title_key) + 1;
                        continue;
                    }

                    // Copy username/title and text to cached message
                    size_t name_len = name_end - name_start;
                    size_t text_len = text_end - text_start;
                    snprintf(
                        cachedMessages[messageCount].username,
                        MAX_USERNAME_LEN,
                        "%.*s",
                        (int)name_len,
                        name_start);
                    snprintf(
                        cachedMessages[messageCount].text,
                        MAX_TEXT_LEN,
                        "%.*s",
                        (int)text_len,
                        text_start);

                    messageCount++;
                    pos = text_end + 1;
                }
            }

            free(chunk);
            free(carryover);
            chunk = NULL;
            carryover = NULL;

            viewingStatus = ViewingSuccess;
            currentScrollLine = 0;
        }
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Fetching messages...");
        break;
    }
}

bool FlipTelegramRun::fetchTelegramMessages() {
    FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
    if(!app) {
        FURI_LOG_E(TAG, "fetchTelegramMessages: App context is NULL");
        return false;
    }
    char token[64];
    if(!app->loadChar("token", token, sizeof(token))) {
        FURI_LOG_E(TAG, "Bot token not set");
        return false;
    }
    char* requestUrl = (char*)malloc(128);
    if(!requestUrl) {
        FURI_LOG_E(TAG, "Failed to allocate memory for request URL");
        return false;
    }
    snprintf(requestUrl, 128, "%s/bot%s/getUpdates", "https://api.telegram.org", token);
    bool success = app->httpRequestAsync("messages.txt", requestUrl, BYTES);
    free(requestUrl);
    return success;
}

bool FlipTelegramRun::httpRequestIsFinished() {
    FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
    if(!app) {
        FURI_LOG_E(TAG, "httpRequestIsFinished: App context is NULL");
        return true;
    }
    auto state = app->getHttpState();
    return state == IDLE || state == ISSUE || state == INACTIVE;
}

bool FlipTelegramRun::sendTelegramMessage(const char* text) {
    FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
    if(!app) {
        FURI_LOG_E(TAG, "httpRequestIsFinished: App context is NULL");
        return false;
    }
    char token[64];
    char chat[64];
    if(!app->loadChar("token", token, sizeof(token))) {
        FURI_LOG_E(TAG, "Bot token not set");
        return false;
    }
    if(!app->loadChar("chat_id", chat, sizeof(chat))) {
        FURI_LOG_E(TAG, "Chat ID not set");
        return false;
    }
    char* requestUrl = (char*)malloc(128);
    if(!requestUrl) {
        FURI_LOG_E(TAG, "Failed to allocate memory for request URL");
        return false;
    }
    char* payload = (char*)malloc(256);
    if(!payload) {
        FURI_LOG_E(TAG, "Failed to allocate memory for payload");
        free(requestUrl);
        return false;
    }
    snprintf(payload, 256, "{\"chat_id\":\"%s\",\"text\":\"%s\"}", chat, text);
    snprintf(requestUrl, 128, "%s/bot%s/sendMessage", "https://api.telegram.org", token);
    bool success = app->httpRequestAsync(
        "message.txt", requestUrl, POST, "{\"Content-Type\":\"application/json\"}", payload);
    free(requestUrl);
    free(payload);
    return success;
}

void FlipTelegramRun::updateDraw(Canvas* canvas) {
    canvas_clear(canvas);

    switch(currentView) {
    case RUN_MENU_MAIN:
        drawMainMenuView(canvas);
        break;
    case RUN_MENU_KEYBOARD:
        drawMessageView(canvas);
        break;
    case RUN_MENU_VIEW:
        drawViewingView(canvas);
        break;
    default:
        FURI_LOG_E(TAG, "updateDraw: Unknown view %d", currentView);
        break;
    }
}

void FlipTelegramRun::updateInput(InputEvent* event) {
    lastInput = event->key;
    debounceInput();
    switch(currentView) {
    case RUN_MENU_MAIN: {
        switch(lastInput) {
        case InputKeyDown:
        case InputKeyRight:
            shouldDebounce = true;
            currentMenuIndex = RUN_MENU_VIEW;
            break;
        case InputKeyUp:
        case InputKeyLeft:
            shouldDebounce = true;
            currentMenuIndex = RUN_MENU_KEYBOARD;
            break;
        case InputKeyOk:
            shouldDebounce = true;
            if(currentMenuIndex == RUN_MENU_KEYBOARD) {
                currentView = RUN_MENU_KEYBOARD;
            } else if(currentMenuIndex == RUN_MENU_VIEW) {
                currentView = RUN_MENU_VIEW;
            }
            break;
        case InputKeyBack:
            shouldDebounce = true;
            shouldReturnToMenu = true;
            break;
        default:
            break;
        };
        break;
    }
    case RUN_MENU_KEYBOARD:
        if(messageStatus == MessageKeyboard) {
            if(keyboard) {
                if(keyboard->handleInput(lastInput)) {
                    messageStatus = MessageSending;
                    sendTelegramMessage(keyboard->getText());
                }
                if(lastInput != InputKeyMAX) {
                    shouldDebounce = true;
                }
            }
        }
        if(lastInput == InputKeyBack) {
            messageStatus = MessageKeyboard;
            shouldDebounce = true;
            if(keyboard) {
                keyboard->clearText();
                keyboard.reset();
            }
            currentView = RUN_MENU_MAIN;
        }
        break;
    case RUN_MENU_VIEW:
        if(viewingStatus == ViewingSuccess && messageCount > 0) {
            switch(lastInput) {
            case InputKeyDown:
                shouldDebounce = true;
                currentScrollLine++;
                break;
            case InputKeyUp:
                shouldDebounce = true;
                if(currentScrollLine > 0) {
                    currentScrollLine--;
                }
                break;
            case InputKeyBack:
                shouldDebounce = true;
                currentView = RUN_MENU_MAIN;
                viewingStatus = ViewingWaiting;
                currentScrollLine = 0;
                break;
            default:
                break;
            }
        } else if(lastInput == InputKeyBack) {
            currentView = RUN_MENU_MAIN;
            currentScrollLine = 0;
            shouldDebounce = true;
        }
        break;
    default:
        break;
    }
}
