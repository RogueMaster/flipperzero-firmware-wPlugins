#include "nfc_comparator.h"

typedef enum {
    NfcComparatorSceneFinderSettingsMenuSelection_Recursive
} NfcComparatorSceneFinderSettingsMenuSelection;

static void nfc_comparator_finder_settings_options_change_callback(VariableItem* item) {
    furi_assert(item);
    NfcComparator* nfc_comparator = variable_item_get_context(item);

    uint8_t current_option =
        variable_item_list_get_selected_item_index(nfc_comparator->views.variable_item_list);
    uint8_t option_value_index = variable_item_get_current_value_index(item);

    switch(current_option) {
    case NfcComparatorSceneFinderSettingsMenuSelection_Recursive:
        nfc_comparator->workers.searcher.settings.recursive = option_value_index;
        variable_item_set_current_value_text(
            item, nfc_comparator->workers.searcher.settings.recursive ? "ON" : "OFF");
        break;
    default:
        break;
    }
}

void nfc_comparator_finder_settings_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;

    variable_item_list_set_header(nfc_comparator->views.variable_item_list, "Finder Settings");

    VariableItem* recursive_setting_item = variable_item_list_add(
        nfc_comparator->views.variable_item_list,
        "Recursive Search",
        2,
        nfc_comparator_finder_settings_options_change_callback,
        nfc_comparator);
    variable_item_set_current_value_index(
        recursive_setting_item, nfc_comparator->workers.searcher.settings.recursive);
    variable_item_set_current_value_text(
        recursive_setting_item,
        nfc_comparator->workers.searcher.settings.recursive ? "ON" : "OFF");

    view_dispatcher_switch_to_view(
        nfc_comparator->view_dispatcher, NfcComparatorView_VariableItemList);
}

bool nfc_comparator_finder_settings_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_comparator_finder_settings_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    variable_item_list_reset(nfc_comparator->views.variable_item_list);
}
