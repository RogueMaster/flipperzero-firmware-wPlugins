#include "../subghz_playlist_creator.h"
#include "scene_menu.h"
#define TAG "PlaylistMenuScene"

void scene_menu_show(SubGhzPlaylistCreator* app) {
    app->current_view = SubGhzPlaylistCreatorViewSubmenu;
    FURI_LOG_D(TAG, "Showing menu view. Dispatcher: %p, Submenu View: %p, ViewId: %lu", app->view_dispatcher, submenu_get_view(app->submenu), (uint32_t)SubGhzPlaylistCreatorViewSubmenu);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewSubmenu);
}

// Add the definition for scene_menu_init_view
void scene_menu_init_view(SubGhzPlaylistCreator* app) {
    // The view (submenu in this case) is allocated in subghz_playlist_creator_alloc
    // And added to the dispatcher there.
    // This function can remain empty for now if allocation and adding are done elsewhere.
    UNUSED(app);
} 