#include "../mizip_balance_editor_i.h"

void mizip_balance_editor_scene_about_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    text_box_set_text(
        app->text_box,
        "MiZip balance editor allows you to easily modify your MiZip tags and files.\n\nSource code available at github.com/teohumeau/MiZipBalanceEditor\n\nHuge thanks to xVergil for his work on tag reading/writing!");

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdAbout);
}

bool mizip_balance_editor_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void mizip_balance_editor_scene_about_on_exit(void* context) {
    UNUSED(context);
}
