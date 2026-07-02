#include "app_state.h"
#include "ui.h"
#include <furi.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <string.h>

static SubDupFinderApp* app_alloc(void) {
    SubDupFinderApp* app = malloc(sizeof(SubDupFinderApp));
    memset(app, 0, sizeof(SubDupFinderApp));
    app->view_dispatcher = view_dispatcher_alloc();
    app->main_submenu = submenu_alloc();
    app->groups_submenu = submenu_alloc();
    app->files_in_group_submenu = submenu_alloc();
    app->confirm_dialog = dialog_ex_alloc();
    app->credits_widget = widget_alloc();
    app->popup = popup_alloc();
    return app;
}

static void app_free(SubDupFinderApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, SubDupFinderViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SubDupFinderViewGroups);
    view_dispatcher_remove_view(app->view_dispatcher, SubDupFinderViewFiles);
    view_dispatcher_remove_view(app->view_dispatcher, SubDupFinderViewConfirm);
    view_dispatcher_remove_view(app->view_dispatcher, SubDupFinderViewCredits);
    view_dispatcher_remove_view(app->view_dispatcher, SubDupFinderViewPopup);
    submenu_free(app->main_submenu);
    submenu_free(app->groups_submenu);
    submenu_free(app->files_in_group_submenu);
    dialog_ex_free(app->confirm_dialog);
    widget_free(app->credits_widget);
    popup_free(app->popup);
    view_dispatcher_free(app->view_dispatcher);
    free(app);
}

int32_t sub_dup_finder_app(void* p) {
    UNUSED(p);

    SubDupFinderApp* app = app_alloc();
    ui_setup_views(app);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewSubmenu);
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    app_free(app);
    return 0;
}
