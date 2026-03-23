#include "../metroflip_i.h"
#include <dolphin/dolphin.h>
#include "../api/metroflip/metroflip_api.h"

#define TAG "Metroflip:Scene:SupportedCards"

static void metroflip_scene_supported_submenu_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

void metroflip_scene_supported_on_enter(void* context) {
    Metroflip* app = context;
    Submenu* submenu = app->submenu;

    dolphin_deed(DolphinDeedNfcReadSuccess);

    submenu_set_header(submenu, "Supported Cards");

    /* Calypso */
    submenu_add_item(
        submenu, "Navigo (Paris)", 0, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "Opus (Montreal)", 1, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "Rav-Kav (Israel)", 2, metroflip_scene_supported_submenu_callback, app);

    /* MIFARE Classic */
    submenu_add_item(
        submenu, "Bip! (Santiago)", 3, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "Charliecard (Boston)", 4, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "MetroMoney (Tbilisi)", 5, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "SmartRider (Perth)", 6, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "Troika (Moscow)", 7, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "RENFE Suma 10 (Spain)", 8, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "RENFE Regular (Spain)", 9, metroflip_scene_supported_submenu_callback, app);

    /* MIFARE DESFire */
    submenu_add_item(
        submenu, "Clipper (San Francisco)", 10, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "ITSO (United Kingdom)", 11, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "myki (Melbourne)", 12, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(submenu, "Nol (Dubai)", 13, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "Opal (Sydney)", 14, metroflip_scene_supported_submenu_callback, app);

    /* FeliCa */
    submenu_add_item(
        submenu, "Suica (Japan)", 15, metroflip_scene_supported_submenu_callback, app);

    /* Other */
    submenu_add_item(
        submenu, "GoCard (Brisbane)", 16, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "Intertic (France)", 17, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "T-Mobilitat (Catalonia)", 18, metroflip_scene_supported_submenu_callback, app);
    submenu_add_item(
        submenu, "TRT (Tianjin)", 19, metroflip_scene_supported_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewSubmenu);
}

bool metroflip_scene_supported_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }
    return consumed;
}

void metroflip_scene_supported_on_exit(void* context) {
    Metroflip* app = context;
    submenu_reset(app->submenu);
}
