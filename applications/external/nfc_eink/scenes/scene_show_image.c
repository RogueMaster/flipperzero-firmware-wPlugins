#include "../nfc_eink_app_i.h"

void nfc_eink_scene_show_image_on_enter(void* context) {
    NfcEinkApp* instance = context;

    if(!instance->screen_loaded) {
        const NfcEinkScreenInfo* info = instance->info_temp;
        instance->screen = nfc_eink_screen_alloc(info->screen_manufacturer);
        nfc_eink_screen_init(instance->screen, info->name);

        if(!nfc_eink_screen_load_data(
               furi_string_get_cstr(instance->file_path), instance->screen, info)) {
            nfc_eink_screen_free(instance->screen);
            instance->screen_loaded = false;
        } else {
            instance->screen_loaded = true;
        }
    }

    ImageScroll* scroll = instance->image_scroll;
    const NfcEinkScreenInfo* info = nfc_eink_screen_get_image_info(instance->screen);

    if(instance->screen_loaded) {
        image_scroll_set_image(
            scroll,
            info->width,
            info->height,
            nfc_eink_screen_get_image_data(instance->screen),
            instance->settings.invert_image);
    }
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewImageScroll);
}

bool nfc_eink_scene_show_image_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void nfc_eink_scene_show_image_on_exit(void* context) {
    NfcEinkApp* instance = context;
    UNUSED(instance);
    image_scroll_reset(instance->image_scroll);

    if(!scene_manager_has_previous_scene(instance->scene_manager, NfcEinkAppSceneEmulate) &&
       instance->screen_loaded) {
        nfc_eink_screen_free(instance->screen);
        instance->screen_loaded = false;
    }
}
