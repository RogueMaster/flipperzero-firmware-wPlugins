#include "callback/alloc.h"
#include "alloc/alloc.h"
#include "callback/callback.h"
#include <flip_storage/storage.h>

bool alloc_message_view(void *context, MessageState state)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->view_message)
    {
        FURI_LOG_E(TAG, "Message view already allocated");
        return false;
    }
    switch (state)
    {
    case MessageStateAbout:
        easy_flipper_set_view(&app->view_message, FlipWorldViewMessage, callback_message_draw, NULL, callback_to_submenu, &app->view_dispatcher, app);
        break;
    case MessageStateLoading:
        easy_flipper_set_view(&app->view_message, FlipWorldViewMessage, callback_message_draw, NULL, NULL, &app->view_dispatcher, app);
        break;
    case MessageStateWaitingLobby:
        easy_flipper_set_view(&app->view_message, FlipWorldViewMessage, callback_message_draw, callback_message_input, NULL, &app->view_dispatcher, app);
        break;
    }
    if (!app->view_message)
    {
        FURI_LOG_E(TAG, "Failed to allocate message view");
        return false;
    }
    view_allocate_model(app->view_message, ViewModelTypeLockFree, sizeof(MessageModel));
    MessageModel *model = view_get_model(app->view_message);
    model->message_state = state;
    return true;
}

