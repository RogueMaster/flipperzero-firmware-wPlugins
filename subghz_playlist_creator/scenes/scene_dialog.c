#include "../subghz_playlist_creator.h"
#include "scene_dialog.h"
#include <gui/modules/dialog_ex.h>

void scene_dialog_show_custom(
    SubGhzPlaylistCreator* app,
    const char* header,
    const char* text,
    const char* left_btn,
    const char* right_btn,
    DialogExResultCallback callback,
    void* context
) {
    app->current_view = SubGhzPlaylistCreatorViewDialog;
    dialog_ex_set_header(app->dialog, header, 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, text, 64, 12, AlignCenter, AlignTop);
    dialog_ex_set_icon(app->dialog, 0, 0, NULL);
    dialog_ex_set_left_button_text(app->dialog, left_btn);
    dialog_ex_set_right_button_text(app->dialog, right_btn);
    dialog_ex_set_context(app->dialog, context);
    dialog_ex_set_result_callback(app->dialog, callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewDialog);
}

void scene_dialog_show(SubGhzPlaylistCreator* app) {
    app->current_view = SubGhzPlaylistCreatorViewDialog;
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewDialog);
}

void scene_dialog_init_view(SubGhzPlaylistCreator* app) {
    UNUSED(app);
} 