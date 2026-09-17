#include "view_bank_tools.h"

static uint32_t bank_tools_back_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

static void bank_tools_item_callback(void* context, uint32_t index) {
    UHFReaderApp* App = context;

    if(!uhf_reader_require_antenna(App, UHFReaderViewBankTools)) return;

    if(index == UHFReaderSubmenuIndexGetSizeBank) {
        uhf_reader_ensure_bank_size_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewBankSize);
    } else if(index == UHFReaderSubmenuIndexCheckTagRewritable) {
        uhf_reader_ensure_rewritable_views(App);
        uhf_reader_open_rewritable_confirm(App);
    }
}

void view_bank_tools_alloc(UHFReaderApp* App) {
    App->SubmenuBankTools = submenu_alloc();
    submenu_set_header(App->SubmenuBankTools, "Get Info Bank on Tag");
    submenu_add_item(
        App->SubmenuBankTools,
        "Get Size Bank",
        UHFReaderSubmenuIndexGetSizeBank,
        bank_tools_item_callback,
        App);
    submenu_add_item(
        App->SubmenuBankTools,
        "Check TAG Rewritable",
        UHFReaderSubmenuIndexCheckTagRewritable,
        bank_tools_item_callback,
        App);
    view_set_previous_callback(submenu_get_view(App->SubmenuBankTools), bank_tools_back_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewBankTools, submenu_get_view(App->SubmenuBankTools));
}

void view_bank_tools_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewBankTools);
    submenu_free(App->SubmenuBankTools);
    App->SubmenuBankTools = NULL;
}
