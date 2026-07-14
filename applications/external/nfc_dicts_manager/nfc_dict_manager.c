#include "nfc_dict_manager.h"
#include "nfc_dict_manager_utils.h"

void (*const nfc_dict_manager_on_enter_handlers[])(void*) = {
    [NfcDictManagerSceneStart] = nfc_dict_manager_scene_start_on_enter,
    [NfcDictManagerSceneBackup] = nfc_dict_manager_scene_backup_on_enter,
    [NfcDictManagerSceneSelect] = nfc_dict_manager_scene_select_on_enter,
    [NfcDictManagerSceneSelectType] = nfc_dict_manager_scene_select_type_on_enter,
    [NfcDictManagerSceneMerge] = nfc_dict_manager_scene_merge_on_enter,
    [NfcDictManagerSceneMergeSelectA] = nfc_dict_manager_scene_merge_select_a_on_enter,
    [NfcDictManagerSceneMergeSelectB] = nfc_dict_manager_scene_merge_select_b_on_enter,
    [NfcDictManagerSceneOptimize] = nfc_dict_manager_scene_optimize_on_enter,
    [NfcDictManagerSceneAbout] = nfc_dict_manager_scene_about_on_enter,
};

bool (*const nfc_dict_manager_on_event_handlers[])(void*, SceneManagerEvent) = {
    [NfcDictManagerSceneStart] = nfc_dict_manager_scene_start_on_event,
    [NfcDictManagerSceneBackup] = nfc_dict_manager_scene_backup_on_event,
    [NfcDictManagerSceneSelect] = nfc_dict_manager_scene_select_on_event,
    [NfcDictManagerSceneSelectType] = nfc_dict_manager_scene_select_type_on_event,
    [NfcDictManagerSceneMerge] = nfc_dict_manager_scene_merge_on_event,
    [NfcDictManagerSceneMergeSelectA] = nfc_dict_manager_scene_merge_select_a_on_event,
    [NfcDictManagerSceneMergeSelectB] = nfc_dict_manager_scene_merge_select_b_on_event,
    [NfcDictManagerSceneOptimize] = nfc_dict_manager_scene_optimize_on_event,
    [NfcDictManagerSceneAbout] = nfc_dict_manager_scene_about_on_event,
};

void (*const nfc_dict_manager_on_exit_handlers[])(void*) = {
    [NfcDictManagerSceneStart] = nfc_dict_manager_scene_start_on_exit,
    [NfcDictManagerSceneBackup] = nfc_dict_manager_scene_backup_on_exit,
    [NfcDictManagerSceneSelect] = nfc_dict_manager_scene_select_on_exit,
    [NfcDictManagerSceneSelectType] = nfc_dict_manager_scene_select_type_on_exit,
    [NfcDictManagerSceneMerge] = nfc_dict_manager_scene_merge_on_exit,
    [NfcDictManagerSceneMergeSelectA] = nfc_dict_manager_scene_merge_select_a_on_exit,
    [NfcDictManagerSceneMergeSelectB] = nfc_dict_manager_scene_merge_select_b_on_exit,
    [NfcDictManagerSceneOptimize] = nfc_dict_manager_scene_optimize_on_exit,
    [NfcDictManagerSceneAbout] = nfc_dict_manager_scene_about_on_exit,
};

static const SceneManagerHandlers nfc_dict_manager_scene_handlers = {
    .on_enter_handlers = nfc_dict_manager_on_enter_handlers,
    .on_event_handlers = nfc_dict_manager_on_event_handlers,
    .on_exit_handlers = nfc_dict_manager_on_exit_handlers,
    .scene_num = NfcDictManagerSceneCount,
};

static NfcDictManager* nfc_dict_manager_alloc() {
    NfcDictManager* app = malloc(sizeof(NfcDictManager));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_dict_manager_back_event_callback);

    app->scene_manager = scene_manager_alloc(&nfc_dict_manager_scene_handlers, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcDictManagerViewSubmenu, submenu_get_view(app->submenu));

    app->file_browser_result = furi_string_alloc();
    app->file_browser = file_browser_alloc(app->file_browser_result);
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcDictManagerViewFileBrowser,
        file_browser_get_view(app->file_browser));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcDictManagerViewTextBox, text_box_get_view(app->text_box));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcDictManagerViewSelectType, dialog_ex_get_view(app->dialog_ex));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcDictManagerViewPopup, popup_get_view(app->popup));

    app->selected_dict_a = furi_string_alloc();
    app->selected_dict_b = furi_string_alloc();
    app->current_dict = furi_string_alloc();
    app->text_box_content = furi_string_alloc();
    app->backup_stage = 0;
    app->backup_success = false;
    app->merge_stage = 0;
    app->optimize_stage = 0;

    app->backup_timer =
        furi_timer_alloc(nfc_dict_manager_backup_timer_callback, FuriTimerTypeOnce, app);
    app->success_timer =
        furi_timer_alloc(nfc_dict_manager_success_timer_callback, FuriTimerTypeOnce, app);
    app->merge_timer =
        furi_timer_alloc(nfc_dict_manager_merge_timer_callback, FuriTimerTypeOnce, app);
    app->optimize_timer =
        furi_timer_alloc(nfc_dict_manager_optimize_timer_callback, FuriTimerTypeOnce, app);

    return app;
}

static void nfc_dict_manager_free(NfcDictManager* app) {
    furi_timer_free(app->backup_timer);
    furi_timer_free(app->success_timer);
    furi_timer_free(app->merge_timer);
    furi_timer_free(app->optimize_timer);

    furi_string_free(app->selected_dict_a);
    furi_string_free(app->selected_dict_b);
    furi_string_free(app->current_dict);
    furi_string_free(app->file_browser_result);
    furi_string_free(app->text_box_content);

    view_dispatcher_remove_view(app->view_dispatcher, NfcDictManagerViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, NfcDictManagerViewFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, NfcDictManagerViewSelectType);
    view_dispatcher_remove_view(app->view_dispatcher, NfcDictManagerViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, NfcDictManagerViewTextBox);

    submenu_free(app->submenu);
    file_browser_free(app->file_browser);
    dialog_ex_free(app->dialog_ex);
    popup_free(app->popup);
    text_box_free(app->text_box);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t nfc_dict_manager_app(void* p) {
    UNUSED(p);

    NfcDictManager* app = nfc_dict_manager_alloc();

    nfc_dict_manager_create_directories(app->storage);

    scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    nfc_dict_manager_free(app);

    return 0;
}
