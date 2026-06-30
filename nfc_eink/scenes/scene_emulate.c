#include "../nfc_eink_app_i.h"

#include <lib/nfc/nfc.h>
#include <lib/nfc/protocols/nfc_protocol.h>

#define TAG "NfcEinkSceneEmulate"

static void nfc_eink_stop_emulation(NfcEinkApp* instance, bool free_screen) {
    furi_assert(instance);
    furi_delay_ms(5);
    nfc_listener_stop(instance->listener);
    nfc_listener_free(instance->listener);
    if(free_screen) {
        nfc_eink_screen_free(instance->screen);
        instance->screen_loaded = false;
    }
}

static void nfc_eink_emulate_callback(NfcEinkScreenEventType type, void* context) {
    furi_assert(context);
    NfcEinkApp* instance = context;
    NfcEinkAppCustomEvents event = NfcEinkAppCustomEventProcessFinish;

    switch(type) {
    case NfcEinkScreenEventTypeTargetDetected:
        event = NfcEinkAppCustomEventTargetDetected;
        FURI_LOG_D(TAG, "Target detected");
        break;
    case NfcEinkScreenEventTypeFinish:
        event = NfcEinkAppCustomEventProcessFinish;
        break;
    case NfcEinkScreenEventTypeError:
        event = NfcEinkAppCustomEventError;
        break;
    default:
        FURI_LOG_E(TAG, "Event: %02X not implemented", type);
        furi_crash();
        break;
    }

    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

void nfc_eink_scene_emulate_on_enter(void* context) {
    NfcEinkApp* instance = context;

    Widget* widget = instance->widget;

    widget_add_icon_element(widget, 0, 3, &I_NFC_dolphin_emulation_51x64);
    widget_add_string_element(widget, 90, 26, AlignCenter, AlignCenter, FontPrimary, "Emulating");

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewWidget);

    const NfcEinkScreen* screen = instance->screen;
    nfc_eink_screen_set_callback(instance->screen, nfc_eink_emulate_callback, instance);

    NfcDevice* nfc_device = nfc_eink_screen_get_nfc_device(screen);
    NfcProtocol protocol = nfc_device_get_protocol(nfc_device);
    const NfcDeviceData* data = nfc_device_get_data(nfc_device, protocol);
    instance->listener = nfc_listener_alloc(instance->nfc, protocol, data);
    NfcGenericCallback cb = nfc_eink_screen_get_nfc_callback(screen, NfcModeListener);
    nfc_listener_start(instance->listener, cb, instance->screen);

    nfc_eink_blink_emulate_start(instance);
}

bool nfc_eink_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcEinkAppCustomEventProcessFinish) {
            nfc_eink_stop_emulation(instance, false);
            instance->screen_loaded = true;
            scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneScreenMenu);
            notification_message(instance->notifications, &sequence_success);
        } else if(event.event == NfcEinkAppCustomEventError) {
            instance->last_error = nfc_eink_screen_get_error(instance->screen);
            nfc_eink_stop_emulation(instance, true);
            scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneError);
            notification_message(instance->notifications, &sequence_error);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        nfc_eink_stop_emulation(instance, true);
        scene_manager_previous_scene(scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_emulate_on_exit(void* context) {
    NfcEinkApp* instance = context;
    nfc_eink_blink_stop(instance);
    widget_reset(instance->widget);
}
