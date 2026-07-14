#include "game_menu.h"

struct GameEngine {
    Gui* gui;
    NotificationApp* notifications;
    FuriPubSub* input_pubsub;
    FuriThreadId thread_id;
    GameEngineSettings settings;
    float fps;
};

bool game_menu_game_selected = false;
bool game_menu_tutorial_selected = false;
bool game_menu_quit_selected = false;
FuriApiLock game_menu_exit_lock = NULL;
FuriApiLock usernameLock = NULL;
char username[100];
void game_settings_menu_button_callback(void* game_manager, uint32_t index) {
    UNUSED(game_manager);
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    if(index == 3) {
        //TODO Back to main menu or pause menu.
        //Return from settings menu
        game_menu_tutorial_selected = false;
        game_menu_game_selected = false;
        game_menu_quit_selected = false;
        notification_message(notifications, &sequence_success);
        api_lock_unlock(game_menu_exit_lock);

        //game_menu_open(game_manager, false);
    }
    UNUSED(index);
}

bool game_menu_text_input_validator(const char* text, FuriString* error, void* game_context) {
    UNUSED(text);
    UNUSED(error);
    UNUSED(game_context);

    return true;
}

void game_menu_text_input_callback(void* game_context) {
    UNUSED(game_context);
    api_lock_unlock(usernameLock);
    FURI_LOG_I("DEADZONE", "Your username: %s", username);
}

void game_menu_button_callback(void* game_manager, uint32_t index) {
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    UNUSED(game_manager);
    if(index == 0) {
        game_menu_tutorial_selected = false;
        game_menu_quit_selected = false;
        game_menu_game_selected = true;
        showBackground = true;
        notification_message(notifications, &sequence_success);
        api_lock_unlock(game_menu_exit_lock);
    } else if(index == 1) {
        game_menu_quit_selected = false;
        game_menu_game_selected = false;
        game_menu_tutorial_selected = true;
        showBackground = false;
        notification_message(notifications, &sequence_success);
        api_lock_unlock(game_menu_exit_lock);
    } else if(index == 2) {
        //Quit
        game_menu_tutorial_selected = false;
        game_menu_game_selected = false;
        game_menu_quit_selected = true;
        notification_message(notifications, &sequence_single_vibro);
        api_lock_unlock(game_menu_exit_lock);
    } else if(index == 3) {
        if(!game_menu_game_selected) {
            kills = 0;
        }
        game_menu_tutorial_selected = false;
        game_menu_quit_selected = false;
        game_menu_game_selected = true;
        showBackground = true;
        //TODO value for play game.
        notification_message(notifications, &sequence_success);
        api_lock_unlock(game_menu_exit_lock);
    } else if(index == 4) {
        if(!game_menu_tutorial_selected) {
            kills = 0;
        }
        game_menu_quit_selected = false;
        game_menu_game_selected = false;
        game_menu_tutorial_selected = true;
        showBackground = false;
        notification_message(notifications, &sequence_success);
        api_lock_unlock(game_menu_exit_lock);
    }
}

// Returns true if the username already existed
bool check_username(ViewHolder* view_holder, GameManager* game_manager) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char buffer[100];
    bool existed = false;
    if(storage_file_open(
           file, APP_DATA_PATH("player_data.deadzone"), FSAM_READ, FSOM_OPEN_ALWAYS)) {
        FURI_LOG_I("DEADZONE", "Checking for stored username in storage...");
        if(storage_file_read(file, buffer, 100) == 0) {
            FURI_LOG_I("DEADZONE", "No username stored. Might be a new user.");
            usernameLock = api_lock_alloc_locked();
            TextInput* textInput = text_input_alloc();
            text_input_set_header_text(textInput, "Choose a username!");
            text_input_set_validator(textInput, game_menu_text_input_validator, game_manager);
            text_input_set_minimum_length(textInput, 5);
            text_input_set_result_callback(
                textInput, game_menu_text_input_callback, game_manager, username, 100, true);

            view_holder_set_view(view_holder, text_input_get_view(textInput));
            api_lock_wait_unlock_and_free(usernameLock);
            storage_file_close(file);
            if(storage_file_open(
                   file, APP_DATA_PATH("player_data.deadzone"), FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
                storage_file_write(file, username, 100);
                FURI_LOG_I("DEADZONE", "Updating stored username: %s", buffer);
            }
        } else {
            memcpy(username, buffer, sizeof(buffer));
            FURI_LOG_I("DEADZONE", "Found username in storage: %s", username);
            existed = true;
        }
    }

    storage_file_close(file);

    // Deallocate file
    storage_file_free(file);

    // Close storage
    furi_record_close(RECORD_STORAGE);
    return existed;
}

void game_menu_open(GameManager* game_manager, bool reopen) {
    UNUSED(reopen);
    Gui* gui = furi_record_open(RECORD_GUI);

    ViewHolder* view_holder = view_holder_alloc();
    view_holder_attach_to_gui(view_holder, gui);
    FURI_LOG_I("DEADZONE", "Attaching view to gui");
    Submenu* submenu = submenu_alloc();
    submenu_add_item(submenu, "DEADZONE", -1, NULL, game_manager);
    submenu_add_item(submenu, "START GAME", 0, game_menu_button_callback, game_manager);
    submenu_add_item(submenu, "TUTORIAL", 1, game_menu_button_callback, game_manager);
    submenu_add_item(submenu, "QUIT", 2, game_menu_button_callback, game_manager);
    game_menu_exit_lock = api_lock_alloc_locked();

    // Username input
    if(check_username(view_holder, game_manager)) {
        // Game menu

        view_holder_set_view(view_holder, submenu_get_view(submenu));
        FURI_LOG_I("DEADZONE", "Hey, setting the view");
    } else {
        // New user, so start with tutorial
        game_menu_quit_selected = false;
        game_menu_game_selected = false;
        game_menu_tutorial_selected = true;
        showBackground = false;
        NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
        notification_message(notifications, &sequence_success);
        furi_record_close(RECORD_NOTIFICATION);
        api_lock_unlock(game_menu_exit_lock);
    }

    if(reopen) {
        game_manager_game_stop(game_manager);
    }
    api_lock_wait_unlock_and_free(game_menu_exit_lock);

    view_holder_set_view(view_holder, NULL);

    // Delete everything to prevent memory leaks.
    view_holder_free(view_holder);
    submenu_free(submenu);
    // End access to the GUI API.
    furi_record_close(RECORD_GUI);
    FURI_LOG_I("DEADZONE", "Hey, destroyed view!");
}
