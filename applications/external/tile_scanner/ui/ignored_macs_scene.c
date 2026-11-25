#include "ui.h"

#define LOG_TAG "tile_scanner_ignored_macs_scene"

void ignored_macs_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    furi_string_set_str(ui->ignored_macs_scene->display_text, "Ignored MACs:");

    for(int i = 0; i < ui->known_mac_index; i++) {
        furi_string_cat_printf(
            ui->ignored_macs_scene->display_text,
            "\n%02X:%02X:%02X:%02X:%02X:%02X",
            ui->known_macs[i].mac_address[0],
            ui->known_macs[i].mac_address[1],
            ui->known_macs[i].mac_address[2],
            ui->known_macs[i].mac_address[3],
            ui->known_macs[i].mac_address[4],
            ui->known_macs[i].mac_address[5]);
    }

    text_box_set_text(
        ui->ignored_macs_scene->ignored_macs_text_box,
        furi_string_get_cstr(ui->ignored_macs_scene->display_text));
    view_dispatcher_switch_to_view(ui->view_dispatcher, View_IgnoredMacsDisplay);
}

bool ignored_macs_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UNUSED(event);

    // Default event handler
    return false;
}

void ignored_macs_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    text_box_reset(ui->ignored_macs_scene->ignored_macs_text_box);
}

IgnoredMacsScene* ignored_macs_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    IgnoredMacsScene* ignored_macs_scene = malloc(sizeof(IgnoredMacsScene));
    ignored_macs_scene->display_text = furi_string_alloc();
    ignored_macs_scene->ignored_macs_text_box = text_box_alloc();

    return ignored_macs_scene;
}

void ignored_macs_scene_free(IgnoredMacsScene* ignored_macs_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ignored_macs_scene);

    text_box_free(ignored_macs_scene->ignored_macs_text_box);
    furi_string_free(ignored_macs_scene->display_text);
    free(ignored_macs_scene);
}