bool alloc_text_input_view(void *context, char *title)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (!title)
    {
        FURI_LOG_E(TAG, "Title is NULL");
        return false;
    }
    app->text_input_buffer_size = 64;
    if (!app->text_input_buffer)
    {
        if (!easy_flipper_set_buffer(&app->text_input_buffer, app->text_input_buffer_size))
        {
            return false;
        }
    }
    if (!app->text_input_temp_buffer)
    {
        if (!easy_flipper_set_buffer(&app->text_input_temp_buffer, app->text_input_buffer_size))
        {
            return false;
        }
    }
    if (!app->text_input)
    {
        if (!easy_flipper_set_uart_text_input(
                &app->text_input,
                FlipWorldViewTextInput,
                title,
                app->text_input_temp_buffer,
                app->text_input_buffer_size,
                is_str(title, "SSID") ? callback_updated_wifi_ssid : is_str(title, "Password")     ? callback_updated_wifi_pass
                                                                 : is_str(title, "Username-Login") ? callback_updated_username
                                                                                                   : callback_updated_password,
                callback_to_wifi_settings,
                &app->view_dispatcher,
                app))
        {
            return false;
        }
        if (!app->text_input)
        {
            return false;
        }
        char ssid[64];
        char pass[64];
        char username[64];
        char password[64];
        if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
        {
            if (is_str(title, "SSID"))
            {
                strncpy(app->text_input_temp_buffer, ssid, app->text_input_buffer_size);
            }
            else if (is_str(title, "Password"))
            {
                strncpy(app->text_input_temp_buffer, pass, app->text_input_buffer_size);
            }
            else if (is_str(title, "Username-Login"))
            {
                strncpy(app->text_input_temp_buffer, username, app->text_input_buffer_size);
            }
            else if (is_str(title, "Password-Login"))
            {
                strncpy(app->text_input_temp_buffer, password, app->text_input_buffer_size);
            }
        }
    }
    return true;
}
bool alloc_variable_item_list(void *context, uint32_t view_id)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    char ssid[64];
    char pass[64];
    char username[64];
    char password[64];
    if (!app->variable_item_list)
    {
        switch (view_id)
        {
        case FlipWorldSubmenuIndexWiFiSettings:
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, callback_wifi_settings_select, callback_to_settings, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return false;
            }

            if (!app->variable_item_list)
            {
                FURI_LOG_E(TAG, "Variable item list is NULL");
                return false;
            }

            if (!app->variable_item_wifi_ssid)
            {
                app->variable_item_wifi_ssid = variable_item_list_add(app->variable_item_list, "SSID", 0, NULL, NULL);
                variable_item_set_current_value_text(app->variable_item_wifi_ssid, "");
            }
            if (!app->variable_item_wifi_pass)
            {
                app->variable_item_wifi_pass = variable_item_list_add(app->variable_item_list, "Password", 0, NULL, NULL);
                variable_item_set_current_value_text(app->variable_item_wifi_pass, "");
            }
            if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
            {
                variable_item_set_current_value_text(app->variable_item_wifi_ssid, ssid);
                // variable_item_set_current_value_text(app->variable_item_wifi_pass, pass);
                save_char("WiFi-SSID", ssid);
                save_char("WiFi-Password", pass);
                save_char("Flip-Social-Username", username);
                save_char("Flip-Social-Password", password);
            }
            break;
        case FlipWorldSubmenuIndexGameSettings:
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, callback_game_settings_select, callback_to_settings, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return false;
            }

            if (!app->variable_item_list)
            {
                FURI_LOG_E(TAG, "Variable item list is NULL");
                return false;
            }

            if (!app->variable_item_game_download_world)
            {
                app->variable_item_game_download_world = variable_item_list_add(app->variable_item_list, "Install Official World Pack", 0, NULL, NULL);
                variable_item_set_current_value_text(app->variable_item_game_download_world, "");
            }
            if (!app->variable_item_game_player_sprite)
            {
                app->variable_item_game_player_sprite = variable_item_list_add(app->variable_item_list, "Weapon", 4, callback_player_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_player_sprite, 1);
                variable_item_set_current_value_text(app->variable_item_game_player_sprite, player_sprite_choices[1]);
            }
            if (!app->variable_item_game_fps)
            {
                app->variable_item_game_fps = variable_item_list_add(app->variable_item_list, "FPS", 4, callback_fps_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_fps, 0);
                variable_item_set_current_value_text(app->variable_item_game_fps, fps_choices_str[0]);
            }
            if (!app->variable_item_game_vgm_x)
            {
                app->variable_item_game_vgm_x = variable_item_list_add(app->variable_item_list, "VGM Horizontal", 12, callback_vgm_x_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_vgm_x, 2);
                variable_item_set_current_value_text(app->variable_item_game_vgm_x, vgm_levels[2]);
            }
            if (!app->variable_item_game_vgm_y)
            {
                app->variable_item_game_vgm_y = variable_item_list_add(app->variable_item_list, "VGM Vertical", 12, callback_vgm_y_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_vgm_y, 2);
                variable_item_set_current_value_text(app->variable_item_game_vgm_y, vgm_levels[2]);
            }
            if (!app->variable_item_game_screen_always_on)
            {
                app->variable_item_game_screen_always_on = variable_item_list_add(app->variable_item_list, "Keep Screen On?", 2, callback_screen_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_screen_always_on, 1);
                variable_item_set_current_value_text(app->variable_item_game_screen_always_on, yes_or_no_choices[1]);
            }
            if (!app->variable_item_game_sound_on)
            {
                app->variable_item_game_sound_on = variable_item_list_add(app->variable_item_list, "Sound On?", 2, callback_sound_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_sound_on, 0);
                variable_item_set_current_value_text(app->variable_item_game_sound_on, yes_or_no_choices[0]);
            }
            if (!app->variable_item_game_vibration_on)
            {
                app->variable_item_game_vibration_on = variable_item_list_add(app->variable_item_list, "Vibration On?", 2, callback_vibration_on_change, NULL);
                variable_item_set_current_value_index(app->variable_item_game_vibration_on, 0);
                variable_item_set_current_value_text(app->variable_item_game_vibration_on, yes_or_no_choices[0]);
            }
            char _game_player_sprite[8];
            if (load_char("Game-Player-Sprite", _game_player_sprite, sizeof(_game_player_sprite)))
            {
                int index = is_str(_game_player_sprite, "naked") ? 0 : is_str(_game_player_sprite, "sword") ? 1
                                                                   : is_str(_game_player_sprite, "axe")     ? 2
                                                                   : is_str(_game_player_sprite, "bow")     ? 3
                                                                                                            : 0;
                variable_item_set_current_value_index(app->variable_item_game_player_sprite, index);
                variable_item_set_current_value_text(
                    app->variable_item_game_player_sprite,
                    is_str(player_sprite_choices[index], "naked") ? "None" : player_sprite_choices[index]);
            }
            char _game_fps[8];
            if (load_char("Game-FPS", _game_fps, sizeof(_game_fps)))
            {
                int index = is_str(_game_fps, "30") ? 0 : is_str(_game_fps, "60") ? 1
                                                      : is_str(_game_fps, "120")  ? 2
                                                      : is_str(_game_fps, "240")  ? 3
                                                                                  : 0;
                variable_item_set_current_value_text(app->variable_item_game_fps, fps_choices_str[index]);
                variable_item_set_current_value_index(app->variable_item_game_fps, index);
            }
            char _game_vgm_x[8];
            if (load_char("Game-VGM-X", _game_vgm_x, sizeof(_game_vgm_x)))
            {
                int vgm_x = atoi(_game_vgm_x);
                int index = vgm_x == -2 ? 0 : vgm_x == -1 ? 1
                                          : vgm_x == 0    ? 2
                                          : vgm_x == 1    ? 3
                                          : vgm_x == 2    ? 4
                                          : vgm_x == 3    ? 5
                                          : vgm_x == 4    ? 6
                                          : vgm_x == 5    ? 7
                                          : vgm_x == 6    ? 8
                                          : vgm_x == 7    ? 9
                                          : vgm_x == 8    ? 10
                                          : vgm_x == 9    ? 11
                                          : vgm_x == 10   ? 12
                                                          : 2;
                variable_item_set_current_value_index(app->variable_item_game_vgm_x, index);
                variable_item_set_current_value_text(app->variable_item_game_vgm_x, vgm_levels[index]);
            }
            char _game_vgm_y[8];
            if (load_char("Game-VGM-Y", _game_vgm_y, sizeof(_game_vgm_y)))
            {
                int vgm_y = atoi(_game_vgm_y);
                int index = vgm_y == -2 ? 0 : vgm_y == -1 ? 1
                                          : vgm_y == 0    ? 2
                                          : vgm_y == 1    ? 3
                                          : vgm_y == 2    ? 4
                                          : vgm_y == 3    ? 5
                                          : vgm_y == 4    ? 6
                                          : vgm_y == 5    ? 7
                                          : vgm_y == 6    ? 8
                                          : vgm_y == 7    ? 9
                                          : vgm_y == 8    ? 10
                                          : vgm_y == 9    ? 11
                                          : vgm_y == 10   ? 12
                                                          : 2;
                variable_item_set_current_value_index(app->variable_item_game_vgm_y, index);
                variable_item_set_current_value_text(app->variable_item_game_vgm_y, vgm_levels[index]);
            }
            char _game_screen_always_on[8];
            if (load_char("Game-Screen-Always-On", _game_screen_always_on, sizeof(_game_screen_always_on)))
            {
                int index = is_str(_game_screen_always_on, "No") ? 0 : is_str(_game_screen_always_on, "Yes") ? 1
                                                                                                             : 0;
                variable_item_set_current_value_text(app->variable_item_game_screen_always_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_screen_always_on, index);
            }
            char _game_sound_on[8];
            if (load_char("Game-Sound-On", _game_sound_on, sizeof(_game_sound_on)))
            {
                int index = is_str(_game_sound_on, "No") ? 0 : is_str(_game_sound_on, "Yes") ? 1
                                                                                             : 0;
                variable_item_set_current_value_text(app->variable_item_game_sound_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_sound_on, index);
            }
            char _game_vibration_on[8];
            if (load_char("Game-Vibration-On", _game_vibration_on, sizeof(_game_vibration_on)))
            {
                int index = is_str(_game_vibration_on, "No") ? 0 : is_str(_game_vibration_on, "Yes") ? 1
                                                                                                     : 0;
                variable_item_set_current_value_text(app->variable_item_game_vibration_on, yes_or_no_choices[index]);
                variable_item_set_current_value_index(app->variable_item_game_vibration_on, index);
            }
            break;
        case FlipWorldSubmenuIndexUserSettings:
            if (!easy_flipper_set_variable_item_list(&app->variable_item_list, FlipWorldViewVariableItemList, callback_user_settings_select, callback_to_settings, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return false;
            }

            if (!app->variable_item_list)
            {
                FURI_LOG_E(TAG, "Variable item list is NULL");
                return false;
            }

            // if logged in, show profile info, otherwise show login/register
            if (is_logged_in() || is_logged_in_to_flip_social())
            {
                if (!app->variable_item_user_username)
                {
                    app->variable_item_user_username = variable_item_list_add(app->variable_item_list, "Username", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_username, "");
                }
                if (!app->variable_item_user_password)
                {
                    app->variable_item_user_password = variable_item_list_add(app->variable_item_list, "Password", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_password, "");
                }
                if (load_settings(ssid, sizeof(ssid), pass, sizeof(pass), username, sizeof(username), password, sizeof(password)))
                {
                    variable_item_set_current_value_text(app->variable_item_user_username, username);
                    variable_item_set_current_value_text(app->variable_item_user_password, "*****");
                }
            }
            else
            {
                if (!app->variable_item_user_username)
                {
                    app->variable_item_user_username = variable_item_list_add(app->variable_item_list, "Username", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_username, "");
                }
                if (!app->variable_item_user_password)
                {
                    app->variable_item_user_password = variable_item_list_add(app->variable_item_list, "Password", 0, NULL, NULL);
                    variable_item_set_current_value_text(app->variable_item_user_password, "");
                }
            }
            break;
        }
    }
    return true;
}
bool alloc_submenu_other(void *context, uint32_t view_id)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->submenu_other)
    {
        FURI_LOG_I(TAG, "Submenu already allocated");
        return true;
    }
    switch (view_id)
    {
    case FlipWorldViewSettings:
        if (!easy_flipper_set_submenu(&app->submenu_other, FlipWorldViewSubmenuOther, "Settings", callback_to_submenu, &app->view_dispatcher))
        {
            FURI_LOG_E(TAG, "Failed to allocate submenu settings");
            return false;
        }
        submenu_add_item(app->submenu_other, "WiFi", FlipWorldSubmenuIndexWiFiSettings, callback_submenu_choices, app);
        submenu_add_item(app->submenu_other, "Game", FlipWorldSubmenuIndexGameSettings, callback_submenu_choices, app);
        submenu_add_item(app->submenu_other, "User", FlipWorldSubmenuIndexUserSettings, callback_submenu_choices, app);
        return true;
    case FlipWorldViewLobby:
        return easy_flipper_set_submenu(&app->submenu_other, FlipWorldViewSubmenuOther, "Lobbies", callback_to_submenu, &app->view_dispatcher);
    default:
        return false;
    }
}

bool alloc_game_submenu(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (!app->submenu_game)
    {
        if (!easy_flipper_set_submenu(&app->submenu_game, FlipWorldViewGameSubmenu, "Play", callback_to_submenu, &app->view_dispatcher))
        {
            return false;
        }
        if (!app->submenu_game)
        {
            return false;
        }
        submenu_add_item(app->submenu_game, "Story", FlipWorldSubmenuIndexStory, callback_submenu_choices, app);
        submenu_add_item(app->submenu_game, "PvE", FlipWorldSubmenuIndexPvE, callback_submenu_choices, app);
        submenu_add_item(app->submenu_game, "PvP", FlipWorldSubmenuIndexPvP, callback_submenu_choices, app);
    }
    return true;
}