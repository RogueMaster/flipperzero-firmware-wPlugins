#include "../nfc_comparator.h"

typedef enum {
    NfcComparatorMainMenu_Physical,
    NfcComparatorMainMenu_Digital,
    NfcComparatorMainMenu_Settings
} NfcComparatorMainMenuMenuSelection;

static void nfc_comparator_finder_menu_menu_callback(void* context, uint32_t index) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    scene_manager_handle_custom_event(nfc_comparator->scene_manager, index);
}

void nfc_comparator_finder_menu_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    submenu_set_header(nfc_comparator->views.submenu, "Finder Menu");

    submenu_add_item(
        nfc_comparator->views.submenu,
        "Start Physical",
        NfcComparatorMainMenu_Physical,
        nfc_comparator_finder_menu_menu_callback,
        nfc_comparator);
    submenu_add_item(
        nfc_comparator->views.submenu,
        "Start Digital",
        NfcComparatorMainMenu_Digital,
        nfc_comparator_finder_menu_menu_callback,
        nfc_comparator);
    submenu_add_item(
        nfc_comparator->views.submenu,
        "Settings",
        NfcComparatorMainMenu_Settings,
        nfc_comparator_finder_menu_menu_callback,
        nfc_comparator);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Submenu);
}

bool nfc_comparator_finder_menu_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NfcComparatorMainMenu_Physical:
            scene_manager_next_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_PhysicalFinderScan);
            consumed = true;
            break;
        case NfcComparatorMainMenu_Digital:
            scene_manager_next_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_DigitalFinderSelect);
            consumed = true;
            break;
        case NfcComparatorMainMenu_Settings:
            scene_manager_next_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_FinderSettings);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void nfc_comparator_finder_menu_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    submenu_reset(nfc_comparator->views.submenu);
}
