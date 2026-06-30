#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/file_browser.h>
#include <storage/storage.h>
#define MAX_TEXT_LENGTH 128

// Forward declare struct for callback
struct SubGhzPlaylistCreator;
typedef void (*SceneFileBrowserSelectCallback)(struct SubGhzPlaylistCreator* app, const char* path);

// Forward declare VariableItemList and DialogEx for struct members.
struct VariableItemList;
struct DialogEx;

// Add ReturnScene enum before struct definition
typedef enum {
    ReturnScene_None = 0,
    ReturnScene_Menu,
    ReturnScene_PlaylistEdit,
    ReturnScene_TextInput,
} ReturnScene;

typedef struct SubGhzPlaylistCreator {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Submenu* playlist_edit_submenu;
    Popup* popup;
    TextInput* text_input;
    DialogEx* dialog;
    FileBrowser* file_browser;
    FuriString* file_browser_result;
    FuriString* playlist_name;
    FuriString* playlist_path;
    char text_buffer[MAX_TEXT_LENGTH];
    SceneFileBrowserSelectCallback file_browser_select_cb;
    int current_view;
    // Playlist editing scene state
    char** playlist_entries;
    size_t playlist_entry_count;
    size_t playlist_entry_capacity;
    bool playlist_modified;
    bool is_stopped;
    FuriTimer* popup_timer;
    struct VariableItemList* playlist_edit_list;
    ReturnScene return_scene; // Add this field
} SubGhzPlaylistCreator;


typedef enum {
    SubGhzPlaylistCreatorViewSubmenu,
    SubGhzPlaylistCreatorViewPopup,
    SubGhzPlaylistCreatorViewTextInput,
    SubGhzPlaylistCreatorViewDialog,
    SubGhzPlaylistCreatorViewFileBrowser,
    SubGhzPlaylistCreatorViewPlaylistEdit,
} SubGhzPlaylistCreatorView;

// Custom events
typedef enum {
    SubGhzPlaylistCreatorCustomEventShowMenu = 100, // Start custom events from 100 or higher
} SubGhzPlaylistCreatorCustomEvent;

// Function declarations
SubGhzPlaylistCreator* subghz_playlist_creator_alloc(void);
void subghz_playlist_creator_free(SubGhzPlaylistCreator* app);
bool subghz_playlist_creator_custom_callback(void* context, uint32_t custom_event);
bool subghz_playlist_creator_back_event_callback(void* context);
int32_t subghz_playlist_creator_app(void* p);
// Playlist editing scene
void scene_playlist_edit_show(SubGhzPlaylistCreator* app);