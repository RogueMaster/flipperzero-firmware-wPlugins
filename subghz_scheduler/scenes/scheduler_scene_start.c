#include "src/scheduler_app_i.h"
#include <furi_hal_power.h>
#include <furi_hal_usb.h>
#include <furi_hal.h>
#include <dolphin/dolphin.h>
#include "scheduler_scene_loadfile.h"
#include <devices/devices.h>

#include <string.h>

#define TAG "Sub-GHzSchedulerSceneStart"

typedef uint8_t (*SchedulerGetIdxFn)(const Scheduler* scheduler);
typedef void (*SchedulerSetIdxFn)(Scheduler* scheduler, uint8_t idx);

static uint8_t clamp_u8(uint8_t v, uint8_t max_exclusive) {
    return (max_exclusive == 0) ? 0 : (v < max_exclusive ? v : 0);
}

static VariableItem* add_scheduler_option_item(
    VariableItemList* list,
    SchedulerApp* app,
    const char* label,
    uint8_t count,
    VariableItemChangeCallback on_change,
    SchedulerGetIdxFn get_idx,
    SchedulerSetIdxFn set_idx,
    const char* const* text_table) {
    furi_assert(list);
    furi_assert(app);
    furi_assert(label);
    furi_assert(on_change);
    furi_assert(get_idx);
    furi_assert(set_idx);
    furi_assert(text_table);
    furi_assert(count > 0);

    VariableItem* item = variable_item_list_add(list, label, count, on_change, app);

    uint8_t idx = get_idx(app->scheduler);
    idx = clamp_u8(idx, count);

    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, text_table[idx]);

    set_idx(app->scheduler, idx);

    return item;
}

static uint8_t get_interval_idx(const Scheduler* s) {
    return scheduler_get_interval((Scheduler*)s);
}
static void set_interval_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_interval(s, idx);
}

static uint8_t get_timing_idx(const Scheduler* s) {
    return scheduler_get_timing_mode((Scheduler*)s);
}
static void set_timing_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_timing_mode(s, idx);
}

static uint8_t get_repeats_idx(const Scheduler* s) {
    return scheduler_get_tx_repeats((Scheduler*)s);
}
static void set_repeats_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_tx_repeats(s, idx);
}

static uint8_t get_mode_idx(const Scheduler* s) {
    return (uint8_t)scheduler_get_mode((Scheduler*)s);
}
static void set_mode_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_mode(s, (SchedulerTxMode)idx);
}

static uint8_t get_tx_delay_idx(const Scheduler* s) {
    return scheduler_get_tx_delay_index((Scheduler*)s);
}
static void set_tx_delay_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_tx_delay(s, idx);
}

static void scheduler_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    SchedulerApp* app = context;

    if(index == SchedulerStartRunEvent) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SchedulerStartRunEvent);
    } else if(index == SchedulerStartEventSelectFile) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SchedulerStartEventSelectFile);
    }
}

static void scheduler_scene_start_set_interval(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, interval_text[index]);
    scheduler_set_interval(app->scheduler, index);
}

static void scheduler_scene_start_set_timing(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, timing_mode_text[index]);
    scheduler_set_timing_mode(app->scheduler, index);
}

static void scheduler_scene_start_set_repeats(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, tx_repeats_text[index]);
    scheduler_set_tx_repeats(app->scheduler, index);
}

static void scheduler_scene_start_set_mode(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    SchedulerTxMode index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, mode_text[index]);
    scheduler_set_mode(app->scheduler, index);
}

static void scheduler_scene_start_set_tx_delay(VariableItem* item) {
    SchedulerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, tx_delay_text[index]);
    scheduler_set_tx_delay(app->scheduler, index);
}

void scheduler_scene_start_on_enter(void* context) {
    SchedulerApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    char buffer[20];

    scheduler_reset(app->scheduler);

    variable_item_list_set_enter_callback(
        var_item_list, scheduler_scene_start_var_list_enter_callback, app);

    add_scheduler_option_item(
        var_item_list,
        app,
        "Interval:",
        INTERVAL_COUNT,
        scheduler_scene_start_set_interval,
        get_interval_idx,
        set_interval_idx,
        interval_text);

    add_scheduler_option_item(
        var_item_list,
        app,
        "Timing:",
        TIMING_MODE_COUNT,
        scheduler_scene_start_set_timing,
        get_timing_idx,
        set_timing_idx,
        timing_mode_text);

    add_scheduler_option_item(
        var_item_list,
        app,
        "Repeats:",
        REPEATS_COUNT,
        scheduler_scene_start_set_repeats,
        get_repeats_idx,
        set_repeats_idx,
        tx_repeats_text);

    add_scheduler_option_item(
        var_item_list,
        app,
        "Mode:",
        SchedulerTxModeSettingsNum,
        scheduler_scene_start_set_mode,
        get_mode_idx,
        set_mode_idx,
        mode_text);

    add_scheduler_option_item(
        var_item_list,
        app,
        "TX Delay:",
        TX_DELAY_COUNT,
        scheduler_scene_start_set_tx_delay,
        get_tx_delay_idx,
        set_tx_delay_idx,
        tx_delay_text);

    VariableItem* item = variable_item_list_add(var_item_list, "Select File", 0, NULL, app);
    if(check_file_extension(furi_string_get_cstr(app->file_path))) {
        scene_manager_set_scene_state(
            app->scene_manager, SchedulerSceneStart, SchedulerStartRunEvent);
        if(scheduler_get_file_type(app->scheduler) == SchedulerFileTypeSingle) {
            variable_item_set_current_value_text(item, "[Single]");
        } else if(scheduler_get_file_type(app->scheduler) == SchedulerFileTypePlaylist) {
            snprintf(
                buffer,
                sizeof(buffer),
                "[Playlist of %d]",
                scheduler_get_list_count(app->scheduler));
            variable_item_set_current_value_text(item, buffer);
        }
    }
    variable_item_list_add(var_item_list, "Start", 0, NULL, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, SchedulerSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewVarItemList);
}

bool scheduler_scene_start_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SchedulerStartRunEvent) {
            if(!check_file_extension(furi_string_get_cstr(app->file_path))) {
                dialog_message_show_storage_error(
                    app->dialogs, "Please select\nplaylist (*.txt) or\n *.sub file!");
            } else {
                scene_manager_next_scene(app->scene_manager, SchedulerSceneRunSchedule);
            }
        } else if(event.event == SchedulerStartEventSelectFile) {
            scene_manager_next_scene(app->scene_manager, SchedulerSceneLoadFile);
        }
        consumed = true;
    }
    return consumed;
}

void scheduler_scene_start_on_exit(void* context) {
    SchedulerApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
