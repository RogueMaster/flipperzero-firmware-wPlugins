#include "../nfc_comparator.h"

static void nfc_comparator_select_nfc_card_menu_callback(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    scene_manager_previous_scene(nfc_comparator->scene_manager);
}

void nfc_comparator_select_nfc_card_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    file_browser_configure(
        nfc_comparator->views.file_browser.view,
        ".nfc",
        NFC_ITEM_LOCATION,
        true,
        true,
        &I_Nfc_10px,
        true);
    file_browser_set_callback(
        nfc_comparator->views.file_browser.view,
        nfc_comparator_select_nfc_card_menu_callback,
        nfc_comparator);
    FuriString* tmp_str = furi_string_alloc_set_str(NFC_ITEM_LOCATION);
    file_browser_start(nfc_comparator->views.file_browser.view, tmp_str);
    furi_string_free(tmp_str);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_FileBrowser);
}

bool nfc_comparator_select_nfc_card_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

void nfc_comparator_select_nfc_card_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    file_browser_stop(nfc_comparator->views.file_browser.view);
}
