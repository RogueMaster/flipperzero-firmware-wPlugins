#include "../dnie_reader.h"
#include "dnie_reader_scene.h"
#include "../dnie_sounds.h"

#define SAVED_EMPTY_INDEX 0xFFFF

static void dnie_scene_saved_list_callback(void* context, uint32_t index) {
    DnieApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dnie_scene_saved_list_on_enter(void* context) {
    DnieApp* app = context;
    dnie_sound_stop(); // por si venimos de un carnet con el himno sonando
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Saved readings");

    // La lista solo ocupa memoria mientras se ve (unos 4 KB)
    if(!app->saved_entries) app->saved_entries = malloc(sizeof(DnieSavedEntry) * DNIE_STORE_MAX);
    // Se descifra cada lectura para mostrar el nombre (puede tardar un instante)
    app->saved_count = app->saved_entries ? dnie_store_list(app->saved_entries, DNIE_STORE_MAX) :
                                            0;
    for(size_t i = 0; i < app->saved_count; i++) {
        submenu_add_item(
            submenu, app->saved_entries[i].label, i, dnie_scene_saved_list_callback, app);
    }
    if(app->saved_count == 0) {
        submenu_add_item(
            submenu, "(none yet)", SAVED_EMPTY_INDEX, dnie_scene_saved_list_callback, app);
    }
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DnieSceneSavedList));
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewSubmenu);
}

// Carga una lectura guardada en la app como si se acabara de leer
static bool load_saved(DnieApp* app, size_t index) {
    DnieRecord* rec = malloc(sizeof(DnieRecord));
    if(!rec) return false;
    bool ok = dnie_store_load(app->saved_entries[index].path, rec) == DnieStoreOk &&
              rec->dg1_len <= sizeof(app->dg1);
    if(ok) {
        app->viewing_album = rec->kind == DnieRecordAlbum;
        if(app->viewing_album) {
            memset(app->dg1, 0, sizeof(app->dg1));
            app->dg1_len = 0;
            memset(&app->mrz, 0, sizeof(app->mrz));
            app->mrz_ok = false;
            strlcpy(app->album_given, rec->given_names, sizeof(app->album_given));
            strlcpy(app->album_surnames, rec->surnames, sizeof(app->album_surnames));
            memcpy(app->album_birth, rec->birth, sizeof(app->album_birth));
            strlcpy(app->album_issuer, rec->issuer, sizeof(app->album_issuer));
        } else {
            memcpy(app->dg1, rec->dg1, rec->dg1_len);
            app->dg1_len = rec->dg1_len;
            app->mrz_ok = dnie_mrz_parse_dg1(app->dg1, app->dg1_len, &app->mrz);
        }
        app->saved_at = rec->saved_at;
        strlcpy(app->saved_alias, rec->alias, sizeof(app->saved_alias));
        strlcpy(app->saved_path, app->saved_entries[index].path, sizeof(app->saved_path));

        app->viewing_saved = true;
        app->demo = false;
        app->photo_decode_tried = true; // la foto viene ya escalada: no hay que decodificar
        memset(&app->pix, 0, sizeof(app->pix));
        if(rec->has_photo) {
            if(!app->pix_src) app->pix_src = malloc(sizeof(DniePixelSource));
            if(app->pix_src) {
                *app->pix_src = rec->photo;
                app->photo_decode_result =
                    dnie_pixelart_dither(app->pix_src, app->dither, &app->pix) ?
                        DnieJ2kOk :
                        DnieJ2kErrorMemory;
            }
        } else {
            app->photo_decode_result = DnieJ2kErrorFormat;
        }
        ok = app->viewing_album || app->mrz_ok;
    }
    memset(rec, 0, sizeof(DnieRecord));
    free(rec);
    return ok;
}

bool dnie_scene_saved_list_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->saved_count) {
            scene_manager_set_scene_state(app->scene_manager, DnieSceneSavedList, event.event);
            if(load_saved(app, event.event)) {
                // El himno de su pais al abrirla
                const char* issuer = app->viewing_album ? app->album_issuer : app->mrz.issuer;
                if(issuer[0]) dnie_sound_original(app->notifications, issuer);
                scene_manager_next_scene(app->scene_manager, DnieSceneCard);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
        }
        consumed = true;
    }
    return consumed;
}

void dnie_scene_saved_list_on_exit(void* context) {
    DnieApp* app = context;
    submenu_reset(app->submenu);
    free(app->saved_entries); // se vuelve a listar al volver a esta pantalla
    app->saved_entries = NULL;
}
