#include "view_saved.h"

/**
 * @brief      Callback when the user exits the saved screen.
 * @details    This function is called when the user exits the saved screen.
 * @param      _context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_saved_exit_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

/**
 * @brief      Callback for the saved tag selected
 * @details    This function is called when the user selects a saved tag from the submenu
 * @param      context  The context - UHFReaderApp object
 * @param      index    The index of the selected saved tag
*/
void uhf_reader_submenu_saved_callback(void* context, uint32_t index) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    if(index == UHF_SAVED_DELETE_ALL_INDEX) {
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewDeleteAllConfirm);
        return;
    }

    if(index == UHF_SAVED_PREVIOUS_PAGE_INDEX) {
        if(App->SavedPage > 0U) App->SavedPage--;
        uhf_reader_saved_menu_rebuild(App);
        return;
    }

    if(index == UHF_SAVED_NEXT_PAGE_INDEX) {
        App->SavedPage++;
        uhf_reader_saved_menu_rebuild(App);
        return;
    }

    if(index == 0U || index > App->NumberOfSavedTags) return;

    App->SelectedTagIndex = index;
    App->ActionContext = ActionFromSaved;
    uhf_reader_ensure_tag_actions_view(App);
    uhf_reader_build_tag_action_menu(App);
    view_set_previous_callback(
        submenu_get_view(App->SubmenuTagActions), uhf_reader_navigation_tag_action_exit_callback);
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewTagAction);
}

void uhf_reader_saved_menu_rebuild(UHFReaderApp* App) {
    if(!App || !App->SubmenuSaved) return;

    const size_t heap_before_rebuild =
        uhf_debug_heap(UHFDebugInfo, "saved_menu", "rebuild_begin", 0U);

    uint32_t total = App->NumberOfSavedTags;
    uint32_t page_count = total == 0U ? 1U :
                                        (total + UHF_SAVED_PAGE_SIZE - 1U) / UHF_SAVED_PAGE_SIZE;
    if(App->SavedPage >= page_count) App->SavedPage = page_count - 1U;

    uint32_t first = App->SavedPage * UHF_SAVED_PAGE_SIZE + 1U;
    uint32_t last = first + UHF_SAVED_PAGE_SIZE - 1U;
    if(last > total) last = total;

    small_submenu_reset(App->SubmenuSaved);

    char header[32];
    if(page_count > 1U) {
        snprintf(
            header,
            sizeof(header),
            "Saved %lu/%lu (%lu)",
            (unsigned long)(App->SavedPage + 1U),
            (unsigned long)page_count,
            (unsigned long)total);
    } else {
        snprintf(header, sizeof(header), "Saved Dumps (%lu)", (unsigned long)total);
    }
    small_submenu_set_header(App->SubmenuSaved, header);

    UHF_I(
        "SAVE",
        "Saved page rebuild BEGIN page=%lu/%lu range=%lu..%lu total=%lu",
        (unsigned long)(App->SavedPage + 1U),
        (unsigned long)page_count,
        (unsigned long)first,
        (unsigned long)last,
        (unsigned long)total);

    if(App->SavedPage > 0U) {
        small_submenu_add_item(
            App->SubmenuSaved,
            "< Previous page",
            UHF_SAVED_PREVIOUS_PAGE_INDEX,
            uhf_reader_submenu_saved_callback,
            App);
    }

    if(total > 0U &&
       flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        FuriString* key = furi_string_alloc();
        FuriString* tag_value = furi_string_alloc();

        for(uint32_t i = first; i <= last; i++) {
            furi_string_printf(key, "Tag%lu", (unsigned long)i);
            furi_string_reset(tag_value);

            if(flipper_format_read_string(App->EpcFile, furi_string_get_cstr(key), tag_value)) {
                char* name = extract_name(furi_string_get_cstr(tag_value));
                if(name) {
                    small_submenu_add_item(
                        App->SubmenuSaved, name, i, uhf_reader_submenu_saved_callback, App);
                    free(name);
                }
            } else {
                UHF_W("SAVE", "Saved menu missing Tag%lu during rebuild", (unsigned long)i);
            }
        }

        furi_string_free(key);
        furi_string_free(tag_value);
        flipper_format_file_close(App->EpcFile);
    } else {
        flipper_format_file_close(App->EpcFile);
    }

    if(last < total) {
        small_submenu_add_item(
            App->SubmenuSaved,
            "Next page >",
            UHF_SAVED_NEXT_PAGE_INDEX,
            uhf_reader_submenu_saved_callback,
            App);
    }

    if(total > 0U) {
        small_submenu_add_item(
            App->SubmenuSaved,
            "Delete ALL saved",
            UHF_SAVED_DELETE_ALL_INDEX,
            uhf_reader_submenu_saved_callback,
            App);
    }

    UHF_I(
        "SAVE",
        "Saved page rebuild END page=%lu visible=%lu",
        (unsigned long)(App->SavedPage + 1U),
        (unsigned long)(last >= first ? last - first + 1U : 0U));
    uhf_debug_heap(UHFDebugInfo, "saved_menu", "rebuild_end", heap_before_rebuild);
    uhf_debug_flush();
}

/**
 * @brief      Allocates the Saved menu menu
 * @details    This function allocates all variables for the saved menu
 * @param      context  The context - UHFReaderApp object.
*/
void view_saved_menu_alloc(UHFReaderApp* App) {
    App->SubmenuSaved = small_submenu_alloc();
    small_submenu_set_header(App->SubmenuSaved, "Saved Dumps (0)");

    /*
     * Never trust Index_File.txt as the primary source.
     * It is only a cache. Saved_EPCs.txt is durable source-of-truth and is
     * recounted on every application launch.
     */
    uhf_saved_recount_and_repair_index(App);
    uhf_saved_update_count_labels(App);
    App->SavedPage = 0U;
    uhf_reader_saved_menu_rebuild(App);

    view_set_previous_callback(
        small_submenu_get_view(App->SubmenuSaved), uhf_reader_navigation_saved_exit_callback);

    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSaved, small_submenu_get_view(App->SubmenuSaved));
}

/**
 * @brief      Frees the saved view.
 * @details    This function frees all variables for the saved view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_saved_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSaved);
    small_submenu_free(App->SubmenuSaved);
    App->SubmenuSaved = NULL;
}
