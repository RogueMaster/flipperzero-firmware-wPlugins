#include "dashboard_i.h"

#include <stdio.h>
#include <string.h>

static AppDashboardMode dashboard_get_mode(App* app) {
    AppDashboardMode mode = AppDashboardNone;
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
        mode = app->dashboard_mode;
        furi_mutex_release(app->mutex);
    }

    return mode;
}

size_t dashboard_model_size(void) {
    return sizeof(AppDashboardModel);
}

void dashboard_view_draw(Canvas* canvas, void* model) {
    const AppDashboardModel* dashboard = model;
    canvas_clear(canvas);

    if(dashboard_read_draw(canvas, dashboard)) {
        return;
    }

    if(dashboard_bittrack_draw(canvas, dashboard)) {
        return;
    }

    dashboard_metric_draw(canvas, dashboard);
}

bool dashboard_view_input(InputEvent* event, void* context) {
    App* app = context;
    if(!app || !event) {
        return false;
    }

    if(event->key == InputKeyBack) {
        return false;
    }

    if(dashboard_read_input(app, event)) {
        return true;
    }

    if(dashboard_bittrack_input(app, event)) {
        return true;
    }

    if(dashboard_metric_input(app, event)) {
        return true;
    }

    return false;
}

void dashboard_apply_template(
    App* app,
    const char* title,
    const char* label,
    const char* value,
    const char* unit,
    const char* note) {
    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            strncpy(model->title, title ? title : "", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            model->mode = AppDashboardNone;
            strncpy(model->label, "Waiting for response", sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';
            strncpy(model->value, "--", sizeof(model->value) - 1U);
            model->value[sizeof(model->value) - 1U] = '\0';
            model->unit[0] = '\0';
            model->note[0] = '\0';
            model->bit_row[0][0] = '\0';
            model->bit_row[1][0] = '\0';
            model->bit_row[2][0] = '\0';
            model->bit_row[3][0] = '\0';
            model->bit_id_line[0] = '\0';
            model->bittrack_have_live = false;
            model->bittrack_have_base = false;
            memset(model->bittrack_live, 0, sizeof(model->bittrack_live));
            memset(model->bittrack_base, 0, sizeof(model->bittrack_base));
            memset(model->bittrack_changed, 0, sizeof(model->bittrack_changed));
            memset(model->bittrack_frozen, 0, sizeof(model->bittrack_frozen));
            model->write_bus = CcBusCan0;
            model->write_id = 0U;
            model->write_ext = false;
            model->write_dlc = 8U;
            memset(model->write_data, 0, sizeof(model->write_data));
            model->write_count_cfg = 1U;
            model->write_interval_ms_cfg = 250U;
            model->read_head = 0U;
            model->read_count = 0U;
            model->read_selected = 0U;
            model->read_page = 0U;
            model->read_hold = false;
            model->read_total = 0U;
            model->read_bus0 = 0U;
            model->read_bus1 = 0U;
            model->read_std = 0U;
            model->read_ext = 0U;
            model->input_hold_mask = 0U;
            model->reverse_phase = 0U;
            model->reverse_count = 0U;
            model->reverse_selected = 0U;
            model->reverse_overflow = false;
            memset(model->reverse_ids, 0, sizeof(model->reverse_ids));
            memset(model->reverse_ext, 0, sizeof(model->reverse_ext));
            memset(model->reverse_byte_mask, 0, sizeof(model->reverse_byte_mask));
            memset(model->reverse_flash_until_ms, 0, sizeof(model->reverse_flash_until_ms));
            memset(model->read_frames, 0, sizeof(model->read_frames));
            model->mode_page = 0U;

            model->speed_has_sample = false;
            model->speed_last_bus = CcBusCan0;
            model->speed_last_rate = 0U;
            model->speed_min_rate = 0U;
            model->speed_max_rate = 0U;
            model->speed_sum_rate = 0U;
            model->speed_total_samples = 0U;
            model->speed_head = 0U;
            model->speed_count = 0U;
            model->speed_selected = 0U;
            memset(model->speed_samples, 0, sizeof(model->speed_samples));

            model->val_selected_byte = 0U;
            model->val_total_changes = 0U;
            model->val_head = 0U;
            model->val_count = 0U;
            model->val_selected = 0U;
            memset(model->val_known, 0, sizeof(model->val_known));
            memset(model->val_bytes, 0, sizeof(model->val_bytes));
            memset(model->val_byte_changes, 0, sizeof(model->val_byte_changes));
            memset(model->val_changes, 0, sizeof(model->val_changes));

            model->unique_total = 0U;
            model->unique_has_last = false;
            model->unique_head = 0U;
            model->unique_count = 0U;
            model->unique_selected = 0U;
            memset(&model->unique_last, 0, sizeof(model->unique_last));
            memset(model->unique_entries, 0, sizeof(model->unique_entries));

            model->dbc_has_latest = false;
            model->dbc_head = 0U;
            model->dbc_count = 0U;
            model->dbc_selected = 0U;
            memset(&model->dbc_latest, 0, sizeof(model->dbc_latest));
            memset(model->dbc_entries, 0, sizeof(model->dbc_entries));
            model->obd_dtc_active = false;
            model->obd_dtc_complete = false;
            model->obd_dtc_page = 0U;
            memset(model->obd_dtc_selected, 0, sizeof(model->obd_dtc_selected));
            memset(model->obd_dtc_count, 0, sizeof(model->obd_dtc_count));
            memset(model->obd_dtc_codes, 0, sizeof(model->obd_dtc_codes));
            memset(model->obd_dtc_cat_counts, 0, sizeof(model->obd_dtc_cat_counts));

            model->custom_selected_slot = app_custom_inject_get_active_slot(app);
            model->custom_pending = false;
            model->custom_pending_slot = 0U;
            model->custom_pending_remaining = 0U;
            model->custom_pending_interval_ms = 0U;
            model->custom_recent_head = 0U;
            model->custom_recent_count = 0U;
            memset(model->custom_slot_used, 0, sizeof(model->custom_slot_used));
            memset(model->custom_slot_ready, 0, sizeof(model->custom_slot_ready));
            memset(model->custom_slot_name, 0, sizeof(model->custom_slot_name));
            memset(model->custom_slot_bus, 0, sizeof(model->custom_slot_bus));
            memset(model->custom_slot_id, 0, sizeof(model->custom_slot_id));
            memset(model->custom_slot_ext, 0, sizeof(model->custom_slot_ext));
            memset(model->custom_recent, 0, sizeof(model->custom_recent));
            for(uint8_t i = 0U; i < 5U; i++) {
                snprintf(
                    model->custom_slot_name[i],
                    sizeof(model->custom_slot_name[i]),
                    "Slot%u",
                    (unsigned)(i + 1U));
            }

            if(label) {
                strncpy(model->label, label, sizeof(model->label) - 1U);
                model->label[sizeof(model->label) - 1U] = '\0';
            }

            if(value) {
                strncpy(model->value, value, sizeof(model->value) - 1U);
                model->value[sizeof(model->value) - 1U] = '\0';
            }

            if(unit) {
                strncpy(model->unit, unit, sizeof(model->unit) - 1U);
                model->unit[sizeof(model->unit) - 1U] = '\0';
            }

            if(note) {
                strncpy(model->note, note, sizeof(model->note) - 1U);
                model->note[sizeof(model->note) - 1U] = '\0';
            }

            model->counter = 0U;
        },
        true);
}

