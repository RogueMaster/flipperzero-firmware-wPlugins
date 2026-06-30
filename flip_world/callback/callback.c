#include <callback/callback.h>
#include <callback/loader.h>
#include <callback/free.h>
#include <callback/alloc.h>
#include <callback/game.h>
#include "alloc/alloc.h"
#include <flip_storage/storage.h>

bool callback_message_input(InputEvent *event, void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (event->key == InputKeyBack)
    {
        FURI_LOG_I(TAG, "Message view - BACK pressed");
        user_hit_back = true;
    }
    return true;
}

void callback_message_draw(Canvas *canvas, void *model)
{
    MessageModel *message_model = model;
    canvas_clear(canvas);
    if (message_model->message_state == MessageStateAbout)
    {
        canvas_draw_str(canvas, 0, 10, VERSION_TAG);
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str(canvas, 0, 20, "Dev: JBlanked, codeallnight");
        canvas_draw_str(canvas, 0, 30, "GFX: the1anonlypr3");
        canvas_draw_str(canvas, 0, 40, "github.com/jblanked/FlipWorld");

        canvas_draw_str_multi(canvas, 0, 55, "The first open world multiplayer\ngame on the Flipper Zero.");
    }
    else if (message_model->message_state == MessageStateLoading)
    {
        canvas_set_font(canvas, FontPrimary);
        if (game_mode_index != 1)
        {
            canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Starting FlipWorld");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 50, "Please wait while your");
            canvas_draw_str(canvas, 0, 60, "game is started.");
        }
        else
        {
            canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Loading Lobbies");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 60, "Please wait....");
        }
    }
    // well this is only called once so let's make a while loop
    else if (message_model->message_state == MessageStateWaitingLobby)
    {
        canvas_draw_str(canvas, 0, 10, "Waiting for more players...");
        // time elapsed based on timer_iteration and timer_refresh
        // char str[32];
        // snprintf(str, sizeof(str), "Time elapsed: %d seconds", timer_iteration * timer_refresh);
        // canvas_draw_str(canvas, 0, 50, str);
        canvas_draw_str(canvas, 0, 60, "Press BACK to cancel.");
        canvas_commit(canvas); // make sure message is drawn
    }
    else
    {
        canvas_draw_str(canvas, 0, 10, "Unknown message state");
    }
}

