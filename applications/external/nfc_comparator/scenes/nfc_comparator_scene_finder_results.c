#include "../nfc_comparator.h"

static volatile bool reset_exit = true;

static void
    nfc_comparator_finder_results_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc_comparator->view_dispatcher, result);
    }
}

void nfc_comparator_finder_results_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    FuriString* temp_str = furi_string_alloc();

    bool match = false;
    switch(nfc_comparator->workers.compare->compare_type) {
    case NfcCompareWorkerType_Deep:
        match = nfc_comparator->workers.compare->results.uid &&
                nfc_comparator->workers.compare->results.uid_length &&
                nfc_comparator->workers.compare->results.protocol &&
                nfc_comparator->workers.compare->results.nfc_data;
        break;
    case NfcCompareWorkerType_Shallow:
        match = nfc_comparator->workers.compare->results.uid &&
                nfc_comparator->workers.compare->results.uid_length &&
                nfc_comparator->workers.compare->results.protocol;
        break;
    default:
        furi_string_set(temp_str, "Unknown comparison type.");
        break;
    }

    if(nfc_comparator->workers.compare->compare_type != NfcCompareWorkerType_Undefined) {
        if(match) {
            furi_string_printf(
                temp_str,
                "\e#Match found!\e#\n%s",
                furi_string_get_cstr(nfc_comparator->workers.compare->nfc_card_path));
        } else {
            if(nfc_comparator->workers.compare->diff.count <
               (nfc_comparator->workers.compare->diff.total * 0.20)) {
                furi_string_printf(
                    temp_str,
                    "\e#Partial match found!\e#\n%s",
                    furi_string_get_cstr(nfc_comparator->workers.compare->nfc_card_path));

                widget_add_button_element(
                    nfc_comparator->views.widget,
                    GuiButtonTypeCenter,
                    "More",
                    nfc_comparator_finder_results_callback,
                    nfc_comparator);
            } else {
                furi_string_set(temp_str, "No match found!");
            }
        }
    }

    widget_add_text_box_element(
        nfc_comparator->views.widget,
        0,
        0,
        128,
        57,
        AlignCenter,
        AlignCenter,
        furi_string_get_cstr(temp_str),
        false);
    widget_add_button_element(
        nfc_comparator->views.widget,
        GuiButtonTypeLeft,
        "Again",
        nfc_comparator_finder_results_callback,
        nfc_comparator);
    widget_add_button_element(
        nfc_comparator->views.widget,
        GuiButtonTypeRight,
        "Exit",
        nfc_comparator_finder_results_callback,
        nfc_comparator);

    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Widget);
}

bool nfc_comparator_finder_results_scene_on_event(void* context, SceneManagerEvent event) {
    NfcComparator* nfc_comparator = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GuiButtonTypeLeft:
            if(scene_manager_has_previous_scene(
                   nfc_comparator->scene_manager, NfcComparatorScene_DigitalFinderSearch)) {
                scene_manager_search_and_switch_to_previous_scene(
                    nfc_comparator->scene_manager, NfcComparatorScene_DigitalFinderSelect);
            } else {
                scene_manager_previous_scene(nfc_comparator->scene_manager);
            }

            consumed = true;
            break;
        case GuiButtonTypeCenter:
            reset_exit = false;
            scene_manager_next_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_DataBreakdown);
            consumed = true;
            break;
        case GuiButtonTypeRight:
            scene_manager_search_and_switch_to_previous_scene(
                nfc_comparator->scene_manager, NfcComparatorScene_FinderMenu);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_FinderMenu);
        consumed = true;
    }
    return consumed;
}

void nfc_comparator_finder_results_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    widget_reset(nfc_comparator->views.widget);
    if(reset_exit) {
        nfc_comparator_compare_worker_reset(nfc_comparator->workers.compare);
    }
}