AppDashboardMode dashboard_mode_for_tool(CcToolId tool_id) {
    switch(tool_id) {
    case CcToolReadAll:
        return AppDashboardReadAll;
    case CcToolFiltered:
        return AppDashboardFiltered;
    case CcToolWrite:
        return AppDashboardWrite;
    case CcToolSpeed:
        return AppDashboardSpeed;
    case CcToolValtrack:
        return AppDashboardValtrack;
    case CcToolUniqueIds:
        return AppDashboardUniqueIds;
    case CcToolBittrack:
        return AppDashboardBittrack;
    case CcToolReverse:
        return AppDashboardReverse;
    case CcToolObdPid:
        return AppDashboardObdPid;
    case CcToolDbcDecode:
        return AppDashboardDbcDecode;
    case CcToolCustomInject:
        return AppDashboardCustomInject;
    default:
        return AppDashboardNone;
    }
}

static void dashboard_init_mode(App* app, AppDashboardMode mode) {
    switch(mode) {
    case AppDashboardReadAll:
        dashboard_apply_template(app, "READ ALL", "Frames", "0", "", "Waiting for CAN frames");
        break;
    case AppDashboardFiltered:
        dashboard_apply_template(app, "FILTERED", "Frames", "0", "", "Waiting for matched frames");
        break;
    case AppDashboardWrite:
        dashboard_apply_template(app, "WRITE", "Sent", "0", "", "No TX sent yet");
        break;
    case AppDashboardSpeed:
        dashboard_apply_template(app, "SPEED TEST", "Rate", "--", "msg/s", "Waiting for 1s sample");
        break;
    case AppDashboardValtrack:
        dashboard_apply_template(app, "VAL TRACK", "Last Change", "--", "", "Waiting for byte changes");
        break;
    case AppDashboardUniqueIds:
        dashboard_apply_template(app, "UNIQUE IDS", "Found", "0", "ids", "Waiting for new IDs");
        break;
    case AppDashboardBittrack:
        dashboard_apply_template(app, "BIT TRACK", "Tracking", "--", "", "Waiting for matching ID");
        dashboard_bittrack_prepare_template(app);
        break;
    case AppDashboardReverse:
        dashboard_apply_template(app, "BYTE WATCHER", "Phase", "Init", "", "Waiting for tool events");
        break;
    case AppDashboardObdPid:
        dashboard_apply_template(app, "OBD PID", "Waiting for response", "--", "", "");
        break;
    case AppDashboardDbcDecode:
        dashboard_apply_template(app, "DBC DECODE", "Signal", "--", "", "Waiting for decoded values");
        break;
    case AppDashboardCustomInject:
        {
            char slot_value[8] = {0};
            snprintf(
                slot_value,
                sizeof(slot_value),
                "%u",
                (unsigned)(app_custom_inject_get_active_slot(app) + 1U));
            dashboard_apply_template(
                app, "CUSTOM INJECT", "Slot", slot_value, "", "Waiting for slot data");
        }
        break;
    case AppDashboardNone:
    default:
        dashboard_apply_template(app, "CAN Commander", "Live Monitor", "--", "", "");
        break;
    }
}

