#include "dnie_reader.h"
#include "dnie_sounds.h"
#include "scenes/dnie_reader_scene.h"

#include <furi_hal_rtc.h>

// --- Callbacks de navegacion del ViewDispatcher ---

static bool dnie_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    DnieApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool dnie_back_event_callback(void* context) {
    furi_assert(context);
    DnieApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// --- Ciclo de vida de la app ---

static DnieApp* dnie_app_alloc(void) {
    DnieApp* app = malloc(sizeof(DnieApp));
    memset(app, 0, sizeof(DnieApp));

    app->nfc = nfc_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&dnie_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, dnie_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, dnie_back_event_callback);

    // Modulos GUI y su registro como Views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DnieViewSubmenu, submenu_get_view(app->submenu));

    app->can_view = dnie_can_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, DnieViewCan, app->can_view);

    app->settings = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DnieViewSettings, variable_item_list_get_view(app->settings));
    dnie_sound_volume_load();
    dnie_photo_wipe(); // restos de versiones antiguas o de un cierre inesperado

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, DnieViewWidget, widget_get_view(app->widget));

    app->card_view = view_alloc();
    view_allocate_model(app->card_view, ViewModelTypeLockFree, sizeof(void*));
    view_dispatcher_add_view(app->view_dispatcher, DnieViewCard, app->card_view);

    return app;
}

static void dnie_app_free(DnieApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, DnieViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, DnieViewCan);
    view_dispatcher_remove_view(app->view_dispatcher, DnieViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, DnieViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, DnieViewCard);
    view_free(app->card_view);
    if(app->toast_timer) {
        furi_timer_stop(app->toast_timer);
        furi_timer_free(app->toast_timer);
    }
    free(app->saved_entries);
    dnie_auth_reset(&app->auth);
    if(app->pix_src) {
        memset(app->pix_src, 0, sizeof(DniePixelSource));
        free(app->pix_src);
    }

    submenu_free(app->submenu);
    view_free(app->can_view);
    widget_free(app->widget);
    variable_item_list_free(app->settings);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    nfc_free(app->nfc);
    dnie_sound_stop(); // corta el himno al instante
    dnie_photo_wipe(); // por si quedo alguna foto en claro
    furi_record_close(RECORD_NOTIFICATION);
    memset(app, 0, sizeof(DnieApp)); // borra datos personales y claves de la RAM
    free(app);
}

// --- Utilidades compartidas ---

void dnie_birth_full(const char* yymmdd, char out[9]) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    int yy = (yymmdd[0] - '0') * 10 + (yymmdd[1] - '0');
    // La MRZ solo trae 2 cifras: si son mayores que el ano actual, es del siglo pasado
    int year = (yy > now.year % 100 ? 1900 : 2000) + yy;
    snprintf(out, 9, "%04d%c%c%c%c", year, yymmdd[2], yymmdd[3], yymmdd[4], yymmdd[5]);
}

// --- Punto de entrada declarado en application.fam ---

int32_t dnie_reader_app(void* p) {
    UNUSED(p);
    DnieApp* app = dnie_app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, DnieSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    dnie_app_free(app);
    return 0;
}
