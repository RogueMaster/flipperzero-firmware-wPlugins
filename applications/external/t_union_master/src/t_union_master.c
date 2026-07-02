#include "t_union_master_i.h"

static bool tum_app_custom_event_cb(void* ctx, uint32_t event) {
    furi_assert(ctx);
    TUM_App* app = ctx;

    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool tum_app_back_event_cb(void* ctx) {
    furi_assert(ctx);
    TUM_App* app = ctx;

    return scene_manager_handle_back_event(app->scene_manager);
}

static void tum_app_tick_event_cb(void* ctx) {
    furi_assert(ctx);
    TUM_App* app = ctx;

    scene_manager_handle_tick_event(app->scene_manager);
}

TUM_App* tum_app_alloc() {
    TUM_App* app = malloc(sizeof(TUM_App));

    app->nfc = nfc_alloc();
    app->card_message = t_union_msg_alloc();
    app->card_message_ext = t_union_msgext_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, tum_app_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, tum_app_back_event_cb);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, tum_app_tick_event_cb, 100);

    // app->storage = furi_record_open(RECORD_STORAGE);

    cfont_alloc(FontSize12px);

    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->scene_manager = scene_manager_alloc(&tum_scene_handlers, app);

    app->submenu = submenu_alloc_utf8();
    view_dispatcher_add_view(app->view_dispatcher, TUM_ViewMenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc_utf8();
    view_dispatcher_add_view(app->view_dispatcher, TUM_ViewPopup, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TUM_ViewWidget, widget_get_view(app->widget));

    app->query_progress = query_progress_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TUM_ViewQureyProgress, query_progress_get_view(app->query_progress));

    app->card_info = card_info_alloc(app->card_message, app->card_message_ext);
    view_dispatcher_add_view(
        app->view_dispatcher, TUM_ViewCardInfo, card_info_get_view(app->card_info));

    app->transaction_list = transaction_list_alloc(app->card_message, app->card_message_ext);
    view_dispatcher_add_view(
        app->view_dispatcher,
        TUM_ViewTransactionList,
        transaction_list_get_view(app->transaction_list));

    app->travel_list = travel_list_alloc(app->card_message, app->card_message_ext);
    view_dispatcher_add_view(
        app->view_dispatcher, TUM_ViewTravelList, travel_list_get_view(app->travel_list));

    app->transaction_detail = transaction_detail_alloc(app->card_message, app->card_message_ext);
    view_dispatcher_add_view(
        app->view_dispatcher,
        TUM_ViewTransactionDetail,
        transaction_detail_get_view(app->transaction_detail));

    app->travel_detail = travel_detail_alloc(app->card_message, app->card_message_ext);
    view_dispatcher_add_view(
        app->view_dispatcher, TUM_ViewTravelDetail, travel_detail_get_view(app->travel_detail));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->query_worker = tum_query_worker_alloc(app->card_message, app->card_message_ext);

    return app;
}

void tum_app_free(TUM_App* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewMenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewQureyProgress);
    query_progress_free(app->query_progress);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewCardInfo);
    card_info_free(app->card_info);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewTransactionList);
    transaction_list_free(app->transaction_list);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewTravelList);
    travel_list_free(app->travel_list);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewTransactionDetail);
    transaction_detail_free(app->transaction_detail);

    view_dispatcher_remove_view(app->view_dispatcher, TUM_ViewTravelDetail);
    travel_detail_free(app->travel_detail);

    cfont_free();

    // furi_record_close(RECORD_STORAGE);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    nfc_free(app->nfc);

    tum_query_worker_free(app->query_worker);

    t_union_msg_free(app->card_message);
    t_union_msgext_free(app->card_message_ext);

    free(app);
}

int32_t tum_app(void* p) {
    UNUSED(p);

    TUM_App* app = tum_app_alloc();
    FURI_LOG_I(TAG, "App Start");

    scene_manager_next_scene(app->scene_manager, TUM_SceneStart);
    view_dispatcher_run(app->view_dispatcher);

    tum_app_free(app);

    return 0;
}
