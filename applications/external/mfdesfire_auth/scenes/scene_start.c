#include "mfdesfire_auth_i.h"

// Icon data for 10px icon for filebrowser system in start scene
const uint8_t icon_data[] = {0x00, 0x80, 0x00, 0x02, 0x01, 0x1E, 0x02, 0x24, 0x02, 0x44, 0x02,
                             0x44, 0x02, 0x44, 0x02, 0x44, 0x02, 0x3E, 0x01, 0x80, 0x00};
// Pointers to frame data
const uint8_t* const icon_frames_10px[] = {icon_data};

typedef enum {
    MfDesfireAuthSubmenuSetIV,
    MfDesfireAuthSubmenuSetKey,
    MfDesfireAuthSubmenuAuthenticate,
} MfDesfireAuthSubmenuIndex;

const Icon submenu_icon = {
    .width = 10,
    .height = 10,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = icon_frames_10px,
};

/**
 * @brief Callback funkce volaná po výběru souboru v FileBrowseru.
 */
static void mfdes_file_select_callback(void* context) {
    furi_assert(context);
    MfDesApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    // Try to load settings for selected card
    if(!mfdesfire_auth_load_settings(instance)) {
        // If no settings found, initialize with zeros
        memset(instance->initial_vector, 0, INITIAL_VECTOR_SIZE);
        memset(instance->key, 0, KEY_SIZE);
    }

    // view_dispatcher_switch_to_view(instance->view_dispatcher, MfDesAppViewBrowser);
    scene_manager_next_scene(scene_manager, MfDesAppViewSubmenu);
}

void desfire_app_scene_start_on_enter(void* context) {
    furi_assert(context);
    MfDesApp* instance = context;

    // Nastavení FileBrowseru
    file_browser_set_callback(
        instance->file_browser, mfdes_file_select_callback, instance); // Select v file browseru
    file_browser_configure(instance->file_browser, ".nfc", NFC_CARDS_PATH, 1, 1, &submenu_icon, 1);
    file_browser_start(instance->file_browser, furi_string_alloc_set(NFC_CARDS_PATH));

    view_dispatcher_switch_to_view(instance->view_dispatcher, MfDesAppViewBrowser);
}

bool desfire_app_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed =
        false; // Protoze tady neresim zadne eventy sama, tak je necham handlovat automaticky
    return consumed;
}

void desfire_app_scene_start_on_exit(void* context) {
    UNUSED(context);
}
