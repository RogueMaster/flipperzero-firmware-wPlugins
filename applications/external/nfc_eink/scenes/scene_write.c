#include "../nfc_eink_app_i.h"

#define TAG "NfcEinkSceneWrite"

typedef enum {
    NfcEinkAppSceneWriteStateWaitingForTarget,
    NfcEinkAppSceneWriteStateWritingDataBlocks,
    NfcEinkAppSceneWriteStateUpdatingScreen,
} NfcEinkAppSceneWriteStates;

static void nfc_eink_write_callback(NfcEinkScreenEventType type, void* context) {
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

    case NfcEinkScreenEventTypeBlockProcessed:
        event = NfcEinkAppCustomEventBlockProcessed;
        break;

    case NfcEinkScreenEventTypeError:
        event = NfcEinkAppCustomEventError;
        break;

    case NfcEinkScreenEventTypeUpdating:
        event = NfcEinkAppCustomEventUpdating;
        break;
    default:
        FURI_LOG_E(TAG, "Event: %02X not implemented", type);
        furi_crash();
        break;
    }
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

static void nfc_eink_scene_write_show_waiting(const NfcEinkApp* instance) {
    popup_reset(instance->popup);
    popup_set_header(instance->popup, "Waiting", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        instance->popup, "Apply eink next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewPopup);
}

static void nfc_eink_scene_write_show_writing_data(const NfcEinkApp* instance) {
    eink_progress_reset(instance->eink_progress);
    FuriString* str = furi_string_alloc_printf(
        "\e#Writting...\n\e#%s", nfc_eink_screen_get_name(instance->screen));

    eink_progress_set_header(instance->eink_progress, furi_string_get_cstr(str));

    size_t total = 0, current = 0;
    nfc_eink_screen_get_progress(instance->screen, &current, &total);

    eink_progress_set_value(instance->eink_progress, current, total);
    furi_string_free(str);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewProgress);
}

static void nfc_eink_scene_write_show_updating(const NfcEinkApp* instance) {
    NfcEinkAppSceneWriteStates state = (NfcEinkAppSceneWriteStates)scene_manager_get_scene_state(
        instance->scene_manager, NfcEinkAppSceneWrite);

    if(state != NfcEinkAppSceneWriteStateUpdatingScreen) {
        popup_reset(instance->popup);
        popup_set_header(instance->popup, "Updating...", 80, 15, AlignCenter, AlignTop);
        popup_set_text(
            instance->popup, "Wait until\nupdate complete", 75, 30, AlignCenter, AlignTop);
        popup_set_icon(instance->popup, 5, 11, &I_ArrowC_1_36x36);
        view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewPopup);
    }

    scene_manager_set_scene_state(
        instance->scene_manager, NfcEinkAppSceneWrite, NfcEinkAppSceneWriteStateUpdatingScreen);
}

void nfc_eink_scene_write_on_enter(void* context) {
    NfcEinkApp* instance = context;

    nfc_eink_scene_write_show_waiting(instance);
    scene_manager_set_scene_state(
        instance->scene_manager, NfcEinkAppSceneWrite, NfcEinkAppSceneWriteStateWaitingForTarget);

    NfcEinkScreen* screen = instance->screen;

    nfc_eink_screen_set_callback(instance->screen, nfc_eink_write_callback, instance);
    const NfcProtocol protocol = nfc_device_get_protocol(nfc_eink_screen_get_nfc_device(screen));
    instance->poller = nfc_poller_alloc(instance->nfc, protocol);

    NfcGenericCallback cb = nfc_eink_screen_get_nfc_callback(screen, NfcModePoller);
    nfc_poller_start(instance->poller, cb, screen);

    nfc_eink_blink_write_start(instance);
}

bool nfc_eink_scene_write_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcEinkAppCustomEventProcessFinish) {
            scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneWriteDone);
            notification_message(instance->notifications, &sequence_success);
        } else if(event.event == NfcEinkAppCustomEventTargetDetected) {
            scene_manager_set_scene_state(
                instance->scene_manager,
                NfcEinkAppSceneWrite,
                NfcEinkAppSceneWriteStateWritingDataBlocks);
            nfc_eink_scene_write_show_writing_data(instance);
        } else if(event.event == NfcEinkAppCustomEventError) {
            instance->last_error = nfc_eink_screen_get_error(instance->screen);
            scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneError);
            notification_message(instance->notifications, &sequence_error);
        } else if(event.event == NfcEinkAppCustomEventBlockProcessed) {
            size_t total = 0, current = 0;
            nfc_eink_screen_get_progress(instance->screen, &current, &total);
            eink_progress_set_value(instance->eink_progress, current, total);
        } else if(event.event == NfcEinkAppCustomEventUpdating) {
            nfc_eink_scene_write_show_updating(instance);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_write_on_exit(void* context) {
    NfcEinkApp* instance = context;
    nfc_eink_blink_stop(instance);
    popup_reset(instance->popup);
    eink_progress_reset(instance->eink_progress);

    nfc_poller_stop(instance->poller);
    nfc_poller_free(instance->poller);
    nfc_eink_screen_free(instance->screen);
    instance->screen_loaded = false;
}
