#include "../nfc_eink_app_i.h"

#define TAG "NfcEinkSceneChooseScreen"

static void nfc_eink_scene_choose_screen_submenu_callback(void* context, uint32_t index) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

static uint8_t nfc_eink_screen_info_filter_by_mode(NfcEinkApp* instance) {
    uint8_t cnt = 0;
    switch(instance->settings.write_mode) {
    case NfcEinkWriteModeStrict:
        cnt = nfc_eink_descriptor_filter_by_name(instance->arr, instance->info_temp->name);
        break;
    case NfcEinkWriteModeResolution:
        cnt = nfc_eink_descriptor_filter_by_screen_size(
            instance->arr, instance->info_temp->screen_size);
        break;
    case NfcEinkWriteModeManufacturer:
        cnt = nfc_eink_descriptor_filter_by_manufacturer(
            instance->arr, instance->info_temp->screen_manufacturer);
        break;
    case NfcEinkWriteModeFree:
    default:
        cnt = nfc_eink_descriptor_get_all_usable(instance->arr);
        break;
    }
    return cnt;
}

void nfc_eink_scene_choose_screen_on_enter(void* context) {
    NfcEinkApp* instance = context;
    Submenu* submenu = instance->submenu;

    submenu_set_header(submenu, "Choose Screen Type");

    EinkScreenInfoArray_init(instance->arr);
    uint8_t cnt = nfc_eink_screen_info_filter_by_mode(instance);

    for(uint8_t i = 0; i < cnt; i++) {
        const NfcEinkScreenInfo* item = *EinkScreenInfoArray_get(instance->arr, i);
        submenu_add_item(
            submenu, item->name, i, nfc_eink_scene_choose_screen_submenu_callback, instance);
        FURI_LOG_D(TAG, "Item: %s, width: %d, height: %d", item->name, item->width, item->height);
    }

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewMenu);
}

bool nfc_eink_scene_choose_screen_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t index = event.event;
        const NfcEinkScreenInfo* item = *EinkScreenInfoArray_get(instance->arr, index);
        instance->screen = nfc_eink_screen_alloc(item->screen_manufacturer);
        nfc_eink_screen_init(instance->screen, item->name);

        instance->screen_loaded = nfc_eink_screen_load_data(
            furi_string_get_cstr(instance->file_path), instance->screen, instance->info_temp);
        if(instance->screen_loaded) {
            scene_manager_next_scene(scene_manager, NfcEinkAppSceneWrite);
        } else {
            FURI_LOG_E(TAG, "Unable to load image data");
        }
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_choose_screen_on_exit(void* context) {
    NfcEinkApp* instance = context;
    submenu_reset(instance->submenu);
    EinkScreenInfoArray_clear(instance->arr);
}
