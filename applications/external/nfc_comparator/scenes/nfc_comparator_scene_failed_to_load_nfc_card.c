#include "../nfc_comparator.h"

static void nfc_comparator_failed_to_load_nfc_card_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc_comparator->view_dispatcher, result);
    }
}

void nfc_comparator_failed_to_load_nfc_card_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    nfc_comparator_led_worker_start(nfc_comparator->notification_app, NfcComparatorLedState_Error);

    widget_add_text_box_element(
        nfc_comparator->views.widget,
        0,
        0,
        128,
        50,
        AlignCenter,
        AlignCenter,
        "\e#Failed to load\nselected card\e#",
        false);
    widget_add_button_element(
        nfc_comparator->views.widget,
        GuiButtonTypeLeft,
        "Try Again",
        nfc_comparator_failed_to_load_nfc_card_callback,
        nfc_comparator);
    widget_add_button_element(
        nfc_comparator->views.widget,
        GuiButtonTypeRight,
        "Main Menu",
        nfc_comparator_failed_to_load_nfc_card_callback,
        nfc_comparator);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Widget);
}

bool nfc_comparator_failed_to_load_nfc_card_scene_on_event(void* context, SceneManagerEvent event) {
    NfcComparator* nfc_comparator = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GuiButtonTypeRight:
            scene_manager_search_and_switch_to_previous_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_MainMenu);
            consumed = true;
            break;
        case GuiButtonTypeLeft:
            scene_manager_previous_scene(nfc_comparator->scene_manager);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_MainMenu);
        consumed = true;
    }
    return consumed;
}

void nfc_comparator_failed_to_load_nfc_card_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    widget_reset(nfc_comparator->views.widget);
    nfc_comparator_led_worker_stop(nfc_comparator->notification_app);
}
