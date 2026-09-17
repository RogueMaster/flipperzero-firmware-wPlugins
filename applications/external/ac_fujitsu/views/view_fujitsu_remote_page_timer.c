#include "view_fujitsu_remote_page_timer.h"
#include "gui/modules/variable_item_list.h"

static const char* g_TimerModeLabels[] = {"ON", "OFF", "ON->OFF", "OFF->ON"};
static const FujiAirTimerMode g_TimerModeValues[] = {
    FujiAirTimerModes_ON,
    FujiAirTimerModes_OFF,
    FujiAirTimerModes_ON_OFF,
    FujiAirTimerModes_OFF_ON};

static const char* g_TimerValueLabels[] = {"30min",    "1h",  "1h 30min", "2h", "2h 30min", "3h",
                                           "3h 30min", "4h",  "4h 30min", "5h", "5h 30min", "6h",
                                           "6h 30min", "7h",  "7h 30min", "8h", "8h 30min", "9h",
                                           "9h 30min", "10h", "11h",      "12h"};

static uint32_t g_TimerValuesInMin[] = {30,  60,  90,  120, 150, 180, 210, 240, 270, 300, 330,
                                        360, 390, 420, 450, 480, 510, 540, 570, 600, 660, 720};

typedef enum {
    FujiTimerListItemName_TimerMode,
    FujiTimerListItemName_TimerOn,
    FujiTimerListItemName_TimerOff,
    FujiTimerListItemName_Confirm,
    FujiTimerListItemName_Count
} FujiTimerListItemName;

typedef struct {
    uint32_t tmp_selected_timer_on_value;
    uint32_t tmp_selected_timer_off_value;
    FujiAirTimerMode tmp_selected_mode;
} FujiTimerViewContext;

static FujiTimerViewContext g_TimerViewContext;

static void view_fujitsu_remote_page_timer_item_mode_change_callback(VariableItem* item) {
    uint8_t current = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, g_TimerModeLabels[current]);
    g_TimerViewContext.tmp_selected_mode = g_TimerModeValues[current];
}

static void view_fujitsu_remote_page_timer_item_on_change_callback(VariableItem* item) {
    uint8_t current = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, g_TimerValueLabels[current]);
    g_TimerViewContext.tmp_selected_timer_on_value = g_TimerValuesInMin[current];
}

static void view_fujitsu_remote_page_timer_item_off_change_callback(VariableItem* item) {
    uint8_t current = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, g_TimerValueLabels[current]);
    g_TimerViewContext.tmp_selected_timer_off_value = g_TimerValuesInMin[current];
}

static void view_fujitsu_remote_page_timer_item_confirm_change_callback(VariableItem* item) {
    UNUSED(item);
}

static void view_fujitsu_remote_page_timer_enter_callback(void* ctx, uint32_t index) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;

    if(index == FujiTimerListItemName_Confirm) {
        app->air_conditioner_state->on_off_timer_mode = g_TimerViewContext.tmp_selected_mode;

        if(g_TimerViewContext.tmp_selected_mode == FujiAirTimerModes_ON) {
            app->air_conditioner_state->on_timer = g_TimerViewContext.tmp_selected_timer_on_value;
            app->air_conditioner_state->off_timer = 0;
        } else if(g_TimerViewContext.tmp_selected_mode == FujiAirTimerModes_OFF) {
            app->air_conditioner_state->on_timer = 0;
            app->air_conditioner_state->off_timer =
                g_TimerViewContext.tmp_selected_timer_off_value;
        } else {
            app->air_conditioner_state->on_timer = g_TimerViewContext.tmp_selected_timer_on_value;
            app->air_conditioner_state->off_timer =
                g_TimerViewContext.tmp_selected_timer_off_value;
        }
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FujitsuRemoteAppEvent_TimerChanged);
        view_dispatcher_send_custom_event(app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage3);
    }
}

void view_fujitsu_remote_page_timer_alloc(FujitsuRemoteApp* app) {
    app->variable_item_list_timer = variable_item_list_alloc();

    variable_item_list_set_enter_callback(
        app->variable_item_list_timer, view_fujitsu_remote_page_timer_enter_callback, app);

    VariableItem* itm_mode = variable_item_list_add(
        app->variable_item_list_timer,
        "Mode:",
        (sizeof(g_TimerModeLabels) / sizeof(char*)),
        view_fujitsu_remote_page_timer_item_mode_change_callback,
        app);

    variable_item_set_current_value_index(itm_mode, FujiTimerListItemName_TimerMode);
    variable_item_set_current_value_text(itm_mode, g_TimerModeLabels[0]);
    g_TimerViewContext.tmp_selected_mode = g_TimerModeValues[0];

    variable_item_list_set_selected_item(
        app->variable_item_list_timer, FujiTimerListItemName_TimerMode);

    VariableItem* itm_on = variable_item_list_add(
        app->variable_item_list_timer,
        "ON Time:",
        sizeof(g_TimerValueLabels) / sizeof(char*),
        view_fujitsu_remote_page_timer_item_on_change_callback,
        app);

    variable_item_set_current_value_index(itm_on, 0);
    variable_item_set_current_value_text(itm_on, g_TimerValueLabels[0]);
    g_TimerViewContext.tmp_selected_timer_on_value = g_TimerValuesInMin[0];

    VariableItem* itm_off = variable_item_list_add(
        app->variable_item_list_timer,
        "OFF Time:",
        sizeof(g_TimerValueLabels) / sizeof(char*),
        view_fujitsu_remote_page_timer_item_off_change_callback,
        app);

    variable_item_set_current_value_index(itm_off, 0);
    variable_item_set_current_value_text(itm_off, g_TimerValueLabels[0]);
    g_TimerViewContext.tmp_selected_timer_off_value = g_TimerValuesInMin[0];

    variable_item_list_add(
        app->variable_item_list_timer,
        "Confirm",
        0,
        view_fujitsu_remote_page_timer_item_confirm_change_callback,
        app);

    app->view_page_timer = variable_item_list_get_view(app->variable_item_list_timer);

    view_set_previous_callback(app->view_page_timer, view_fujitsu_remote_page_timer_exit_callback);
}

void view_fujitsu_remote_page_timer_add_view(FujitsuRemoteApp* app) {
    view_dispatcher_add_view(app->view_dispatcher, FujitsuViewId_Page_Timer, app->view_page_timer);
}

void view_fujitsu_remote_page_timer_remove_view(FujitsuRemoteApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FujitsuViewId_Page_Timer);
}

void view_fujitsu_remote_page_timer_free(FujitsuRemoteApp* app) {
    app->view_page_timer = NULL;
    variable_item_list_free(app->variable_item_list_timer);
}

uint32_t view_fujitsu_remote_page_timer_exit_callback(void* ctx) {
    UNUSED(ctx);
    return FujitsuViewId_Page3;
}
