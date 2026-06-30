#include "nfc_comparator.h"

static bool nfc_comparator_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    return scene_manager_handle_custom_event(nfc_comparator->scene_manager, custom_event);
}

static bool nfc_comparator_back_event_callback(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    return scene_manager_handle_back_event(nfc_comparator->scene_manager);
}

static void nfc_comparator_tick_event_callback(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    scene_manager_handle_tick_event(nfc_comparator->scene_manager);
}

static NfcComparator* nfc_comparator_alloc() {
    NfcComparator* nfc_comparator = malloc(sizeof(NfcComparator));
    furi_assert(nfc_comparator);
    nfc_comparator->scene_manager =
        scene_manager_alloc(&nfc_comparator_scene_handlers, nfc_comparator);
    nfc_comparator->view_dispatcher = view_dispatcher_alloc();

    nfc_comparator->views.submenu = submenu_alloc();

    nfc_comparator->views.file_browser.output = furi_string_alloc();
    nfc_comparator->views.file_browser.tmp_output = furi_string_alloc();
    nfc_comparator->views.file_browser.view =
        file_browser_alloc(nfc_comparator->views.file_browser.output);

    nfc_comparator->views.popup = popup_alloc();

    nfc_comparator->views.widget = widget_alloc();

    nfc_comparator->views.loading = loading_alloc();

    nfc_comparator->views.variable_item_list = variable_item_list_alloc();

    nfc_comparator->views.text_box.view = text_box_alloc();
    nfc_comparator->views.text_box.store = furi_string_alloc();

    nfc_comparator->notification_app = furi_record_open(RECORD_NOTIFICATION);

    nfc_comparator->workers.compare = nfc_comparator_compare_worker_alloc();

    nfc_comparator->workers.searcher.settings.recursive = true;

    view_dispatcher_set_event_callback_context(nfc_comparator->view_dispatcher, nfc_comparator);
    view_dispatcher_set_custom_event_callback(
        nfc_comparator->view_dispatcher, nfc_comparator_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        nfc_comparator->view_dispatcher, nfc_comparator_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        nfc_comparator->view_dispatcher, nfc_comparator_tick_event_callback, 100);

    view_dispatcher_add_view(
        nfc_comparator->view_dispatcher,
        NfcComparatorView_Submenu,
        submenu_get_view(nfc_comparator->views.submenu));
    view_dispatcher_add_view(
        nfc_comparator->view_dispatcher,
        NfcComparatorView_FileBrowser,
        file_browser_get_view(nfc_comparator->views.file_browser.view));
    view_dispatcher_add_view(
        nfc_comparator->view_dispatcher,
        NfcComparatorView_Popup,
        popup_get_view(nfc_comparator->views.popup));
    view_dispatcher_add_view(
        nfc_comparator->view_dispatcher,
        NfcComparatorView_Widget,
        widget_get_view(nfc_comparator->views.widget));
    view_dispatcher_add_view(
        nfc_comparator->view_dispatcher,
        NfcComparatorView_Loading,
        loading_get_view(nfc_comparator->views.loading));
    view_dispatcher_add_view(
        nfc_comparator->view_dispatcher,
        NfcComparatorView_VariableItemList,
        variable_item_list_get_view(nfc_comparator->views.variable_item_list));
    view_dispatcher_add_view(
        nfc_comparator->view_dispatcher,
        NfcComparatorView_TextBox,
        text_box_get_view(nfc_comparator->views.text_box.view));

    return nfc_comparator;
}

static void nfc_comparator_free(NfcComparator* nfc_comparator) {
    furi_assert(nfc_comparator);

    view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_Submenu);
    view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_FileBrowser);
    view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_Popup);
    view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_Widget);
    view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_Loading);
    view_dispatcher_remove_view(
        nfc_comparator->view_dispatcher, NfcComparatorView_VariableItemList);
    view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_TextBox);

    scene_manager_free(nfc_comparator->scene_manager);
    view_dispatcher_free(nfc_comparator->view_dispatcher);

    submenu_free(nfc_comparator->views.submenu);
    file_browser_free(nfc_comparator->views.file_browser.view);
    furi_string_free(nfc_comparator->views.file_browser.output);
    furi_string_free(nfc_comparator->views.file_browser.tmp_output);
    popup_free(nfc_comparator->views.popup);
    widget_free(nfc_comparator->views.widget);
    loading_free(nfc_comparator->views.loading);
    variable_item_list_free(nfc_comparator->views.variable_item_list);
    text_box_free(nfc_comparator->views.text_box.view);
    furi_string_free(nfc_comparator->views.text_box.store);
    nfc_comparator_compare_worker_free(nfc_comparator->workers.compare);
    furi_record_close(RECORD_NOTIFICATION);

    free(nfc_comparator);
}

static inline void nfc_comparator_set_log_level() {
#ifdef FURI_DEBUG
    furi_log_set_level(FuriLogLevelTrace);
#else
    furi_log_set_level(FuriLogLevelInfo);
#endif
}

int32_t nfc_comparator_main(void* p) {
    UNUSED(p);

    NfcComparator* nfc_comparator = nfc_comparator_alloc();

    nfc_comparator_set_log_level();

    dolphin_deed(DolphinDeedPluginStart);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(
        nfc_comparator->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(nfc_comparator->scene_manager, NfcComparatorScene_MainMenu);

    view_dispatcher_run(nfc_comparator->view_dispatcher);

    furi_record_close(RECORD_GUI);
    nfc_comparator_free(nfc_comparator);

    return 0;
}
