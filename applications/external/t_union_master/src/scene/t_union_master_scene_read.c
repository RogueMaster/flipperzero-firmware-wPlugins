#include "../t_union_master_i.h"

NfcCommand tum_scene_read_poller_cb(TUnionPollerEvent event, void* ctx) {
    TUM_App* app = ctx;
    NfcCommand command = NfcCommandContinue;

    switch(event) {
    case TUnionPollerEventTypeSuccess:
        // 读卡成功
        command = NfcCommandStop;
        view_dispatcher_send_custom_event(app->view_dispatcher, TUM_CustomEventWorkerSuccess);
        break;
    case TUnionPollerEventTypeFail:
        // 读卡失败
        app->poller_error = t_union_poller_get_error(app->poller);
        view_dispatcher_send_custom_event(app->view_dispatcher, TUM_CustomEventWorkerFail);
        command = NfcCommandStop;
        break;
    case TUnionPollerEventTypeDetected:
        // 识别卡片
        view_dispatcher_send_custom_event(app->view_dispatcher, TUM_CustomEventWorkerDetected);
        command = NfcCommandContinue;
        break;
    }
    return command;
}

void tum_scene_read_on_enter(void* ctx) {
    TUM_App* app = ctx;
    Popup* popup = app->popup;

    popup_set_icon(popup, 0, 8, &I_NFC_manual_60x50);
    popup_set_text(popup, "请将卡片置\n于设备背面", 128, 32, AlignRight, AlignCenter);

    app->poller = t_union_poller_alloc(app->nfc, app->card_message);
    t_union_poller_start(app->poller, tum_scene_read_poller_cb, app);

    notification_message(app->notifications, &tum_sequence_blink_start_cyan);
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewPopup);
}

bool tum_scene_read_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TUM_CustomEventWorkerSuccess:
            scene_manager_next_scene(app->scene_manager, TUM_SceneQuery);
            consumed = true;
            break;
        case TUM_CustomEventWorkerFail:
            scene_manager_next_scene(app->scene_manager, TUM_SceneFail);
            consumed = true;
            break;
        case TUM_CustomEventWorkerDetected:
            popup_reset(app->popup);
            popup_set_text(app->popup, "正在读取...", 85, 27, AlignCenter, AlignTop);
            popup_set_icon(app->popup, 12, 23, &A_Loading_24);
            break;
        }
    }

    return consumed;
}

void tum_scene_read_on_exit(void* ctx) {
    TUM_App* app = ctx;

    t_union_poller_stop(app->poller);
    t_union_poller_free(app->poller);
    popup_reset(app->popup);

    notification_message(app->notifications, &sequence_blink_stop);
}