void dashboard_set_mode(App* app, AppDashboardMode mode) {
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
        app->dashboard_mode = mode;
        furi_mutex_release(app->mutex);
    }

    dashboard_init_mode(app, mode);
    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        { model->mode = mode; },
        true);
}

bool dashboard_handle_event(App* app, const CcEvent* event) {
    if(!app || !event) {
        return false;
    }

    switch(dashboard_get_mode(app)) {
    case AppDashboardReadAll:
        if(event->type == CcEventTypeCanFrame) {
            dashboard_read_update(app, event, "READ ALL");
            return true;
        }
        return false;
    case AppDashboardFiltered:
        if(event->type == CcEventTypeCanFrame) {
            dashboard_read_update(app, event, "FILTERED");
            return true;
        }
        return false;
    case AppDashboardWrite:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolWrite) {
            dashboard_update_write(app, event);
            return true;
        }
        return false;
    case AppDashboardSpeed:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolSpeed) {
            dashboard_update_speed(app, event);
            return true;
        }
        return false;
    case AppDashboardValtrack:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolValtrack) {
            dashboard_update_valtrack(app, event);
            return true;
        }
        return false;
    case AppDashboardUniqueIds:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolUniqueIds) {
            dashboard_update_unique_ids(app, event);
            return true;
        }
        return false;
    case AppDashboardBittrack:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolBittrack) {
            dashboard_bittrack_update(app, event);
            return true;
        }
        return false;
    case AppDashboardReverse:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolReverse) {
            dashboard_update_reverse(app, event);
            return true;
        }
        return false;
    case AppDashboardObdPid:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolObdPid) {
            dashboard_update_obd(app, event);
            return true;
        }
        return false;
    case AppDashboardDbcDecode:
        if(event->type == CcEventTypeDbcDecode) {
            dashboard_update_dbc_decode(app, event);
            return true;
        }
        return false;
    case AppDashboardCustomInject:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolCustomInject) {
            dashboard_update_custom_inject(app, event);
            return true;
        }
        return false;
    case AppDashboardNone:
    default:
        return false;
    }
}
