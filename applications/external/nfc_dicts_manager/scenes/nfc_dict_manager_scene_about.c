#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"
#include <gui/icon.h>
#include <gui/elements.h>

// Disegna l'immagine Credits_96x64 sulla scena About
static void nfc_dict_manager_scene_about_draw(Canvas* canvas, void* context) {
    UNUSED(context);
    // Centra l'immagine sul canvas (128x64). L'immagine è 96x64.
    int x = (128 - 96) / 2;
    int y = 0;
    canvas_draw_icon(canvas, x, y, &I_Credits_96x64);
}

void nfc_dict_manager_scene_about_on_enter(void* context) {
    NfcDictManager* app = context;

    // Crea una view temporanea solo per About
    View* view = view_alloc();
    view_set_context(view, app);
    view_set_draw_callback(view, nfc_dict_manager_scene_about_draw);

    // Registra la view nel dispatcher
    view_dispatcher_add_view(app->view_dispatcher, NfcDictManagerViewAbout, view);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewAbout);
}

void nfc_dict_manager_scene_about_on_exit(void* context) {
    NfcDictManager* app = context;
    view_dispatcher_remove_view(app->view_dispatcher, NfcDictManagerViewAbout);
}

bool nfc_dict_manager_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
