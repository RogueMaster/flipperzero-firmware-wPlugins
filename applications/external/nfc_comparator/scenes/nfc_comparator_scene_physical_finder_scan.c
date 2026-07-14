#include "../nfc_comparator.h"

static volatile bool force_quit = false;

void nfc_comparator_physical_finder_scan_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    force_quit = false;

    popup_set_context(nfc_comparator->views.popup, nfc_comparator);
    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Popup);

    nfc_comparator->workers.compare->compare_type = NfcCompareWorkerType_Shallow;

    nfc_comparator->workers.finder = nfc_comparator_finder_reader_worker_alloc(
        nfc_comparator->workers.compare, &nfc_comparator->workers.searcher.settings);

    nfc_comparator_finder_reader_worker_start(nfc_comparator->workers.finder);
    nfc_comparator_led_worker_start(
        nfc_comparator->notification_app, NfcComparatorLedState_Running);
}

bool nfc_comparator_physical_finder_scan_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        force_quit = true;
        nfc_comparator_finder_reader_worker_stop(nfc_comparator->workers.finder);
        scene_manager_search_and_switch_to_previous_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_FinderMenu);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        switch(*nfc_comparator_finder_reader_worker_get_state(nfc_comparator->workers.finder)) {
        case NfcComparatorFinderReaderWorkerState_Scanning:
            popup_set_header(
                nfc_comparator->views.popup, "Scanning....", 64, 5, AlignCenter, AlignTop);
            popup_set_text(
                nfc_comparator->views.popup,
                "Hold card next\nto Flipper's back",
                64,
                40,
                AlignCenter,
                AlignTop);
            break;
        case NfcComparatorFinderReaderWorkerState_Polling:
            popup_set_header(
                nfc_comparator->views.popup, "Polling....", 64, 5, AlignCenter, AlignTop);
            break;
        case NfcComparatorFinderReaderWorkerState_Finding:
            popup_set_header(
                nfc_comparator->views.popup, "Finding....", 64, 5, AlignCenter, AlignTop);
            break;
        case NfcComparatorFinderReaderWorkerState_Stopped:
            if(!force_quit) {
                nfc_comparator_finder_reader_worker_stop(nfc_comparator->workers.finder);

                nfc_comparator_led_worker_stop(nfc_comparator->notification_app);
                nfc_comparator_led_worker_start(
                    nfc_comparator->notification_app, NfcComparatorLedState_Complete);

                scene_manager_next_scene(
                    nfc_comparator->scene_manager, NfcComparatorScene_FinderResults);
            } else {
                nfc_comparator_compare_worker_reset(nfc_comparator->workers.compare);
            }
            consumed = true;
            break;
        default: {
            break;
        }
        }
    }
    return consumed;
}

void nfc_comparator_physical_finder_scan_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    popup_reset(nfc_comparator->views.popup);
    nfc_comparator_led_worker_stop(nfc_comparator->notification_app);
    nfc_comparator_finder_reader_worker_free(nfc_comparator->workers.finder);
}
