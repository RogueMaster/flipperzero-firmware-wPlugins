#include <stdio.h>

#include "../timed_remote.h"
#include "timed_remote_scene.h"

enum {
    ITEM_MODE,
    ITEM_HOURS,
    ITEM_MINUTES,
    ITEM_SECONDS,
    ITEM_REPEAT,
    ITEM_START,
};

#define REPEAT_OFF       0U
#define REPEAT_UNLIMITED 255U

static void add_time_item(
    VariableItemList*,
    const char*,
    uint8_t,
    uint8_t,
    VariableItemChangeCallback,
    TimedRemoteApp*);
static void build_items(TimedRemoteApp*);
static uint8_t index_from_repeat(uint8_t);
static void set_repeat_text(VariableItem*, uint8_t);
static uint8_t repeat_from_index(uint8_t);
static void set_two_digits(VariableItem*, uint8_t);
static uint32_t start_item_index(const TimedRemoteApp*);

static void set_two_digits(VariableItem* item, uint8_t value) {
    char text[4];

    snprintf(text, sizeof(text), "%02d", value);
    variable_item_set_current_value_text(item, text);
}

static uint8_t repeat_from_index(uint8_t index) {
    if(index == 0) return REPEAT_OFF;
    if(index == 1) return REPEAT_UNLIMITED;
    return index - 1;
}

static uint8_t index_from_repeat(uint8_t repeat) {
    if(repeat == REPEAT_OFF) return 0;
    if(repeat == REPEAT_UNLIMITED) return 1;
    return repeat + 1;
}

static void set_repeat_text(VariableItem* item, uint8_t repeat) {
    char text[16];

    if(repeat == REPEAT_OFF) {
        variable_item_set_current_value_text(item, "Off");
        return;
    }
    if(repeat == REPEAT_UNLIMITED) {
        variable_item_set_current_value_text(item, "Unlimited");
        return;
    }

    snprintf(text, sizeof(text), "%u", repeat);
    variable_item_set_current_value_text(item, text);
}

static uint32_t start_item_index(const TimedRemoteApp* app) {
    if(app->mode == MODE_COUNTDOWN) return ITEM_START;
    return ITEM_REPEAT;
}

static void on_mode_change(VariableItem* item) {
    TimedRemoteApp* app;
    uint8_t mode_index;

    app = variable_item_get_context(item);
    mode_index = variable_item_get_current_value_index(item);
    app->mode = mode_index == 0 ? MODE_COUNTDOWN : MODE_SCHEDULED;
    variable_item_set_current_value_text(
        item, app->mode == MODE_COUNTDOWN ? "Countdown" : "Scheduled");
    if(app->mode == MODE_SCHEDULED) app->repeat = REPEAT_OFF;

    view_dispatcher_send_custom_event(app->vd, EVENT_MODE_CHANGED);
}

static void on_hours_change(VariableItem* item) {
    TimedRemoteApp* app;

    app = variable_item_get_context(item);
    app->h = variable_item_get_current_value_index(item);
    set_two_digits(item, app->h);
}

static void on_minutes_change(VariableItem* item) {
    TimedRemoteApp* app;

    app = variable_item_get_context(item);
    app->m = variable_item_get_current_value_index(item);
    set_two_digits(item, app->m);
}

static void on_seconds_change(VariableItem* item) {
    TimedRemoteApp* app;

    app = variable_item_get_context(item);
    app->s = variable_item_get_current_value_index(item);
    set_two_digits(item, app->s);
}

static void on_repeat_change(VariableItem* item) {
    TimedRemoteApp* app;

    app = variable_item_get_context(item);
    app->repeat = repeat_from_index(variable_item_get_current_value_index(item));
    set_repeat_text(item, app->repeat);
}

static void on_enter(void* context, uint32_t index) {
    TimedRemoteApp* app;

    app = context;
    if(index != start_item_index(app)) return;

    view_dispatcher_send_custom_event(app->vd, EVENT_TIMER_CONFIGURED);
}

static void add_time_item(
    VariableItemList* list,
    const char* name,
    uint8_t value_count,
    uint8_t value,
    VariableItemChangeCallback callback,
    TimedRemoteApp* app) {
    VariableItem* item;

    item = variable_item_list_add(list, name, value_count, callback, app);
    variable_item_set_current_value_index(item, value);
    set_two_digits(item, value);
}

static void build_items(TimedRemoteApp* app) {
    VariableItem* item;

    variable_item_list_reset(app->vlist);

    item = variable_item_list_add(app->vlist, "Mode", 2, on_mode_change, app);
    variable_item_set_current_value_index(item, app->mode == MODE_COUNTDOWN ? 0 : 1);
    variable_item_set_current_value_text(
        item, app->mode == MODE_COUNTDOWN ? "Countdown" : "Scheduled");

    add_time_item(app->vlist, "Hours", 24, app->h, on_hours_change, app);
    add_time_item(app->vlist, "Minutes", 60, app->m, on_minutes_change, app);
    add_time_item(app->vlist, "Seconds", 60, app->s, on_seconds_change, app);

    if(app->mode == MODE_COUNTDOWN) {
        item = variable_item_list_add(app->vlist, "Repeat", 101, on_repeat_change, app);
        variable_item_set_current_value_index(item, index_from_repeat(app->repeat));
        set_repeat_text(item, app->repeat);
    }

    variable_item_list_add(app->vlist, ">> Start Timer <<", 0, NULL, NULL);
    variable_item_list_set_enter_callback(app->vlist, on_enter, app);
}

void scene_cfg_enter(void* context) {
    TimedRemoteApp* app;

    app = context;
    build_items(app);
    view_dispatcher_switch_to_view(app->vd, VIEW_LIST);
}

bool scene_cfg_event(void* context, SceneManagerEvent event) {
    TimedRemoteApp* app;

    app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == EVENT_MODE_CHANGED) {
        build_items(app);
        return true;
    }
    if(event.event != EVENT_TIMER_CONFIGURED) return false;

    if(app->repeat == REPEAT_OFF)
        app->repeat_left = 1;
    else if(app->repeat == REPEAT_UNLIMITED)
        app->repeat_left = REPEAT_UNLIMITED;
    else
        app->repeat_left = app->repeat + 1;

    scene_manager_next_scene(app->sm, SCENE_RUN);
    return true;
}

void scene_cfg_exit(void* context) {
    TimedRemoteApp* app;

    app = context;
    variable_item_list_reset(app->vlist);
}
