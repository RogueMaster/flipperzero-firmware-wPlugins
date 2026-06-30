#include <callback/game.h>
//
#include "engine/engine.h"
#include "engine/game_engine.h"
#include "engine/game_manager_i.h"
#include "engine/level_i.h"
#include "engine/entity_i.h"
//
#include "game/storage.h"
//
#include <callback/loader.h>
#include <callback/free.h>
#include <callback/alloc.h>
#include <callback/callback.h>
#include "alloc/alloc.h"
#include <flip_storage/storage.h>

bool user_hit_back = false;
uint32_t lobby_index = -1;
char *lobby_list[10];
char game_ws_lobby_name[64];

static uint8_t timer_iteration = 0; // timer iteration for the loading screen
static uint8_t timer_refresh = 5;   // duration for timer to refresh

FuriThread *game_thread = NULL;
FuriThread *waiting_thread = NULL;
bool game_thread_running = false;
bool waiting_thread_running = false;

static void game_frame_cb(GameEngine *engine, Canvas *canvas, InputState input, void *context)
{
    UNUSED(engine);
    GameManager *game_manager = context;
    game_manager_input_set(game_manager, input);
    game_manager_update(game_manager);
    game_manager_render(game_manager, canvas);
}

static int32_t game_app(void *p)
{
    UNUSED(p);
    GameManager *game_manager = game_manager_alloc();
    if (!game_manager)
    {
        FURI_LOG_E("Game", "Failed to allocate game manager");
        return -1;
    }

    // Setup game engine settings...
    GameEngineSettings settings = game_engine_settings_init();
    settings.target_fps = atof_(fps_choices_str[fps_index]);
    settings.show_fps = game.show_fps;
    settings.always_backlight = strstr(yes_or_no_choices[screen_always_on_index], "Yes") != NULL;
    settings.frame_callback = game_frame_cb;
    settings.context = game_manager;
    GameEngine *engine = game_engine_alloc(settings);
    if (!engine)
    {
        FURI_LOG_E("Game", "Failed to allocate game engine");
        game_manager_free(game_manager);
        return -1;
    }
    game_manager_engine_set(game_manager, engine);

    // Allocate custom game context if needed
    void *game_context = NULL;
    if (game.context_size > 0)
    {
        game_context = malloc(game.context_size);
        game_manager_game_context_set(game_manager, game_context);
    }

    // Start the game
    game.start(game_manager, game_context);

    // 1) Run the engine
    game_engine_run(engine);

    // 2) Stop the game FIRST, so it can do any internal cleanup
    game.stop(game_context);

    // 3) Now free the engine
    game_engine_free(engine);

    // 4) Now free the manager
    game_manager_free(game_manager);

    // 5) Finally, free your custom context if it was allocated
    if (game_context)
    {
        free(game_context);
    }

    // 6) Check for leftover entities
    int32_t entities = entities_get_count();
    if (entities != 0)
    {
        FURI_LOG_E("Game", "Memory leak detected: %ld entities still allocated", entities);
        return -1;
    }

    return 0;
}

static int32_t game_waiting_app_callback(void *p)
{
    FlipWorldApp *app = (FlipWorldApp *)p;
    furi_check(app);
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP");
        return -1;
    }
    user_hit_back = false;
    timer_iteration = 0;
    while (timer_iteration < 60 && !user_hit_back)
    {
        FURI_LOG_I(TAG, "Waiting for more players...");
        game_waiting_process(fhttp, app);
        FURI_LOG_I(TAG, "Waiting for more players... %d", timer_iteration);
        timer_iteration++;
        furi_delay_ms(1000 * timer_refresh);
    }
    // if we reach here, it means we timed out or the user hit back
    FURI_LOG_E(TAG, "No players joined within the timeout or user hit back");
    game_remove_from_lobby(fhttp);
    flipper_http_free(fhttp);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
    return 0;
}

