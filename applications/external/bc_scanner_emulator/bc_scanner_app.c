#include "bc_scanner_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <expansion/expansion.h>
#include <string.h>
#define TAG        "BarCodeScanner"
#define WORKER_TAG TAG "App"

static bool bc_scanner_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BarCodeApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bc_scanner_app_back_event_callback(void* context) {
    furi_assert(context);
    BarCodeApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void bc_scanner_app_tick_event_callback(void* context) {
    furi_assert(context);
    BarCodeApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/**
 * RPC event handler. Runs in the RPC thread; incoming bytes are forwarded to USB CDC and the
 * command is confirmed so the phone learns whether delivery succeeded (FR-015).
 */
static void bc_scanner_app_rpc_callback(const RpcAppSystemEvent* event, void* context) {
    furi_assert(context);
    BarCodeApp* app = context;
    furi_assert(app->rpc);

    if(event->type == RpcAppEventTypeSessionClose) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        rpc_system_app_set_callback(app->rpc, NULL, NULL);
        app->rpc = NULL;
    } else if(event->type == RpcAppEventTypeAppExit) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        rpc_system_app_confirm(app->rpc, true);
    } else if(event->type == RpcAppEventTypeDataExchange) {
        bool ok = false;
        if(event->data.type == RpcAppSystemEventDataTypeBytes) {
            app->rpc_last_size = event->data.bytes.size;
            app->rpc_status = BarCodeRpcStatusReceived;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BarCodeCustomEventRpcStatusUpdate);

            if(app->bc_scanner_script) {
                ok = bc_scanner_script_send_data(
                    app->bc_scanner_script, event->data.bytes.ptr, event->data.bytes.size);
            }
            app->rpc_status = ok ? BarCodeRpcStatusSent : BarCodeRpcStatusError;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BarCodeCustomEventRpcStatusUpdate);
        }
        rpc_system_app_confirm(app->rpc, ok);
    } else {
        rpc_system_app_confirm(app->rpc, false);
    }
}

BarCodeApp* bc_scanner_app_alloc(char* arg) {
    FURI_LOG_D(WORKER_TAG, "Start AppAlloc");
    BarCodeApp* app = malloc(sizeof(BarCodeApp));

    app->file_path = furi_string_alloc();
    app->bc_scanner_script = NULL;
    app->rpc = NULL;
    app->rpc_status = BarCodeRpcStatusWaiting;
    app->rpc_last_size = 0;

    // RPC mode is requested by the phone through StartRequest with args="RPC <hex-ptr>".
    uint32_t rpc_ctx = 0;
    bool rpc_mode = (arg && sscanf(arg, "RPC %lX", &rpc_ctx) == 1);

    if(!rpc_mode && arg && strlen(arg)) {
        furi_string_set(app->file_path, arg);
        FURI_LOG_D(WORKER_TAG, "File Path Setted");
    }

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&bc_scanner_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, bc_scanner_app_tick_event_callback, 500);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bc_scanner_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bc_scanner_app_back_event_callback);

    // Custom Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BarCodeAppViewError, widget_get_view(app->widget));

    app->bc_scanner_view = bc_scanner_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BarCodeAppViewWork, bc_scanner_get_view(app->bc_scanner_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    if(rpc_mode) {
        app->rpc = (RpcAppSystem*)rpc_ctx;
        rpc_system_app_set_callback(app->rpc, bc_scanner_app_rpc_callback, app);
        rpc_system_app_send_started(app->rpc);

        // USB CDC is needed to relay received bytes; a locked USB keeps the app in RPC mode but
        // every data exchange is confirmed with an error.
        if(!furi_hal_usb_is_locked()) {
            app->bc_scanner_script = bc_scanner_script_open_rpc();
        }
        scene_manager_next_scene(app->scene_manager, BarCodeSceneRpc);
    } else if(furi_hal_usb_is_locked()) {
        app->error = BarCodeAppErrorCloseRpc;
        scene_manager_next_scene(app->scene_manager, BarCodeSceneError);
    } else {
        if(!furi_string_empty(app->file_path)) {
            scene_manager_next_scene(app->scene_manager, BarCodeSceneWork);
        } else {
            furi_string_set(app->file_path, BC_SCANNER_APP_PATH_FOLDER);
            scene_manager_next_scene(app->scene_manager, BarCodeSceneFileSelect);
        }
    }
    FURI_LOG_D(WORKER_TAG, "End AppAlloc");
    return app;
}

void bc_scanner_app_free(BarCodeApp* app) {
    furi_assert(app);

    if(app->bc_scanner_script) {
        bc_scanner_script_close(app->bc_scanner_script);
        app->bc_scanner_script = NULL;
    }

    if(app->rpc) {
        rpc_system_app_set_callback(app->rpc, NULL, NULL);
        rpc_system_app_send_exited(app->rpc);
        app->rpc = NULL;
    }

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, BarCodeAppViewWork);
    bc_scanner_free(app->bc_scanner_view);

    // Custom Widget
    view_dispatcher_remove_view(app->view_dispatcher, BarCodeAppViewError);
    widget_free(app->widget);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);

    furi_string_free(app->file_path);

    free(app);
    FURI_LOG_D(WORKER_TAG, "App Free");
}

int32_t bc_scanner_app(void* p) {
    FURI_LOG_D(WORKER_TAG, "Start App");
    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    BarCodeApp* bar_code_app = bc_scanner_app_alloc((char*)p);

    view_dispatcher_run(bar_code_app->view_dispatcher);

    bc_scanner_app_free(bar_code_app);

    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}
