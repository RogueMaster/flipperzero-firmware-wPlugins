#include "../nfc_comparator.h"

static void nfc_comparator_digital_finder_select_menu_callback(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    NfcDevice* nfc_card_1 = nfc_device_alloc();
    if(!nfc_device_load(
           nfc_card_1, furi_string_get_cstr(nfc_comparator->views.file_browser.output))) {
        nfc_device_free(nfc_card_1);
        scene_manager_next_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_FailedToLoadNfcCard);
        return;
    }

    nfc_comparator->workers.compare->compare_type = NfcCompareWorkerType_Deep;

    nfc_comparator->workers.searcher.worker = nfc_comparator_finder_searcher_worker_alloc(
        nfc_comparator->workers.compare,
        &nfc_comparator->workers.searcher.settings,
        nfc_card_1,
        nfc_comparator->views.file_browser.output);

    scene_manager_next_scene(
        nfc_comparator->scene_manager, NfcComparatorScene_DigitalFinderSearch);
}

void nfc_comparator_digital_finder_select_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    furi_string_swap(
        nfc_comparator->views.file_browser.output, nfc_comparator->views.file_browser.tmp_output);

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
        nfc_comparator_digital_finder_select_menu_callback,
        nfc_comparator);
    FuriString* tmp_str = furi_string_alloc_set_str(NFC_ITEM_LOCATION);
    file_browser_start(nfc_comparator->views.file_browser.view, tmp_str);
    furi_string_free(tmp_str);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_FileBrowser);
}

bool nfc_comparator_digital_finder_select_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_comparator_digital_finder_select_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    if(furi_string_empty(nfc_comparator->views.file_browser.output)) {
        furi_string_swap(
            nfc_comparator->views.file_browser.output,
            nfc_comparator->views.file_browser.tmp_output);
        furi_string_reset(nfc_comparator->views.file_browser.tmp_output);
    }
    file_browser_stop(nfc_comparator->views.file_browser.view);
}