static bool game_start_waiting_thread(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    // free game thread
    if (waiting_thread_running)
    {
        waiting_thread_running = false;
        if (waiting_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(waiting_thread), WorkerEvtStop);
            furi_thread_join(waiting_thread);
            furi_thread_free(waiting_thread);
        }
    }
    // start waiting thread
    FuriThread *thread = furi_thread_alloc_ex("waiting_thread", 2048, game_waiting_app_callback, app);
    if (!thread)
    {
        FURI_LOG_E(TAG, "Failed to allocate waiting thread");
        easy_flipper_dialog("Error", "Failed to allocate waiting thread. Restart your Flipper.");
        return false;
    }
    furi_thread_start(thread);
    waiting_thread = thread;
    waiting_thread_running = true;
    return true;
}

static bool game_fetch_world_list(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        easy_flipper_dialog("Error", "fhttp is NULL. Press BACK to return.");
        return false;
    }

    // ensure flip_world directory exists
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);

    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

    fhttp->save_received_data = true;
    return flipper_http_request(fhttp, GET, "https://www.jblanked.com/flipper/api/world/v8/list/10/", "{\"Content-Type\":\"application/json\"}", NULL);
}
// we will load the palyer stats from the API and save them
// in player_spawn game method, it will load the player stats that we saved
static bool game_fetch_player_stats(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        easy_flipper_dialog("Error", "fhttp is NULL. Press BACK to return.");
        return false;
    }
    char username[64];
    if (!load_char("Flip-Social-Username", username, sizeof(username)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        easy_flipper_dialog("Error", "Failed to load saved username. Go to settings to update.");
        return false;
    }
    char url[128];
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/user/game-stats/%s/", username);

    // ensure the folders exist
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data");
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);

    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player/player_stats.json");
    fhttp->save_received_data = true;
    return flipper_http_request(fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL);
}