void callback_submenu_choices(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    switch (index)
    {
    case FlipWorldSubmenuIndexGameSubmenu:
        free_all_views(app, true, true, true);
        if (!alloc_game_submenu(app))
        {
            FURI_LOG_E(TAG, "Failed to allocate game submenu");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewGameSubmenu);
        break;
    case FlipWorldSubmenuIndexStory:
        game_mode_index = 2; // GAME_MODE_STORY
        game_run(app);
        break;
    case FlipWorldSubmenuIndexPvP:
        game_mode_index = 1; // GAME_MODE_PVP
        game_run(app);
        break;
    case FlipWorldSubmenuIndexPvE:
        game_mode_index = 0; // GAME_MODE_PVE
        game_run(app);
        break;
    case FlipWorldSubmenuIndexMessage:
        // About menu.
        free_all_views(app, true, true, true);
        if (!alloc_message_view(app, MessageStateAbout))
        {
            FURI_LOG_E(TAG, "Failed to allocate message view");
            return;
        }

        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewMessage);
        break;
    case FlipWorldSubmenuIndexSettings:
        free_all_views(app, true, true, true);
        if (!alloc_submenu_other(app, FlipWorldViewSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate settings view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        break;
    case FlipWorldSubmenuIndexWiFiSettings:
        free_all_views(app, true, false, true);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexWiFiSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    case FlipWorldSubmenuIndexGameSettings:
        free_all_views(app, true, false, true);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexGameSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    case FlipWorldSubmenuIndexUserSettings:
        free_all_views(app, true, false, true);
        if (!alloc_variable_item_list(app, FlipWorldSubmenuIndexUserSettings))
        {
            FURI_LOG_E(TAG, "Failed to allocate variable item list");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        break;
    default:
        break;
    }
}

void callback_updated_wifi_ssid(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");

    // store the entered text
    strncpy(app->text_input_buffer, app->text_input_temp_buffer, app->text_input_buffer_size);

    // Ensure null-termination
    app->text_input_buffer[app->text_input_buffer_size - 1] = '\0';

    // save the setting
    save_char("WiFi-SSID", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_wifi_ssid)
    {
        variable_item_set_current_value_text(app->variable_item_wifi_ssid, app->text_input_buffer);

        // get value of password
        char pass[64];
        char username[64];
        char password[64];
        if (load_char("WiFi-Password", pass, sizeof(pass)))
        {
            if (strlen(pass) > 0 && strlen(app->text_input_buffer) > 0)
            {
                // save the settings
                load_char("Flip-Social-Username", username, sizeof(username));
                load_char("Flip-Social-Password", password, sizeof(password));
                save_settings(app->text_input_buffer, pass, username, password);

                // initialize the http
                FlipperHTTP *fhttp = flipper_http_alloc();
                if (fhttp)
                {
                    // save the wifi if the device is connected
                    if (!flipper_http_save_wifi(fhttp, app->text_input_buffer, pass))
                    {
                        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
                    }

                    // free the resources
                    flipper_http_free(fhttp);
                }
                else
                {
                    easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
                }
            }
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
}
void callback_updated_wifi_pass(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");

    // store the entered text
    strncpy(app->text_input_buffer, app->text_input_temp_buffer, app->text_input_buffer_size);

    // Ensure null-termination
    app->text_input_buffer[app->text_input_buffer_size - 1] = '\0';

    // save the setting
    save_char("WiFi-Password", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_wifi_pass)
    {
        // variable_item_set_current_value_text(app->variable_item_wifi_pass, app->text_input_buffer);
    }

    // get value of ssid
    char ssid[64];
    char username[64];
    char password[64];
    if (load_char("WiFi-SSID", ssid, sizeof(ssid)))
    {
        if (strlen(ssid) > 0 && strlen(app->text_input_buffer) > 0)
        {
            // save the settings
            load_char("Flip-Social-Username", username, sizeof(username));
            load_char("Flip-Social-Password", password, sizeof(password));
            save_settings(ssid, app->text_input_buffer, username, password);

            // initialize the http
            FlipperHTTP *fhttp = flipper_http_alloc();
            if (fhttp)
            {
                // save the wifi if the device is connected
                if (!flipper_http_save_wifi(fhttp, ssid, app->text_input_buffer))
                {
                    easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
                }

                // free the resources
                flipper_http_free(fhttp);
            }
            else
            {
                easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
            }
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList);
}
void callback_updated_username(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");

    // store the entered text
    strncpy(app->text_input_buffer, app->text_input_temp_buffer, app->text_input_buffer_size);

    // Ensure null-termination
    app->text_input_buffer[app->text_input_buffer_size - 1] = '\0';

    // save the setting
    save_char("Flip-Social-Username", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_user_username)
    {
        variable_item_set_current_value_text(app->variable_item_user_username, app->text_input_buffer);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList); // back to user settings
}
void callback_updated_password(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");

    // store the entered text
    strncpy(app->text_input_buffer, app->text_input_temp_buffer, app->text_input_buffer_size);

    // Ensure null-termination
    app->text_input_buffer[app->text_input_buffer_size - 1] = '\0';

    // save the setting
    save_char("Flip-Social-Password", app->text_input_buffer);

    // update the variable item text
    if (app->variable_item_user_password)
    {
        variable_item_set_current_value_text(app->variable_item_user_password, app->text_input_buffer);
    }

    // get value of username
    char username[64];
    char ssid[64];
    char pass[64];
    if (load_char("Flip-Social-Username", username, sizeof(username)))
    {
        if (strlen(username) > 0 && strlen(app->text_input_buffer) > 0)
        {
            // save the settings
            load_char("WiFi-SSID", ssid, sizeof(ssid));
            load_char("WiFi-Password", pass, sizeof(pass));
            save_settings(ssid, pass, username, app->text_input_buffer);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewVariableItemList); // back to user settings
}

void callback_wifi_settings_select(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    char ssid[64];
    char pass[64];
    char username[64];
    char password[64];
    switch (index)
    {
    case 0: // Input SSID
        free_all_views(app, false, false, true);
        if (!alloc_text_input_view(app, "SSID"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        // load SSID
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
        {
            strncpy(app->text_input_temp_buffer, ssid, app->text_input_buffer_size - 1);
            app->text_input_temp_buffer[app->text_input_buffer_size - 1] = '\0';
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    case 1: // Input Password
        free_all_views(app, false, false, true);
        if (!alloc_text_input_view(app, "Password"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        // load password
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
        {
            strncpy(app->text_input_temp_buffer, pass, app->text_input_buffer_size - 1);
            app->text_input_temp_buffer[app->text_input_buffer_size - 1] = '\0';
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}
void callback_fps_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    fps_index = index;
    variable_item_set_current_value_text(item, fps_choices_str[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-FPS", fps_choices_str[index]);
}
void callback_screen_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    screen_always_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Screen-Always-On", yes_or_no_choices[index]);
}
void callback_sound_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    sound_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Sound-On", yes_or_no_choices[index]);
}
void callback_vibration_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    vibration_on_index = index;
    variable_item_set_current_value_text(item, yes_or_no_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Vibration-On", yes_or_no_choices[index]);
}
void callback_player_on_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    player_sprite_index = index;
    variable_item_set_current_value_text(item, is_str(player_sprite_choices[index], "naked") ? "None" : player_sprite_choices[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-Player-Sprite", player_sprite_choices[index]);
}
void callback_vgm_x_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    vgm_x_index = index;
    variable_item_set_current_value_text(item, vgm_levels[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-VGM-X", vgm_levels[index]);
}
void callback_vgm_y_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    vgm_y_index = index;
    variable_item_set_current_value_text(item, vgm_levels[index]);
    variable_item_set_current_value_index(item, index);
    save_char("Game-VGM-Y", vgm_levels[index]);
}

static bool _fetch_worlds(DataLoaderModel *model)
{
    if (!model || !model->fhttp)
    {
        FURI_LOG_E(TAG, "model or fhttp is NULL");
        return false;
    }
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
    snprintf(model->fhttp->file_path, sizeof(model->fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list_full.json");
    model->fhttp->save_received_data = true;
    return flipper_http_request(model->fhttp, GET, "https://www.jblanked.com/flipper/api/world/v8/get/10/", "{\"Content-Type\":\"application/json\"}", NULL);
}
static char *_parse_worlds(DataLoaderModel *model)
{
    UNUSED(model);
    return "World Pack Installed";
}
static void switch_to_view_get_worlds(FlipWorldApp *app)
{
    if (!loader_view_alloc(app))
    {
        FURI_LOG_E(TAG, "Failed to allocate view loader");
        return;
    }
    loader_switch_to_view(app, "Fetching World Pack..", _fetch_worlds, _parse_worlds, 1, callback_to_submenu, FlipWorldViewLoader);
}
void callback_game_settings_select(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    switch (index)
    {
    case 0: // Download all world data as one huge json
        switch_to_view_get_worlds(app);
    case 1: // Player Sprite
        break;
    case 2: // Change FPS
        break;
    case 3: // VGM X
        break;
    case 4: // VGM Y
        break;
    case 5: // Screen Always On
        break;
    case 6: // Sound On
        break;
    case 7: // Vibration On
        break;
    }
}
void callback_user_settings_select(void *context, uint32_t index)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    switch (index)
    {
    case 0: // Username
        free_all_views(app, false, false, true);
        if (!alloc_text_input_view(app, "Username-Login"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    case 1: // Password
        free_all_views(app, false, false, true);
        if (!alloc_text_input_view(app, "Password-Login"))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewTextInput);
        break;
    }
}

void callback_submenu_lobby_pvp_choices(void *context, uint32_t index)
{
    /* Handle other game lobbies
             1. when clicked on, send request to fetch the selected game lobby details
             2. start the websocket session
             3. start the game thread (the rest will be handled by game_start_game and player_update)
             */
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    if (index >= FlipWorldSubmenuIndexLobby && index < FlipWorldSubmenuIndexLobby + 10)
    {
        lobby_index = index - FlipWorldSubmenuIndexLobby;

        FlipperHTTP *fhttp = flipper_http_alloc();
        if (!fhttp)
        {
            FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
            easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP. Press BACK to return.");
            return;
        }

        // fetch the lobby details
        if (!game_fetch_lobby(fhttp, lobby_list[lobby_index]))
        {
            FURI_LOG_E(TAG, "Failed to fetch lobby details");
            easy_flipper_dialog("Error", "Failed to fetch lobby details. Press BACK to return.");
            flipper_http_free(fhttp);
            return;
        }

        // load the lobby details
        FuriString *lobby = flipper_http_load_from_file(fhttp->file_path);
        if (!lobby)
        {
            FURI_LOG_E(TAG, "Failed to load lobby details");
            flipper_http_free(fhttp);
            return;
        }

        // if there are no players, add the user to the lobby and make the user wait until another player joins
        // if there is one player and it's the user, make the user wait until another player joins
        // if there is one player and it's not the user, parse_lobby and start websocket
        // if there are 2 players (which there shouldn't be at this point), show an error message saying the lobby is full
        if (game_mode_index == 1) // pvp
        {
            switch (game_lobby_count(fhttp, lobby))
            {
            case -1:
                FURI_LOG_E(TAG, "Failed to get player count");
                easy_flipper_dialog("Error", "Failed to get player count. Press BACK to return.");
                flipper_http_free(fhttp);
                furi_string_free(lobby);
                return;
            case 0:
                // add the user to the lobby
                if (!game_join_lobby(fhttp, lobby_list[lobby_index]))
                {
                    FURI_LOG_E(TAG, "Failed to join lobby");
                    easy_flipper_dialog("Error", "Failed to join lobby. Press BACK to return.");
                    flipper_http_free(fhttp);
                    furi_string_free(lobby);
                    return;
                }
                // send the user to the waiting screen
                game_waiting_lobby(app);
                return;
            case 1:
                // check if the user is in the lobby
                if (game_in_lobby(fhttp, lobby))
                {
                    if (!game_remove_from_lobby(fhttp))
                    {
                        FURI_LOG_I(TAG, "User is in the lobby but failed to remove");
                        easy_flipper_dialog("Error", "You're already in the lobby.\nContact JBlanked.\n\nPress BACK to return.");
                        flipper_http_free(fhttp);
                        furi_string_free(lobby);
                        return;
                    }
                }
                // add the user to the lobby
                if (!game_join_lobby(fhttp, lobby_list[lobby_index]))
                {
                    FURI_LOG_E(TAG, "Failed to join lobby");
                    easy_flipper_dialog("Error", "Failed to join lobby. Press BACK to return.");
                    flipper_http_free(fhttp);
                    furi_string_free(lobby);
                    return;
                }
                break;
            case 2:
                // show an error message saying the lobby is full
                FURI_LOG_E(TAG, "Lobby is full");
                easy_flipper_dialog("Error", "Lobby is full. Press BACK to return.");
                flipper_http_free(fhttp);
                furi_string_free(lobby);
                return;
            };
        }
        else
        {
            // check if the user is in the lobby
            if (game_in_lobby(fhttp, lobby))
            {
                if (!game_remove_from_lobby(fhttp))
                {
                    FURI_LOG_I(TAG, "User is in the lobby but failed to remove");
                    easy_flipper_dialog("Error", "You're already in the lobby.\nContact JBlanked.\n\nPress BACK to return.");
                    flipper_http_free(fhttp);
                    furi_string_free(lobby);
                    return;
                }
            }
            // add the user to the lobby
            if (!game_join_lobby(fhttp, lobby_list[lobby_index]))
            {
                FURI_LOG_E(TAG, "Failed to join lobby");
                easy_flipper_dialog("Error", "Failed to join lobby. Press BACK to return.");
                flipper_http_free(fhttp);
                furi_string_free(lobby);
                return;
            }
        }

        game_start_game(fhttp, lobby, app); // this will free both the fhttp and lobby, and start the game
    }
}