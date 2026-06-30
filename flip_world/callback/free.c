#include "callback/free.h"
#include "callback/loader.h"
#include "callback/game.h"

void free_game_submenu(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->submenu_game)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewGameSubmenu);
        submenu_free(app->submenu_game);
        app->submenu_game = NULL;
    }
}

void free_submenu_other(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->submenu_other)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        submenu_free(app->submenu_other);
        app->submenu_other = NULL;
    }
    for (int i = 0; i < 10; i++)
    {
        if (lobby_list[i])
        {
            free(lobby_list[i]);
            lobby_list[i] = NULL;
        }
    }
}

void free_message_view(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->view_message)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewMessage);
        view_free(app->view_message);
        app->view_message = NULL;
    }
}

void free_text_input_view(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->text_input)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewTextInput);
        text_input_free(app->text_input);
        app->text_input = NULL;
    }
    if (app->text_input_buffer)
    {
        free(app->text_input_buffer);
        app->text_input_buffer = NULL;
    }
    if (app->text_input_temp_buffer)
    {
        free(app->text_input_temp_buffer);
        app->text_input_temp_buffer = NULL;
    }
}

void free_variable_item_list(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (app->variable_item_list)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewVariableItemList);
        variable_item_list_free(app->variable_item_list);
        app->variable_item_list = NULL;
    }
    if (app->variable_item_wifi_ssid)
    {
        free(app->variable_item_wifi_ssid);
        app->variable_item_wifi_ssid = NULL;
    }
    if (app->variable_item_wifi_pass)
    {
        free(app->variable_item_wifi_pass);
        app->variable_item_wifi_pass = NULL;
    }
    if (app->variable_item_game_fps)
    {
        free(app->variable_item_game_fps);
        app->variable_item_game_fps = NULL;
    }
    if (app->variable_item_game_screen_always_on)
    {
        free(app->variable_item_game_screen_always_on);
        app->variable_item_game_screen_always_on = NULL;
    }
    if (app->variable_item_game_download_world)
    {
        free(app->variable_item_game_download_world);
        app->variable_item_game_download_world = NULL;
    }
    if (app->variable_item_game_sound_on)
    {
        free(app->variable_item_game_sound_on);
        app->variable_item_game_sound_on = NULL;
    }
    if (app->variable_item_game_vibration_on)
    {
        free(app->variable_item_game_vibration_on);
        app->variable_item_game_vibration_on = NULL;
    }
    if (app->variable_item_game_player_sprite)
    {
        free(app->variable_item_game_player_sprite);
        app->variable_item_game_player_sprite = NULL;
    }
    if (app->variable_item_game_vgm_x)
    {
        free(app->variable_item_game_vgm_x);
        app->variable_item_game_vgm_x = NULL;
    }
    if (app->variable_item_game_vgm_y)
    {
        free(app->variable_item_game_vgm_y);
        app->variable_item_game_vgm_y = NULL;
    }
    if (app->variable_item_user_username)
    {
        free(app->variable_item_user_username);
        app->variable_item_user_username = NULL;
    }
    if (app->variable_item_user_password)
    {
        free(app->variable_item_user_password);
        app->variable_item_user_password = NULL;
    }
}

void free_all_views(void *context, bool free_variable_list, bool free_settings_other, bool free_submenu_game)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    if (free_variable_list)
    {
        free_variable_item_list(app);
    }
    free_message_view(app);
    free_text_input_view(app);

    // free game thread
    if (game_thread_running)
    {
        game_thread_running = false;
        if (game_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(game_thread), WorkerEvtStop);
            furi_thread_join(game_thread);
            furi_thread_free(game_thread);
            game_thread = NULL;
        }
    }

    if (free_settings_other)
    {
        free_submenu_other(app);
    }

    // free Derek's loader
    loader_view_free(app);

    if (free_submenu_game)
    {
        free_game_submenu(app);
    }

    // free waiting thread
    if (waiting_thread_running)
    {
        waiting_thread_running = false;
        if (waiting_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(waiting_thread), WorkerEvtStop);
            furi_thread_join(waiting_thread);
            furi_thread_free(waiting_thread);
            waiting_thread = NULL;
        }
    }
}