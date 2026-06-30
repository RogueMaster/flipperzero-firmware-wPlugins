#include "../subghz_playlist_creator.h"
#include "scene_popup.h"
void scene_popup_show(SubGhzPlaylistCreator* app, const char* header, const char* text) {
    app->current_view = SubGhzPlaylistCreatorViewPopup;
    popup_set_header(app->popup, header, 64, 0, AlignCenter, AlignTop);
    popup_set_text(app->popup, text, 64, 32, AlignCenter, AlignCenter);
    popup_set_callback(app->popup, NULL);
    popup_set_context(app->popup, NULL);
    popup_set_timeout(app->popup, 2000);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewPopup);
} 