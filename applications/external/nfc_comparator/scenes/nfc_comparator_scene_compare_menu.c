#include "../nfc_comparator.h"

typedef enum {
    NfcComparatorMainMenu_Physical,
    NfcComparatorMainMenu_Digital,
    NfcComparatorMainMenu_SelectNfcCard
} NfcComparatorMainMenuMenuSelection;

static void nfc_comparator_compare_menu_menu_callback(void* context, uint32_t index) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    scene_manager_handle_custom_event(nfc_comparator->scene_manager, index);
}

void nfc_comparator_compare_menu_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    submenu_set_header(nfc_comparator->views.submenu, "Compare Menu");

    submenu_add_lockable_item(
        nfc_comparator->views.submenu,
        "Start Physical",
        NfcComparatorMainMenu_Physical,
        nfc_comparator_compare_menu_menu_callback,
        nfc_comparator,
        furi_string_empty(nfc_comparator->views.file_browser.output),
        "No NFC\ncard selected");

    submenu_add_lockable_item(
        nfc_comparator->views.submenu,
        "Start Digital",
        NfcComparatorMainMenu_Digital,
        nfc_comparator_compare_menu_menu_callback,
        nfc_comparator,
        furi_string_empty(nfc_comparator->views.file_browser.output),
        "No NFC\ncard selected");

    submenu_add_item(
        nfc_comparator->views.submenu,
        "Select NFC Card",
        NfcComparatorMainMenu_SelectNfcCard,
        nfc_comparator_compare_menu_menu_callback,
        nfc_comparator);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Submenu);
}

bool nfc_comparator_compare_menu_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NfcComparatorMainMenu_Physical:
            scene_manager_next_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_PhysicalCompareScan);
            consumed = true;
            break;
        case NfcComparatorMainMenu_Digital:
            scene_manager_next_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_DigitalCompareScan);
            consumed = true;
            break;
        case NfcComparatorMainMenu_SelectNfcCard:
            scene_manager_next_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_SelectNfcCard);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void nfc_comparator_compare_menu_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    submenu_reset(nfc_comparator->views.submenu);
}
