#include "../nfc_comparator.h"

static volatile bool reset_exit = true;

static void
    nfc_comparator_compare_results_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc_comparator->view_dispatcher, result);
    }
}

static const char* match_str(bool match) {
    return match ? "Match" : "Mismatch";
}

void nfc_comparator_compare_results_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    FuriString* temp_str = furi_string_alloc();

    switch(nfc_comparator->workers.compare->compare_type) {
    case NfcCompareWorkerType_Shallow:
        furi_string_printf(
            temp_str,
            "\e#UID:\e# %s\n\e#UID length:\e# %s\n\e#Protocol:\e# %s",
            match_str(nfc_comparator->workers.compare->results.uid),
            match_str(nfc_comparator->workers.compare->results.uid_length),
            match_str(nfc_comparator->workers.compare->results.protocol));
        break;
    case NfcCompareWorkerType_Deep:
        furi_string_printf(
            temp_str,
            "\e#UID:\e# %s\n\e#UID length:\e# %s\n\e#Protocol:\e# %s\n\e#NFC Data:\e# %s",
            match_str(nfc_comparator->workers.compare->results.uid),
            match_str(nfc_comparator->workers.compare->results.uid_length),
            match_str(nfc_comparator->workers.compare->results.protocol),
            match_str(nfc_comparator->workers.compare->results.nfc_data));

        if(!nfc_comparator->workers.compare->results.nfc_data &&
           nfc_comparator->workers.compare->diff.count > 0) {
            widget_add_button_element(
                nfc_comparator->views.widget,
                GuiButtonTypeCenter,
                "More",
                nfc_comparator_compare_results_callback,
                nfc_comparator);
        }
        break;
    default:
        furi_string_set(temp_str, "Unknown comparison type.");
        break;
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
        nfc_comparator_compare_results_callback,
        nfc_comparator);
    widget_add_button_element(
        nfc_comparator->views.widget,
        GuiButtonTypeRight,
        "Exit",
        nfc_comparator_compare_results_callback,
        nfc_comparator);

    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Widget);
}

bool nfc_comparator_compare_results_scene_on_event(void* context, SceneManagerEvent event) {
    NfcComparator* nfc_comparator = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GuiButtonTypeLeft:
            scene_manager_previous_scene(nfc_comparator->scene_manager);
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
                nfc_comparator->scene_manager, NfcComparatorScene_CompareMenu);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_CompareMenu);
        consumed = true;
    }
    return consumed;
}

void nfc_comparator_compare_results_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    widget_reset(nfc_comparator->views.widget);
    if(reset_exit) {
        nfc_comparator_compare_worker_reset(nfc_comparator->workers.compare);
    }
}
