#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// NFC Worker回调函数
static void nfc_worker_callback(NfcWorkerEvent event, void* context) {
    NfcApduRunner* app = context;

    // 发送自定义事件到视图分发器
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

// 运行场景弹出窗口回调
static void nfc_apdu_runner_scene_running_popup_callback(void* context) {
    NfcApduRunner* app = context;
    FURI_LOG_I("APDU_DEBUG", "弹出窗口回调");
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcApduRunnerCustomEventPopupClosed);
}

// 显示错误消息并设置回调
static void show_error_popup(NfcApduRunner* app, const char* message) {
    FURI_LOG_I("APDU_DEBUG", "显示错误弹窗: %s", message);
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_text(popup, message, 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 2000);
    popup_set_callback(popup, nfc_apdu_runner_scene_running_popup_callback);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
}

// 显示等待卡片弹出窗口
static void show_waiting_popup(NfcApduRunner* app) {
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, "NFC APDU Runner", 64, 0, AlignCenter, AlignTop);
    popup_set_text(
        popup,
        "Waiting for card...\nPlace card on Flipper's back",
        64,
        32,
        AlignCenter,
        AlignCenter);
    popup_set_timeout(popup, 0);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
}

// 显示运行中弹出窗口
static void show_running_popup(NfcApduRunner* app) {
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, "Running APDU commands", 64, 0, AlignCenter, AlignTop);
    popup_set_text(
        popup, "Please wait...\nDo not remove the card", 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 0);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);
}

// 运行场景进入回调
void nfc_apdu_runner_scene_running_on_enter(void* context) {
    NfcApduRunner* app = context;
    Popup* popup = app->popup;
    FURI_LOG_I("APDU_DEBUG", "进入运行场景");

    // 确保文件路径有效
    if(furi_string_empty(app->file_path)) {
        FURI_LOG_E("APDU_DEBUG", "文件路径为空");
        show_error_popup(app, "Invalid file path");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "文件路径: %s", furi_string_get_cstr(app->file_path));

    // 显示正在运行的弹出窗口
    FURI_LOG_I("APDU_DEBUG", "显示加载脚本弹窗");
    popup_reset(popup);
    popup_set_text(popup, "Loading script...\nPlease wait", 89, 44, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 0);
    popup_set_context(popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewPopup);

    // 解析脚本文件
    FURI_LOG_I("APDU_DEBUG", "开始解析脚本文件");
    app->script = nfc_apdu_script_parse(app->storage, furi_string_get_cstr(app->file_path));

    if(app->script == NULL) {
        FURI_LOG_E("APDU_DEBUG", "解析脚本文件失败");
        show_error_popup(app, "Failed to parse script file");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "脚本解析成功, 卡类型: %d", app->script->card_type);

    // 检查卡类型是否支持
    if(app->script->card_type == CardTypeUnknown) {
        FURI_LOG_E("APDU_DEBUG", "不支持的卡类型");
        show_error_popup(app, "Unsupported card type");
        return;
    }
    FURI_LOG_I("APDU_DEBUG", "卡类型支持");

    // 显示等待卡片弹出窗口
    show_waiting_popup(app);

    // 重置卡片丢失标志
    app->card_lost = false;
    // 设置运行标志为true，允许执行APDU命令
    app->running = true;

    // 启动NFC Worker
    nfc_worker_start(app->worker, NfcWorkerStateRunning, app->script, nfc_worker_callback, app);
}

// 运行场景事件回调
bool nfc_apdu_runner_scene_running_on_event(void* context, SceneManagerEvent event) {
    NfcApduRunner* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // 处理自定义事件
        uint32_t custom_event = event.event;

        if(custom_event == NfcWorkerEventCardDetected) {
            FURI_LOG_I("APDU_DEBUG", "卡片检测成功事件");
            show_running_popup(app);
            consumed = true;
        } else if(custom_event == NfcWorkerEventSuccess) {
            FURI_LOG_I("APDU_DEBUG", "操作成功事件");

            // 获取响应结果
            nfc_worker_get_responses(app->worker, &app->responses, &app->response_count);

            // 切换到结果场景
            scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneResults);
            consumed = true;
        } else if(custom_event == NfcWorkerEventFail) {
            FURI_LOG_E("APDU_DEBUG", "操作失败事件");
            // 停止NFC Worker
            nfc_worker_stop(app->worker);

            // 获取自定义错误信息
            const char* error_message = nfc_worker_get_error_message(app->worker);
            if(error_message) {
                show_error_popup(app, error_message);
            } else {
                show_error_popup(
                    app, "Failed to detect card or run APDU commands\nCheck the log for details");
            }

            consumed = true;
        } else if(custom_event == NfcWorkerEventCardLost) {
            FURI_LOG_E("APDU_DEBUG", "卡片丢失事件");
            // 不停止NFC Worker，而是显示等待卡片弹窗
            show_waiting_popup(app);
            consumed = true;
        } else if(custom_event == NfcWorkerEventAborted) {
            FURI_LOG_I("APDU_DEBUG", "操作被中止事件");
            // 停止NFC Worker
            nfc_worker_stop(app->worker);

            // 检查是否已经显示了错误信息
            const char* error_message = nfc_worker_get_error_message(app->worker);
            if(!error_message) {
                // 只有在没有错误信息的情况下才显示用户取消的消息
                show_error_popup(app, "Operation cancelled by user");
            }

            consumed = true;
        } else if(custom_event == NfcApduRunnerCustomEventPopupClosed) {
            FURI_LOG_I("APDU_DEBUG", "弹出窗口关闭事件");
            // 如果是错误弹窗关闭，返回到文件选择场景
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(custom_event == NfcApduRunnerCustomEventViewExit) {
            FURI_LOG_I("APDU_DEBUG", "视图退出事件");
            // 用户请求退出，设置运行标志为false
            app->running = false;
            nfc_worker_stop(app->worker);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_I("APDU_DEBUG", "返回事件");
        // 用户按下返回键，设置运行标志为false并停止worker
        app->running = false;
        nfc_worker_stop(app->worker);
        consumed = false; // 不消费事件，让场景管理器处理返回
    }

    return consumed;
}

// 运行场景退出回调
void nfc_apdu_runner_scene_running_on_exit(void* context) {
    NfcApduRunner* app = context;
    FURI_LOG_I("APDU_DEBUG", "退出运行场景");

    // 设置运行标志为false，停止任何正在进行的操作
    app->running = false;

    // 停止NFC Worker
    nfc_worker_stop(app->worker);

    // 释放脚本资源
    if(app->script) {
        nfc_apdu_script_free(app->script);
        app->script = NULL;
    }

    popup_reset(app->popup);
}
