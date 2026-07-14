#include "../nfc_comparator.h"

static void nfc_comparator_digital_compare_scan_menu_callback(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Loading);

    furi_string_swap(
        nfc_comparator->views.file_browser.output, nfc_comparator->views.file_browser.tmp_output);

    NfcDevice* nfc_card_1 = nfc_device_alloc();
    if(!nfc_device_load(
           nfc_card_1, furi_string_get_cstr(nfc_comparator->views.file_browser.output))) {
        nfc_device_free(nfc_card_1);
        scene_manager_next_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_FailedToLoadNfcCard);
        return;
    }

    NfcDevice* nfc_card_2 = nfc_device_alloc();
    if(!nfc_device_load(
           nfc_card_2, furi_string_get_cstr(nfc_comparator->views.file_browser.tmp_output))) {
        nfc_device_free(nfc_card_1);
        nfc_device_free(nfc_card_2);
        scene_manager_next_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_FailedToLoadNfcCard);
        return;
    }

    nfc_comparator->workers.compare->compare_type = NfcCompareWorkerType_Deep;

    nfc_comparator_compare_worker_compare_cards(
        nfc_comparator->workers.compare, nfc_card_1, nfc_card_2);

    nfc_comparator_led_worker_stop(nfc_comparator->notification_app);

    if(nfc_comparator->workers.compare->diff.total > 0) {
        uint16_t total = nfc_comparator->workers.compare->diff.total;
        uint16_t diff = nfc_comparator->workers.compare->diff.count;
        uint8_t percentage = ((total - diff) * 100) / total;

        if(percentage == 100) {
            dolphin_deed(DolphinDeedNfcReadSuccess);
            dolphin_deed(DolphinDeedNfcSave);
        } else if(percentage >= 95) {
            dolphin_deed(DolphinDeedNfcReadSuccess);
        } else if(percentage >= 80) {
            dolphin_deed(DolphinDeedNfcRead);
        }
    } else if(nfc_comparator->workers.compare->results.nfc_data) {
        dolphin_deed(DolphinDeedNfcReadSuccess);
        dolphin_deed(DolphinDeedNfcSave);
    } else {
        dolphin_deed(DolphinDeedNfcRead);
    }

    nfc_device_free(nfc_card_1);
    nfc_device_free(nfc_card_2);

    furi_string_reset(nfc_comparator->views.file_browser.tmp_output);

    nfc_comparator_led_worker_start(
        nfc_comparator->notification_app, NfcComparatorLedState_Complete);

    scene_manager_next_scene(nfc_comparator->scene_manager, NfcComparatorScene_CompareResults);
}

void nfc_comparator_digital_compare_scan_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    nfc_comparator_led_worker_start(
        nfc_comparator->notification_app, NfcComparatorLedState_Running);

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
        nfc_comparator_digital_compare_scan_menu_callback,
        nfc_comparator);
    FuriString* tmp_str = furi_string_alloc_set_str(NFC_ITEM_LOCATION);
    file_browser_start(nfc_comparator->views.file_browser.view, tmp_str);
    furi_string_free(tmp_str);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_FileBrowser);
}

bool nfc_comparator_digital_compare_scan_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

void nfc_comparator_digital_compare_scan_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    if(furi_string_empty(nfc_comparator->views.file_browser.output)) {
        furi_string_swap(
            nfc_comparator->views.file_browser.output,
            nfc_comparator->views.file_browser.tmp_output);
        furi_string_reset(nfc_comparator->views.file_browser.tmp_output);
    }
    file_browser_stop(nfc_comparator->views.file_browser.view);
    nfc_comparator_led_worker_stop(nfc_comparator->notification_app);
}
