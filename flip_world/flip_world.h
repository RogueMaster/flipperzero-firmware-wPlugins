#pragma once
#include <easy_flipper/easy_flipper.h>
#include <flipper_http/flipper_http.h>
#include <font/font.h>

// added by Derek Jamison to lower memory usage
#undef FURI_LOG_E
#define FURI_LOG_E(tag, msg, ...)

#undef FURI_LOG_I
#define FURI_LOG_I(tag, msg, ...)

#undef FURI_LOG_D
#define FURI_LOG_D(tag, msg, ...)
//

#define TAG "FlipWorld"
#define VERSION "0.8.3"
#define VERSION_TAG TAG " " FAP_VERSION
//

// Define the submenu items for our FlipWorld application
typedef enum
{
    FlipWorldSubmenuIndexPvE,
    FlipWorldSubmenuIndexStory,
    FlipWorldSubmenuIndexPvP,
    //
    FlipWorldSubmenuIndexGameSubmenu,
    FlipWorldSubmenuIndexMessage,
    FlipWorldSubmenuIndexSettings,
    FlipWorldSubmenuIndexWiFiSettings,
    FlipWorldSubmenuIndexGameSettings,
    FlipWorldSubmenuIndexUserSettings,
    FlipWorldSubmenuIndexLobby,
} FlipWorldSubmenuIndex;

// Define a single view for our FlipWorld application
typedef enum
{
    FlipWorldViewSubmenu,          // The submenu
    FlipWorldViewGameSubmenu,      // The game submenu
    FlipWorldViewSubmenuOther,     // The submenu used by settings and lobby
    FlipWorldViewMessage,          // The about, loading screen
    FlipWorldViewSettings,         // The settings screen
    FlipWorldViewLobby,            // The lobby screen
    FlipWorldViewWaitingLobby,     // The waiting lobby screen
    FlipWorldViewVariableItemList, // The variable item list screen
    FlipWorldViewTextInput,        // The text input screen
    //
    FlipWorldViewWidgetResult, // The text box that displays the random fact
    FlipWorldViewLoader,       // The loader screen retrieves data from the internet
} FlipWorldView;

// Define a custom event for our FlipWorld application
typedef enum
{
    FlipWorldCustomEventProcess,
    FlipWorldCustomEventPlay, // Play the game
} FlipWorldCustomEvent;

// Each screen will have its own view
typedef struct
{
    View *view_loader;
    Widget *widget_result;
    //
    ViewDispatcher *view_dispatcher;       // Switches between our views
    View *view_message;                    // The about, loading screen
    Submenu *submenu;                      // The submenu
    Submenu *submenu_game;                 // The game submenu
    Submenu *submenu_other;                // submenu used by settings and lobby
    VariableItemList *variable_item_list;  // The variable item list (settngs)
    VariableItem *variable_item_wifi_ssid; // The variable item for WiFi SSID
    VariableItem *variable_item_wifi_pass; // The variable item for WiFi password
    //
    VariableItem *variable_item_game_fps;              // The variable item for Game FPS
    VariableItem *variable_item_game_screen_always_on; // The variable item for Screen always on
    VariableItem *variable_item_game_download_world;   // The variable item for Download world
    VariableItem *variable_item_game_sound_on;         // The variable item for Sound on
    VariableItem *variable_item_game_vibration_on;     // The variable item for Vibration on
    VariableItem *variable_item_game_player_sprite;    // The variable item for Player sprite
    VariableItem *variable_item_game_vgm_x;            // The variable item for VGM X
    VariableItem *variable_item_game_vgm_y;            // The variable item for VGM Y
    //
    VariableItem *variable_item_user_username; // The variable item for the User username
    VariableItem *variable_item_user_password; // The variable item for the User password

    TextInput *text_input;      // The text input
    char *text_input_buffer;         // Buffer for the text input
    char *text_input_temp_buffer;    // Temporary buffer for the text input
    uint32_t text_input_buffer_size; // Size of the text input buffer
    //
} FlipWorldApp;

extern char *fps_choices_str[];
extern uint8_t fps_index;
extern char *yes_or_no_choices[];
extern uint8_t screen_always_on_index;
extern uint8_t sound_on_index;
extern uint8_t vibration_on_index;
extern char *player_sprite_choices[];
extern uint8_t player_sprite_index;
extern char *vgm_levels[];
extern uint8_t vgm_x_index;
extern uint8_t vgm_y_index;
extern uint8_t game_mode_index;
float atof_(const char *nptr);
float atof_furi(const FuriString *nptr);
bool is_str(const char *src, const char *dst);
bool is_enough_heap(size_t heap_size, bool check_blocks);
