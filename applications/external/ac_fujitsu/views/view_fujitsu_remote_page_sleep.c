#include "view_fujitsu_remote_page_sleep.h"
#include "gui/modules/variable_item_list.h"

// static const uint8_t g_SleepTimeValues[] = {2, 4, 8};
static const char* g_SleepTimeValueLabels[] = {"30min", "1h", "2h", "3h", "5h", "7h", "9h"};

static const uint32_t g_SleeptimeValuesInMinutes[] = {30, 60, 120, 180, 300, 420, 540};

typedef enum {
    FujiSleepListItemName_Timer,
    FujiSleepListItemName_Confirm,
    FujiSleepListItemName_Count
} FujiSleepListItemName;

typedef struct {
    uint32_t tmp_selected_timer_value;
} FujiSleepViewContext;

static FujiSleepViewContext g_SleepViewContext;

void view_fujitsu_remote_page_sleep_item_time_change_callback(VariableItem* item) {
    uint8_t current = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, g_SleepTimeValueLabels[current]);
    g_SleepViewContext.tmp_selected_timer_value = g_SleeptimeValuesInMinutes[current];
}

void view_fujitsu_remote_page_sleep_item_confirm_change_callback(VariableItem* item) {
    UNUSED(item);
}

void view_fujitsu_remote_page_sleep_enter_callback(void* ctx, uint32_t index) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;

    if(index == FujiSleepListItemName_Confirm) {
        app->air_conditioner_state->sleep_timer = g_SleepViewContext.tmp_selected_timer_value;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FujitsuRemoteAppEvent_SleepChanged);
        view_dispatcher_send_custom_event(app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage3);
    }
}

void view_fujitsu_remote_page_sleep_alloc(FujitsuRemoteApp* app) {
    app->variable_item_list_sleep = variable_item_list_alloc();

    variable_item_list_set_enter_callback(
        app->variable_item_list_sleep, view_fujitsu_remote_page_sleep_enter_callback, app);

    VariableItem* itm1 = variable_item_list_add(
        app->variable_item_list_sleep,
        "Sleep after:",
        (sizeof(g_SleepTimeValueLabels) / sizeof(char*)),
        view_fujitsu_remote_page_sleep_item_time_change_callback,
        app);

    variable_item_set_current_value_index(itm1, FujiSleepListItemName_Timer);
    variable_item_set_current_value_text(itm1, g_SleepTimeValueLabels[0]);

    variable_item_list_set_selected_item(
        app->variable_item_list_sleep, FujiSleepListItemName_Timer);

    variable_item_list_add(
        app->variable_item_list_sleep,
        "Confirm",
        0,
        view_fujitsu_remote_page_sleep_item_confirm_change_callback,
        app);

    app->view_page_sleep = variable_item_list_get_view(app->variable_item_list_sleep);

    view_set_previous_callback(app->view_page_sleep, view_fujitsu_remote_page_sleep_exit_callback);
}

void view_fujitsu_remote_page_sleep_add_view(FujitsuRemoteApp* app) {
    view_dispatcher_add_view(app->view_dispatcher, FujitsuViewId_Page_Sleep, app->view_page_sleep);
}

void view_fujitsu_remote_page_sleep_remove_view(FujitsuRemoteApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FujitsuViewId_Page_Sleep);
}

void view_fujitsu_remote_page_sleep_free(FujitsuRemoteApp* app) {
    app->view_page_sleep = NULL;
    variable_item_list_free(app->variable_item_list_sleep);
}

uint32_t view_fujitsu_remote_page_sleep_exit_callback(void* ctx) {
    UNUSED(ctx);
    return FujitsuViewId_Page3;
}
