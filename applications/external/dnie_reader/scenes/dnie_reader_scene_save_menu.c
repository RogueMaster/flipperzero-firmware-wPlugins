#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

#include <furi_hal_rtc.h>

typedef enum {
    SaveIndexFull,
    SaveIndexAlbum,
} SaveIndex;

static void dnie_scene_save_menu_callback(void* context, uint32_t index) {
    DnieApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dnie_scene_save_menu_on_enter(void* context) {
    DnieApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Save reading");
    submenu_add_item(
        submenu, "Full ID (encrypted)", SaveIndexFull, dnie_scene_save_menu_callback, app);
    submenu_add_item(
        submenu, "Name, photo & birthday", SaveIndexAlbum, dnie_scene_save_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewSubmenu);
}

static DnieStoreResult save_current(DnieApp* app, DnieRecordKind kind) {
    DnieRecord* rec = malloc(sizeof(DnieRecord));
    memset(rec, 0, sizeof(DnieRecord));
    rec->kind = kind;
    rec->saved_at = furi_hal_rtc_get_timestamp();
    if(kind == DnieRecordFull) {
        rec->dg1_len = (uint8_t)app->dg1_len;
        memcpy(rec->dg1, app->dg1, app->dg1_len);
    } else {
        // Solo lo necesario para recordar a la persona: nada de numeros de documento
        strlcpy(rec->given_names, app->mrz.given_names, sizeof(rec->given_names));
        strlcpy(rec->surnames, app->mrz.surnames, sizeof(rec->surnames));
        dnie_birth_full(app->mrz.birth, rec->birth);
        strlcpy(rec->issuer, app->mrz.issuer, sizeof(rec->issuer));
    }
    if(app->photo_decode_result == DnieJ2kOk && app->pix_src) {
        rec->has_photo = true;
        rec->photo = *app->pix_src;
    }
    DnieStoreResult r = dnie_store_save(rec);
    memset(rec, 0, sizeof(DnieRecord));
    free(rec);
    return r;
}

bool dnie_scene_save_menu_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != SaveIndexFull && event.event != SaveIndexAlbum) return false;

    DnieStoreResult r =
        save_current(app, event.event == SaveIndexFull ? DnieRecordFull : DnieRecordAlbum);
    if(r == DnieStoreOk) {
        notification_message(app->notifications, &sequence_success);
        app->pending_toast = event.event == SaveIndexFull ? "ID saved" : "Saved to album";
    } else {
        notification_message(app->notifications, &sequence_error);
        app->pending_toast = r == DnieStoreErrorFull ? "Full (20 max)" : "Error saving";
    }
    scene_manager_previous_scene(app->scene_manager); // de vuelta al carnet
    return true;
}

void dnie_scene_save_menu_on_exit(void* context) {
    DnieApp* app = context;
    submenu_reset(app->submenu);
}
