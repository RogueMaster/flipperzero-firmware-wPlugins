#include "../subghz_playlist_creator.h"
#include "scene_text_input.h"
void scene_text_input_show(SubGhzPlaylistCreator* app) {
    app->current_view = SubGhzPlaylistCreatorViewTextInput;
    memset(app->text_buffer, 0, MAX_TEXT_LENGTH);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewTextInput);
}

// Add the definition for scene_text_input_init_view
void scene_text_input_init_view(SubGhzPlaylistCreator* app) {
    // The view is allocated in subghz_playlist_creator_alloc and added to the dispatcher there.
    UNUSED(app);
} 