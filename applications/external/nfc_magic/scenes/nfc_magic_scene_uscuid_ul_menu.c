#include "../nfc_magic_app_i.h"

#include <nfc/helpers/nfc_data_generator.h>

enum SubmenuIndex {
    SubmenuIndexWrite,
    SubmenuIndexWipe,
    SubmenuIndexAuthPassword,
};

// Map a recognised USCUID-UL type to its "Add manually" default-dump generator.
static NfcDataGeneratorType nfc_magic_uscuid_ul_wipe_generator(MfUltralightType type) {
    switch(type) {
    case MfUltralightTypeUL11:
        return NfcDataGeneratorTypeMfUltralightEV1_11;
    case MfUltralightTypeNTAG213:
        return NfcDataGeneratorTypeNTAG213;
    case MfUltralightTypeNTAG215:
        return NfcDataGeneratorTypeNTAG215;
    case MfUltralightTypeNTAG216:
        return NfcDataGeneratorTypeNTAG216;
    case MfUltralightTypeUL21:
    default: // UL21, and UL-5 (Origin) which is physically a UL21
        return NfcDataGeneratorTypeMfUltralightEV1_21;
    }
}

// Wipe = a factory-default dump for the detected type (config/PWD/lock reset) with a zeroed UID
// (valid BCC0 = 0x88). Synthesized into the source device; the existing write engine then clones
// it onto the tag, using the armed password if one is set.
static void nfc_magic_uscuid_ul_menu_fill_wipe_dump(NfcMagicApp* instance) {
    nfc_data_generator_fill_data(
        nfc_magic_uscuid_ul_wipe_generator(instance->uscuid_ul_data.type), instance->source_dev);

    MfUltralightData* data = mf_ultralight_alloc();
    mf_ultralight_copy(data, nfc_device_get_data(instance->source_dev, NfcProtocolMfUltralight));
    memset(data->page[0].data, 0, MF_ULTRALIGHT_PAGE_SIZE);
    data->page[0].data[3] = 0x88; // BCC0 = CT ^ 0 ^ 0 ^ 0
    memset(data->page[1].data, 0, MF_ULTRALIGHT_PAGE_SIZE);
    memset(data->page[2].data, 0, MF_ULTRALIGHT_PAGE_SIZE);
    nfc_device_set_data(instance->source_dev, NfcProtocolMfUltralight, data);
    mf_ultralight_free(data);
}

void nfc_magic_scene_uscuid_ul_menu_submenu_callback(void* context, uint32_t index) {
    NfcMagicApp* instance = context;

    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

void nfc_magic_scene_uscuid_ul_menu_on_enter(void* context) {
    NfcMagicApp* instance = context;
    instance->uscuid_ul_is_wipe_mode = false;

    Submenu* submenu = instance->submenu;
    submenu_add_item(
        submenu,
        "Write",
        SubmenuIndexWrite,
        nfc_magic_scene_uscuid_ul_menu_submenu_callback,
        instance);

    // Wipe is offered for any writable USCUID-UL: the recognised types and UL-5 (a UL21).
    // Same gate as the write path; excludes UL-C, UL-Y/unknown and not-detected.
    if(uscuid_ul_data_is_writable(&instance->uscuid_ul_data)) {
        submenu_add_item(
            submenu,
            "Wipe",
            SubmenuIndexWipe,
            nfc_magic_scene_uscuid_ul_menu_submenu_callback,
            instance);
    }

    // PWD-AUTH only works on the direct/ATS transport (wakeup None); not the backdoor path.
    if(instance->uscuid_ul_data.wakeup == UscuidUlWakeupNone) {
        submenu_add_item(
            submenu,
            instance->uscuid_ul_password_set ? "Auth with Password (set)" : "Auth with Password",
            SubmenuIndexAuthPassword,
            nfc_magic_scene_uscuid_ul_menu_submenu_callback,
            instance);
    }

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneUscuidUlMenu));
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewMenu);
}

bool nfc_magic_scene_uscuid_ul_menu_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexWrite) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneFileSelect);
            consumed = true;
        } else if(event.event == SubmenuIndexWipe) {
            instance->uscuid_ul_is_wipe_mode = true;
            nfc_magic_uscuid_ul_menu_fill_wipe_dump(instance);
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWriteConfirm);
            consumed = true;
        } else if(event.event == SubmenuIndexAuthPassword) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlPwdInput);
            consumed = true;
        }
        scene_manager_set_scene_state(
            instance->scene_manager, NfcMagicSceneUscuidUlMenu, event.event);
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneStart);
    }

    return consumed;
}

void nfc_magic_scene_uscuid_ul_menu_on_exit(void* context) {
    NfcMagicApp* instance = context;

    submenu_reset(instance->submenu);
}
