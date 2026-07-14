
#include "ui.h"

#include <notification/notification_messages.h>

#define LOG_TAG "gs1_rfid_parser_scan_scene"

static int32_t scan_thread_callback(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);

    UI* ui = context;
    ui->scan_scene->thread_state = ScanThreadRunning;

    while(ui->scan_scene->thread_state == ScanThreadRunning) {
        int32_t result = uhf_u107_single_poll(ui->uhf_device, ui->epc_data);
        FURI_LOG_D(LOG_TAG, "Single Poll Response: %ld", result);

        if(result > 0) {
            FURI_LOG_D(LOG_TAG, "PC: %04X", ui->epc_data->pc);
            ui->user_mem_data->is_valid = false;

            if(ui->epc_data->pc & UHF_EPC_USER_MEM_FLAG) {
                // At least one tag was found, attempt to get the user memory and display the parse results
                result = uhf_u107_read_user_memory(ui->uhf_device, ui->user_mem_data);
                FURI_LOG_D(LOG_TAG, "Read User Response: %ld", result);

                if(result < 0) {
                    FURI_LOG_E(
                        LOG_TAG,
                        "Error returned by UHF reader while reading user memory: %ld",
                        result);
                }
            }

            scene_manager_next_scene(ui->scene_manager, ParseDisplay);
        } else {
            FURI_LOG_E(
                LOG_TAG, "Error returned by UHF reader while polling for tags: %ld", result);
        }

        furi_delay_ms(100);
    }

    ui->scan_scene->thread_state = ScanThreadIdle;
    return 0;
}

void scan_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    notification_message(ui->notifications, &sequence_blink_start_cyan);

    widget_reset(ui->scan_scene->scan_widget);
    widget_add_string_element(
        ui->scan_scene->scan_widget,
        0,
        0,
        AlignLeft,
        AlignTop,
        FontPrimary,
        "Looking for UHF Tags");

    view_dispatcher_switch_to_view(ui->view_dispatcher, View_ScanDisplay);

    furi_thread_start(ui->scan_scene->scan_thread);
}

bool scan_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }

    // Default event handler
    return false;
}

void scan_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    ui->scan_scene->thread_state = ScanThreadStopping;

    // Prevent joining when the exit is triggered by a tag being found
    if(furi_thread_get_current() != ui->scan_scene->scan_thread) {
        furi_thread_join(ui->scan_scene->scan_thread);
    }

    widget_reset(ui->scan_scene->scan_widget);
    notification_message(ui->notifications, &sequence_blink_stop);
}

ScanScene* scan_scene_alloc(UI* context) {
    FURI_LOG_T(LOG_TAG, __func__);

    ScanScene* scan_scene = malloc(sizeof(ScanScene));
    scan_scene->scan_widget = widget_alloc();

    scan_scene->scan_thread = furi_thread_alloc();
    furi_thread_set_name(scan_scene->scan_thread, "UHFScanWorker");
    furi_thread_set_context(scan_scene->scan_thread, context);
    furi_thread_set_priority(scan_scene->scan_thread, FuriThreadPriorityHighest);
    furi_thread_set_stack_size(scan_scene->scan_thread, 8 * 1024);
    furi_thread_set_callback(scan_scene->scan_thread, scan_thread_callback);
    scan_scene->thread_state = ScanThreadIdle;

    return scan_scene;
}

void scan_scene_free(ScanScene* scan_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(scan_scene);
    furi_assert(scan_scene->thread_state == ScanThreadIdle);

    widget_free(scan_scene->scan_widget);
    furi_thread_free(scan_scene->scan_thread);
    free(scan_scene);
}
