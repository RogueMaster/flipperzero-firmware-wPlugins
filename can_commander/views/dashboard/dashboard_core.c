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
            /* Zero the entire model including the union */
            memset(model, 0, sizeof(AppDashboardModel));

            model->mode = AppDashboardNone;

            strncpy(model->title, title ? title : "", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';

            if(label) {
                strncpy(model->label, label, sizeof(model->label) - 1U);
            } else {
                strncpy(model->label, "Waiting for response", sizeof(model->label) - 1U);
            }
            model->label[sizeof(model->label) - 1U] = '\0';

            if(value) {
                strncpy(model->value, value, sizeof(model->value) - 1U);
            } else {
                strncpy(model->value, "--", sizeof(model->value) - 1U);
            }
            model->value[sizeof(model->value) - 1U] = '\0';

            if(unit) {
                strncpy(model->unit, unit, sizeof(model->unit) - 1U);
                model->unit[sizeof(model->unit) - 1U] = '\0';
            }

            if(note) {
                strncpy(model->note, note, sizeof(model->note) - 1U);
                model->note[sizeof(model->note) - 1U] = '\0';
            }
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
    case CcToolReplay:
        return AppDashboardReplay;
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
    case AppDashboardReplay:
        dashboard_apply_template(app, "REPLAY", "State", "IDLE", "", "Waiting for tool ready");
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
        {
            model->mode = mode;
            if(mode == AppDashboardWrite) {
                model->write_dlc = 8U;
                model->write_count_cfg = 1U;
                model->write_interval_ms_cfg = 250U;
            } else if(mode == AppDashboardCustomInject) {
                model->custom_selected_slot = app_custom_inject_get_active_slot(app);
                for(uint8_t i = 0U; i < 5U; i++) {
                    snprintf(
                        model->custom_slot_name[i],
                        sizeof(model->custom_slot_name[i]),
                        "Slot%u",
                        (unsigned)(i + 1U));
                }
            } else if(mode == AppDashboardReplay) {
                model->replay_max_frames = 384U;
                model->replay_count_cfg = 1U;
            } else if(mode == AppDashboardDbcDecode) {
                model->dbc_signal_count = 0U;
                model->dbc_signal_selected = 0U;
                model->mode_page = 0U;
                memset(model->dbc_signals, 0, sizeof(model->dbc_signals));

                uint8_t count = 0U;
                for(uint8_t i = 0U; i < APP_DBC_CFG_MAX_SIGNALS && app->dbc_config_signals; i++) {
                    const AppDbcSignalCache* signal = &app->dbc_config_signals[i];
                    if(!signal->used || count >= APP_DBC_CFG_MAX_SIGNALS) {
                        continue;
                    }

                    DashboardDbcEntry* slot = &model->dbc_signals[count];
                    memset(slot, 0, sizeof(DashboardDbcEntry));
                    slot->sid = signal->def.sid;
                    slot->bus = signal->def.bus;
                    slot->frame_id = signal->def.id;
                    slot->in_range = true;
                    if(signal->signal_name[0] != '\0') {
                        strncpy(slot->signal_name, signal->signal_name, sizeof(slot->signal_name) - 1U);
                        slot->signal_name[sizeof(slot->signal_name) - 1U] = '\0';
                    } else {
                        snprintf(slot->signal_name, sizeof(slot->signal_name), "SID%u", (unsigned)signal->def.sid);
                        slot->signal_name[sizeof(slot->signal_name) - 1U] = '\0';
                    }
                    strncpy(slot->unit, signal->def.unit, sizeof(slot->unit) - 1U);
                    slot->unit[sizeof(slot->unit) - 1U] = '\0';
                    count++;
                }

                model->dbc_signal_count = count;
            }
        },
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
    case AppDashboardReplay:
        if(event->type == CcEventTypeTool && event->data.tool.tool == CcToolReplay) {
            dashboard_update_replay(app, event);
            return true;
        }
        return false;
    case AppDashboardNone:
    default:
        return false;
    }
}