static bool game_thread_start(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "app is NULL");
        easy_flipper_dialog("Error", "app is NULL. Press BACK to return.");
        return false;
    }

    // free everything but message_view
    free_variable_item_list(app);
    free_text_input_view(app);
    // free_submenu_other(app); // free lobby list or settings
    loader_view_free(app);
    free_game_submenu(app);

    // free game thread
    if (game_thread_running)
    {
        game_thread_running = false;
        if (game_thread)
        {
            furi_thread_flags_set(furi_thread_get_id(game_thread), WorkerEvtStop);
            furi_thread_join(game_thread);
            furi_thread_free(game_thread);
        }
    }
    // start game thread
    FuriThread *thread = furi_thread_alloc_ex("game", 2048, game_app, app);
    if (!thread)
    {
        FURI_LOG_E(TAG, "Failed to allocate game thread");
        easy_flipper_dialog("Error", "Failed to allocate game thread. Restart your Flipper.");
        return false;
    }
    furi_thread_start(thread);
    game_thread = thread;
    game_thread_running = true;
    return true;
}
// combine register, login, and world list fetch into one function to switch to the loader view
static bool game_fetch(DataLoaderModel *model)
{
    FlipWorldApp *app = (FlipWorldApp *)model->parser_context;
    if (!app)
    {
        FURI_LOG_E(TAG, "app is NULL");
        easy_flipper_dialog("Error", "app is NULL. Press BACK to return.");
        return false;
    }
    if (model->request_index == 0)
    {
        // login
        char username[64];
        char password[64];
        if (!load_char("Flip-Social-Username", username, sizeof(username)))
        {
            FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            easy_flipper_dialog("Error", "Failed to load saved username\nGo to user settings to update.");
            return false;
        }
        if (!load_char("Flip-Social-Password", password, sizeof(password)))
        {
            FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            easy_flipper_dialog("Error", "Failed to load saved password\nGo to settings to update.");
            return false;
        }
        char payload[256];
        snprintf(payload, sizeof(payload), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
        return flipper_http_request(model->fhttp, POST, "https://www.jblanked.com/flipper/api/user/login/", "{\"Content-Type\":\"application/json\"}", payload);
    }
    else if (model->request_index == 1)
    {
        // check if login was successful
        char is_logged_in[8];
        if (!load_char("is_logged_in", is_logged_in, sizeof(is_logged_in)))
        {
            FURI_LOG_E(TAG, "Failed to load is_logged_in");
            easy_flipper_dialog("Error", "Failed to load is_logged_in\nGo to user settings to update.");
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            return false;
        }
        if (is_str(is_logged_in, "false") && is_str(model->title, "Registering..."))
        {
            // register
            char username[64];
            char password[64];
            if (!load_char("Flip-Social-Username", username, sizeof(username)))
            {
                FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
                easy_flipper_dialog("Error", "Failed to load saved username. Go to settings to update.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return false;
            }
            if (!load_char("Flip-Social-Password", password, sizeof(password)))
            {
                FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
                easy_flipper_dialog("Error", "Failed to load saved password. Go to settings to update.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return false;
            }
            char payload[172];
            snprintf(payload, sizeof(payload), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
            model->title = "Registering...";
            return flipper_http_request(model->fhttp, POST, "https://www.jblanked.com/flipper/api/user/register/", "{\"Content-Type\":\"application/json\"}", payload);
        }
        else
        {
            model->title = "Fetching World List..";
            return game_fetch_world_list(model->fhttp);
        }
    }
    else if (model->request_index == 2)
    {
        model->title = "Fetching World List..";
        return game_fetch_world_list(model->fhttp);
    }
    else if (model->request_index == 3)
    {
        snprintf(model->fhttp->file_path, sizeof(model->fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

        FuriString *world_list = flipper_http_load_from_file(model->fhttp->file_path);
        if (!world_list)
        {
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            FURI_LOG_E(TAG, "Failed to load world list");
            easy_flipper_dialog("Error", "Failed to load world list. Go to game settings to download packs.");
            return false;
        }
        FuriString *first_world = get_json_array_value_furi("worlds", 0, world_list);
        if (!first_world)
        {
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            FURI_LOG_E(TAG, "Failed to get first world");
            easy_flipper_dialog("Error", "Failed to get first world. Go to game settings to download packs.");
            furi_string_free(world_list);
            return false;
        }
        if (world_exists(furi_string_get_cstr(first_world)))
        {
            furi_string_free(world_list);
            furi_string_free(first_world);

            if (!game_thread_start(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Failed to start game thread";
            }
            return true;
        }
        snprintf(model->fhttp->file_path, sizeof(model->fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", furi_string_get_cstr(first_world));

        model->fhttp->save_received_data = true;
        char url[128];
        snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/v8/get/world/%s/", furi_string_get_cstr(first_world));
        furi_string_free(world_list);
        furi_string_free(first_world);
        return flipper_http_request(model->fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL);
    }
    FURI_LOG_E(TAG, "Unknown request index");
    return false;
}
static char *game_parse(DataLoaderModel *model)
{
    FlipWorldApp *app = (FlipWorldApp *)model->parser_context;

    if (model->request_index == 0)
    {
        if (!model->fhttp->last_response)
        {
            save_char("is_logged_in", "false");
            // Go back to the main menu
            easy_flipper_dialog("Error", "Response is empty. Press BACK to return.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
            return "Response is empty...";
        }

        // Check for successful conditions
        if (strstr(model->fhttp->last_response, "[SUCCESS]") != NULL || strstr(model->fhttp->last_response, "User found") != NULL)
        {
            save_char("is_logged_in", "true");
            model->title = "Login successful!";
            model->title = "Fetching World List..";
            return "Login successful!";
        }

        // Check if user not found
        if (strstr(model->fhttp->last_response, "User not found") != NULL)
        {
            save_char("is_logged_in", "false");
            model->title = "Registering...";
            return "Account not found...\nRegistering now.."; // if they see this an issue happened switching to register
        }

        // If not success, not found, check length conditions
        size_t resp_len = strlen(model->fhttp->last_response);
        if (resp_len == 0 || resp_len > 127)
        {
            // Empty or too long means failed login
            save_char("is_logged_in", "false");
            // Go back to the main menu
            easy_flipper_dialog("Error", "Failed to login. Press BACK to return.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
            return "Failed to login...";
        }

        // Handle any other unknown response as a failure
        save_char("is_logged_in", "false");
        // Go back to the main menu
        easy_flipper_dialog("Error", "Failed to login. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
        return "Failed to login...";
    }
    else if (model->request_index == 1)
    {
        if (is_str(model->title, "Registering..."))
        {
            // check registration response
            if (model->fhttp->last_response != NULL && (strstr(model->fhttp->last_response, "[SUCCESS]") != NULL || strstr(model->fhttp->last_response, "User created") != NULL))
            {
                save_char("is_logged_in", "true");
                char username[64];
                char password[64];
                // load the username and password, then save them
                if (!load_char("Flip-Social-Username", username, sizeof(username)))
                {
                    FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
                    easy_flipper_dialog("Error", "Failed to load Flip-Social-Username");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load Flip-Social-Username";
                }
                if (!load_char("Flip-Social-Password", password, sizeof(password)))
                {
                    FURI_LOG_E(TAG, "Failed to load Flip-Social-Password");
                    easy_flipper_dialog("Error", "Failed to load Flip-Social-Password");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load Flip-Social-Password";
                }
                // load wifi ssid,pass then save
                char ssid[64];
                char pass[64];
                if (!load_char("WiFi-SSID", ssid, sizeof(ssid)))
                {
                    FURI_LOG_E(TAG, "Failed to load WiFi-SSID");
                    easy_flipper_dialog("Error", "Failed to load WiFi-SSID");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load WiFi-SSID";
                }
                if (!load_char("WiFi-Password", pass, sizeof(pass)))
                {
                    FURI_LOG_E(TAG, "Failed to load WiFi-Password");
                    easy_flipper_dialog("Error", "Failed to load WiFi-Password");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                    return "Failed to load WiFi-Password";
                }
                save_settings(ssid, pass, username, password);
                model->title = "Fetching World List..";
                return "Account created!";
            }
            else if (strstr(model->fhttp->last_response, "Username or password not provided") != NULL)
            {
                easy_flipper_dialog("Error", "Please enter your credentials.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Please enter your credentials.";
            }
            else if (strstr(model->fhttp->last_response, "User already exists") != NULL || strstr(model->fhttp->last_response, "Multiple users found") != NULL)
            {
                easy_flipper_dialog("Error", "Registration failed...\nUsername already exists.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Username already exists.";
            }
            else
            {
                easy_flipper_dialog("Error", "Registration failed...\nUpdate your credentials.\nPress BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Registration failed...";
            }
        }
        else
        {
            if (!game_thread_start(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher,
                                               FlipWorldViewSubmenu); // just go back to the main menu for now
                return "Failed to start game thread";
            }
            return "Thanks for playing FlipWorld!\n\n\n\nPress BACK to return if this\ndoesn't automatically close.";
        }
    }
    else if (model->request_index == 2)
    {
        return "Welcome to FlipWorld!\n\n\n\nPress BACK to return if this\ndoesn't automatically close.";
    }
    else if (model->request_index == 3)
    {
        if (!game_thread_start(app))
        {
            FURI_LOG_E(TAG, "Failed to start game thread");
            easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
            view_dispatcher_switch_to_view(app->view_dispatcher,
                                           FlipWorldViewSubmenu); // just go back to the main menu for now
            return "Failed to start game thread";
        }
        return "Thanks for playing FlipWorld!\n\n\n\nPress BACK to return if this\ndoesn't automatically close.";
    }
    easy_flipper_dialog("Error", "Unknown error. Press BACK to return.");
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu); // just go back to the main menu for now
    return "Unknown error";
}
static void game_switch_to_view(FlipWorldApp *app)
{
    if (!loader_view_alloc(app))
    {
        FURI_LOG_E(TAG, "Failed to allocate view loader");
        return;
    }
    loader_switch_to_view(app, "Starting Game..", game_fetch, game_parse, 5, callback_to_submenu, FlipWorldViewLoader);
}
bool game_start_ws(FlipperHTTP *fhttp, char *lobby_name)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby_name || strlen(lobby_name) == 0)
    {
        FURI_LOG_E(TAG, "Lobby name is NULL or empty");
        return false;
    }
    fhttp->state = IDLE; // ensure it's set to IDLE for the next request
    char websocket_url[128];
    snprintf(websocket_url, sizeof(websocket_url), "ws://www.jblanked.com/ws/game/%s/", lobby_name);
    if (!flipper_http_websocket_start(fhttp, websocket_url, 80, "{\"Content-Type\":\"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to start websocket");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return true;
}
// load pvp info (this returns the lobbies available)
static bool game_fetch_lobbies(FlipperHTTP *fhttp, uint8_t max_players, uint8_t max_lobbies)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    char lobby_type[4];
    snprintf(lobby_type, sizeof(lobby_type), game_mode_index == 0 ? "pve" : "pvp");
    // ensure flip_world directory exists
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/%s", lobby_type);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/%s/lobbies", lobby_type);
    storage_common_mkdir(storage, directory_path);
    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/%s/%s_lobbies.json", lobby_type, lobby_type);
    storage_simply_remove_recursive(storage, fhttp->file_path); // ensure the file is empty
    furi_record_close(RECORD_STORAGE);
    //
    char url[128];
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/%s/lobbies/%d/%d/", lobby_type, max_players, max_lobbies);
    fhttp->save_received_data = true;
    return flipper_http_request(fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL);
}

static bool game_parse_lobbies(FlipperHTTP *fhttp, uint8_t max_lobbies, void *context)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app);
    // add the lobbies to the submenu
    FuriString *lobbies = flipper_http_load_from_file(fhttp->file_path);
    if (!lobbies)
    {
        FURI_LOG_E(TAG, "Failed to load lobbies");
        return false;
    }

    // parse the lobbies
    for (uint32_t i = 0; i < max_lobbies; i++)
    {
        FuriString *lobby = get_json_array_value_furi("lobbies", i, lobbies);
        if (!lobby)
        {
            FURI_LOG_I(TAG, "No more lobbies");
            break;
        }
        FuriString *lobby_id = get_json_value_furi("id", lobby);
        if (!lobby_id)
        {
            FURI_LOG_E(TAG, "Failed to get lobby id");
            furi_string_free(lobby);
            return false;
        }
        // add the lobby to the submenu
        submenu_add_item(app->submenu_other, furi_string_get_cstr(lobby_id), FlipWorldSubmenuIndexLobby + i, callback_submenu_lobby_pvp_choices, app);
        // add the lobby to the list
        if (!easy_flipper_set_buffer(&lobby_list[i], 64))
        {
            FURI_LOG_E(TAG, "Failed to allocate lobby list");
            furi_string_free(lobby);
            furi_string_free(lobby_id);
            return false;
        }
        snprintf(lobby_list[i], 64, "%s", furi_string_get_cstr(lobby_id));
        furi_string_free(lobby);
        furi_string_free(lobby_id);
    }
    furi_string_free(lobbies);
    return true;
}

static bool game_lobbies(FlipperHTTP *fhttp, uint8_t max_players, uint8_t max_lobbies, void *context)
{
    if (!fhttp || !context)
    {
        FURI_LOG_E(TAG, "FlipperHTTP or context is NULL");
        return false;
    }
    if (!game_fetch_lobbies(fhttp, max_players, max_lobbies))
    {
        FURI_LOG_E(TAG, "Failed to fetch lobbies");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        // Wait for the request to be received
        furi_delay_ms(100);
    }
    return game_parse_lobbies(fhttp, max_lobbies, context);
}

void game_run(FlipWorldApp *app)
{
    furi_check(app, "FlipWorldApp is NULL");
    free_all_views(app, true, true, false);
    // only need to check if they have 30k free (game needs about 12k currently)
    if (!is_enough_heap(30000, false))
    {
        const size_t min_free = memmgr_get_free_heap();
        char message[64];
        snprintf(message, sizeof(message), "Not enough heap memory.\nThere are %zu bytes free.", min_free);
        easy_flipper_dialog("Error", message);
        return;
    }
    // check if logged in
    if (is_logged_in() || is_logged_in_to_flip_social())
    {
        FlipperHTTP *fhttp = flipper_http_alloc();
        if (!fhttp)
        {
            FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
            easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP. Press BACK to return.");
            return;
        }
        bool game_fetch_world_list_i()
        {
            return game_fetch_world_list(fhttp);
        }
        bool parse_world_list_i()
        {
            return fhttp->state != ISSUE;
        }

        bool game_fetch_player_stats_i()
        {
            return game_fetch_player_stats(fhttp);
        }

        if (!alloc_message_view(app, MessageStateLoading))
        {
            FURI_LOG_E(TAG, "Failed to allocate message view");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewMessage);

        // Make the request
        if (game_mode_index == 2) // GAME_MODE_STORY
        {
            if (!flipper_http_process_response_async(fhttp, game_fetch_world_list_i, parse_world_list_i) || !flipper_http_process_response_async(fhttp, game_fetch_player_stats_i, set_player_context))
            {
                FURI_LOG_E(HTTP_TAG, "Failed to make request");
                flipper_http_free(fhttp);
            }

            flipper_http_free(fhttp);

            if (!game_thread_start(app))
            {
                FURI_LOG_E(TAG, "Failed to start game thread");
                easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
                return;
            }
        }
        else
        {
            free_submenu_other(app);
            if (!alloc_submenu_other(app, FlipWorldViewLobby))
            {
                FURI_LOG_E(TAG, "Failed to allocate lobby submenu");
                return;
            }

            // load pvp lobbies and player stats
            if (!game_lobbies(
                    fhttp,
                    game_mode_index == 1 ? 2 : MAX_ENEMIES,
                    10,
                    app))
            {
                // unlike the pve/story, receiving data is necessary
                // so send the user back to the main menu if it fails
                FURI_LOG_E(HTTP_TAG, "Failed to make request");
                easy_flipper_dialog("Error", "Failed to make request. Press BACK to return.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
                flipper_http_free(fhttp);
            }
            else
            {
                flipper_http_free(fhttp);

                // switch to the lobby submenu
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
            }
        }
    }
    else
    {
        game_switch_to_view(app);
    }
}

bool game_fetch_lobby(FlipperHTTP *fhttp, const char *lobby_name)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby_name || strlen(lobby_name) == 0)
    {
        FURI_LOG_E(TAG, "Lobby name is NULL or empty");
        return false;
    }
    char lobby_type[4];
    snprintf(lobby_type, sizeof(lobby_type), game_mode_index == 0 ? "pve" : "pvp");
    char username[64];
    if (!load_char("Flip-Social-Username", username, sizeof(username)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        return false;
    }
    // send the request to fetch the lobby details, with player_username
    char url[128];
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/%s/lobby/get/%s/%s/", lobby_type, lobby_name, username);
    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/pvp/lobbies/%s.json", lobby_name);
    fhttp->save_received_data = true;
    if (!flipper_http_request(fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL))
    {
        FURI_LOG_E(TAG, "Failed to fetch lobby details");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return true;
}
bool game_join_lobby(FlipperHTTP *fhttp, const char *lobby_name)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby_name || strlen(lobby_name) == 0)
    {
        FURI_LOG_E(TAG, "Lobby name is NULL or empty");
        return false;
    }
    char lobby_type[4];
    snprintf(lobby_type, sizeof(lobby_type), game_mode_index == 0 ? "pve" : "pvp");
    char username[64];
    if (!load_char("Flip-Social-Username", username, sizeof(username)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        return false;
    }
    char url[128];
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"username\":\"%s\", \"game_id\":\"%s\"}", username, lobby_name);
    save_char("pvp_lobby_name", lobby_name); // save the lobby name
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/%s/lobby/join/", lobby_type);
    if (!flipper_http_request(fhttp, POST, url, "{\"Content-Type\":\"application/json\"}", payload))
    {
        FURI_LOG_E(TAG, "Failed to join lobby");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return true;
}
static bool game_create_pvp_enemy(FuriString *lobby_details)
{
    if (!lobby_details)
    {
        FURI_LOG_E(TAG, "Failed to load lobby details");
        return false;
    }

    char current_user[64];
    if (!load_char("Flip-Social-Username", current_user, sizeof(current_user)))
    {
        FURI_LOG_E(TAG, "Failed to load Flip-Social-Username");
        save_char("create_pvp_error", "Failed to load Flip-Social-Username");
        return false;
    }

    for (uint8_t i = 0; i < 2; i++)
    {
        // parse the lobby details
        FuriString *player_stats = get_json_array_value_furi("player_stats", i, lobby_details);
        if (!player_stats)
        {
            FURI_LOG_E(TAG, "Failed to get player stats");
            save_char("create_pvp_error", "Failed to get player stats array");
            return false;
        }

        // available keys from player_stats
        FuriString *username = get_json_value_furi("username", player_stats);
        if (!username)
        {
            FURI_LOG_E(TAG, "Failed to get username");
            save_char("create_pvp_error", "Failed to get username");
            furi_string_free(player_stats);
            return false;
        }

        // check if the username is the same as the current user
        if (is_str(furi_string_get_cstr(username), current_user))
        {
            furi_string_free(player_stats);
            furi_string_free(username);
            continue; // skip the current user
        }

        FuriString *strength = get_json_value_furi("strength", player_stats);
        FuriString *health = get_json_value_furi("health", player_stats);
        FuriString *attack_timer = get_json_value_furi("attack_timer", player_stats);

        if (!strength || !health || !attack_timer)
        {
            FURI_LOG_E(TAG, "Failed to get player stats");
            save_char("create_pvp_error", "Failed to get player stats");
            furi_string_free(player_stats);
            furi_string_free(username);
            if (strength)
                furi_string_free(strength);
            if (health)
                furi_string_free(health);
            if (attack_timer)
                furi_string_free(attack_timer);
            return false;
        }

        // create enemy data
        FuriString *enemy_data = furi_string_alloc();
        furi_string_printf(
            enemy_data,
            "{\"enemy_data\":[{\"id\":\"sword\",\"is_user\":\"true\",\"username\":\"%s\","
            "\"index\":0,\"start_position\":{\"x\":350,\"y\":210},\"end_position\":{\"x\":350,\"y\":210},"
            "\"move_timer\":1,\"speed\":1,\"attack_timer\":%f,\"strength\":%f,\"health\":%f}]}",
            furi_string_get_cstr(username),
            (double)atof_furi(attack_timer),
            (double)atof_furi(strength),
            (double)atof_furi(health));

        char directory_path[128];
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
        Storage *storage = furi_record_open(RECORD_STORAGE);
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/pvp_world");
        storage_common_mkdir(storage, directory_path);
        furi_record_close(RECORD_STORAGE);

        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/pvp_world/pvp_world_enemy_data.json");

        // remove the enemy_data file if it exists
        storage_simply_remove_recursive(storage, directory_path);

        File *file = storage_file_alloc(storage);
        if (!storage_file_open(file, directory_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        {
            FURI_LOG_E("Game", "Failed to open file for writing: %s", directory_path);
            save_char("create_pvp_error", "Failed to open file for writing");
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            furi_string_free(enemy_data);
            furi_string_free(player_stats);
            furi_string_free(username);
            furi_string_free(strength);
            furi_string_free(health);
            furi_string_free(attack_timer);
            return false;
        }

        size_t data_size = furi_string_size(enemy_data);
        if (storage_file_write(file, furi_string_get_cstr(enemy_data), data_size) != data_size)
        {
            FURI_LOG_E("Game", "Failed to write enemy_data");
            save_char("create_pvp_error", "Failed to write enemy_data");
        }
        storage_file_close(file);

        furi_string_free(enemy_data);
        furi_string_free(player_stats);
        furi_string_free(username);
        furi_string_free(strength);
        furi_string_free(health);
        furi_string_free(attack_timer);

        // player is found so break
        break;
    }

    return true;
}

size_t game_lobby_count(FlipperHTTP *fhttp, FuriString *lobby)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return -1;
    }
    if (!lobby)
    {
        FURI_LOG_E(TAG, "Lobby details are NULL");
        return -1;
    }
    // check if the player is in the lobby
    FuriString *player_count = get_json_value_furi("player_count", lobby);
    if (!player_count)
    {
        FURI_LOG_E(TAG, "Failed to get player count");
        return -1;
    }
    const size_t count = atoi(furi_string_get_cstr(player_count));
    furi_string_free(player_count);
    return count;
}
bool game_in_lobby(FlipperHTTP *fhttp, FuriString *lobby)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if (!lobby)
    {
        FURI_LOG_E(TAG, "Lobby details are NULL");
        return false;
    }
    // check if the player is in the lobby
    FuriString *is_in_game = get_json_value_furi("is_in_game", lobby);
    if (!is_in_game)
    {
        FURI_LOG_E(TAG, "Failed to get is_in_game");
        furi_string_free(is_in_game);
        return false;
    }
    const bool in_game = is_str(furi_string_get_cstr(is_in_game), "true");
    furi_string_free(is_in_game);
    return in_game;
}

// this will free both the fhttp and lobby
void game_start_game(FlipperHTTP *fhttp, FuriString *lobby, void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");

    if (game_mode_index == 1) // pvp (in pve, we will apennd the enemies as they are read from the websocket)
    {
        // only thing left to do is create the enemy data and start the websocket session
        if (!game_create_pvp_enemy(lobby))
        {
            FURI_LOG_E(TAG, "Failed to create pvp enemy context.");
            easy_flipper_dialog("Error", "Failed to create pvp enemy context. Press BACK to return.");
            flipper_http_free(fhttp);
            furi_string_free(lobby);
            return;
        }
    }
    furi_string_free(lobby);

    // used later in PVE mode if needed to fetch worlds
    snprintf(game_ws_lobby_name, sizeof(game_ws_lobby_name), "%s", lobby_list[lobby_index]);

    // start the websocket session
    if (!game_start_ws(fhttp, game_ws_lobby_name))
    {
        FURI_LOG_E(TAG, "Failed to start websocket session");
        easy_flipper_dialog("Error", "Failed to start websocket session. Press BACK to return.");
        flipper_http_free(fhttp);
        return;
    }

    flipper_http_free(fhttp);

    // start the game thread
    if (!game_thread_start(app))
    {
        FURI_LOG_E(TAG, "Failed to start game thread");
        easy_flipper_dialog("Error", "Failed to start game thread. Press BACK to return.");
        return;
    }
};
void game_waiting_process(FlipperHTTP *fhttp, void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        return;
    }
    // fetch the lobby details
    if (!game_fetch_lobby(fhttp, lobby_list[lobby_index]))
    {
        FURI_LOG_E(TAG, "Failed to fetch lobby details");
        flipper_http_free(fhttp);
        easy_flipper_dialog("Error", "Failed to fetch lobby details. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        return;
    }
    // load the lobby details
    FuriString *lobby = flipper_http_load_from_file(fhttp->file_path);
    if (!lobby)
    {
        FURI_LOG_E(TAG, "Failed to load lobby details");
        flipper_http_free(fhttp);
        easy_flipper_dialog("Error", "Failed to load lobby details. Press BACK to return.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenuOther);
        return;
    }
    // get the player count
    const size_t count = game_lobby_count(fhttp, lobby);
    if (count == 2)
    {
        // break out of this and start the game
        game_start_game(fhttp, lobby, app); // this will free both the fhttp and lobby
        return;
    }
    furi_string_free(lobby);
}

void game_waiting_lobby(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "waiting_lobby: FlipWorldApp is NULL");

    if (!game_start_waiting_thread(app))
    {
        FURI_LOG_E(TAG, "Failed to start waiting thread");
        easy_flipper_dialog("Error", "Failed to start waiting thread. Press BACK to return.");
        return;
    }

    free_message_view(app);
    if (!alloc_message_view(app, MessageStateWaitingLobby))
    {
        FURI_LOG_E(TAG, "Failed to allocate message view");
        return;
    }
    // finally, switch to the waiting lobby view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewMessage);
};

bool game_remove_from_lobby(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    char username[32];
    if (!load_char("Flip-Social-Username", username, sizeof(username)))
    {
        FURI_LOG_E(TAG, "Failed to load data/Flip-Social-Username");
        return false;
    }
    char lobby_type[4];
    snprintf(lobby_type, sizeof(lobby_type), game_mode_index == 0 ? "pve" : "pvp");
    char url[128];
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"username\":\"%s\", \"game_id\":\"%s\"}", username, game_ws_lobby_name);
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/%s/lobby/remove/", lobby_type);
    fhttp->state = IDLE;
    if (!flipper_http_request(fhttp, POST, url, "{\"Content-Type\":\"application/json\"}", payload))
    {
        FURI_LOG_E(TAG, "Failed to remove player from lobby");
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return true;
}
