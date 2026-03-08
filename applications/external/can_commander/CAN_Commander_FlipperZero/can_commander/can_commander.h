#pragma once

#include <furi.h>
#include <gui/gui.h>

#include <gui/modules/submenu.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include "libraries/can_commander_uart.h"
#include "scenes_config/scene_functions.h"

#define PROGRAM_VERSION "v2.0.0"

typedef enum {
    AppViewSubmenu = 0,
    AppViewTextBox,
    AppViewTextInput,
    AppViewByteInput,
    AppViewVarList,
    AppViewWidget,
    AppViewDashboard,
} AppView;

typedef enum {
    AppCustomEventMonitorUpdated = 0x1000,
    AppCustomEventInputDone = 0x1001,
} AppCustomEvent;

typedef enum {
    AppDashboardNone = 0,
    AppDashboardReadAll,
    AppDashboardFiltered,
    AppDashboardWrite,
    AppDashboardSpeed,
    AppDashboardValtrack,
    AppDashboardUniqueIds,
    AppDashboardBittrack,
    AppDashboardReverse,
    AppDashboardObdPid,
    AppDashboardDbcDecode,
    AppDashboardCustomInject,
} AppDashboardMode;

typedef enum {
    AppHexInputNone = 0,
    AppHexInputU8,
    AppHexInputU16,
    AppHexInputU32,
    AppHexInputBytes,
} AppHexInputMode;

#define APP_ARGS_EDITOR_MAX_ITEMS  16U
#define APP_ARGS_EDITOR_KEY_MAX    24U
#define APP_ARGS_EDITOR_VALUE_MAX  80U

typedef enum {
    AppArgValueText = 0,
    AppArgValueAction,
    AppArgValueBus,
    AppArgValueBool01,
    AppArgValueOrder,
    AppArgValueSign,
    AppArgValueModeListen,
    AppArgValueModeReverse,
} AppArgValueType;

typedef struct App App;
typedef void (*AppArgsApplyCallback)(App* app);

typedef struct {
    char key[APP_ARGS_EDITOR_KEY_MAX];
    char value[APP_ARGS_EDITOR_VALUE_MAX];
    AppArgValueType type;
} AppArgItem;

struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    TextBox* text_box;
    TextInput* text_input;
    ByteInput* byte_input;
    VariableItemList* var_list;
    Widget* widget;
    View* dashboard_view;

    FuriMutex* mutex;
    FuriThread* rx_worker;
    bool rx_worker_stop;

    CcClient* client;
    bool connected;
    bool tool_active;
    bool monitor_scene_active;
    AppDashboardMode dashboard_mode;

    FuriString* monitor_text;
    FuriString* status_text;

    char input_work[220];
    char* input_dest;
    size_t input_dest_size;
    const char* input_header;

    char args_read_all[96];
    char args_filtered[128];
    char args_write_tool[128];
    char args_speed[64];
    char args_valtrack[64];
    char args_unique_ids[64];
    char args_bittrack[96];
    char args_reverse_auto[128];
    char args_reverse_read[128];
    char args_obd_pid[96];
    char args_dbc_decode[64];
    char args_custom_inject_start[64];
    char args_custom_inject_slots[5][220];
    char args_custom_inject_bit[64];
    char args_custom_inject_clearbit[48];
    char args_custom_inject_field[80];
    char args_tool_config[96];

    char args_bus_cfg_can0[96];
    char args_bus_cfg_can1[96];

    char args_filter_can0[96];
    char args_filter_can1[96];

    char args_dbc_add[220];
    char args_dbc_remove[48];

    char* args_editor_target;
    size_t args_editor_target_size;
    const char* args_editor_title;
    const char* args_editor_apply_label;
    AppArgsApplyCallback args_editor_apply_callback;
    CanCommanderScene args_editor_apply_next_scene;
    bool args_editor_apply_enabled;
    uint8_t args_editor_selected_index;
    uint8_t args_editor_count;
    AppArgItem args_editor_items[APP_ARGS_EDITOR_MAX_ITEMS];
    VariableItem* args_editor_var_items[APP_ARGS_EDITOR_MAX_ITEMS];

    bool input_editing_arg_value;
    uint8_t input_arg_value_index;
    bool input_use_byte_input;
    AppHexInputMode input_hex_mode;
    uint8_t input_hex_store[16];
    uint8_t input_hex_count;

    uint8_t custom_inject_active_slot;
    bool custom_inject_slot_provisioned[5];
    char custom_inject_edit_bus[32];
    char custom_inject_edit_name[40];
    char custom_inject_edit_id[40];
    char custom_inject_edit_count[40];
    char custom_inject_edit_interval[48];
    char custom_inject_edit_bit[48];
    char custom_inject_edit_field[64];
    char custom_inject_edit_bytes[32];
    char custom_inject_edit_value_hex[20];
    char custom_inject_edit_mux[96];
    char custom_inject_edit_mux_start[24];
    char custom_inject_edit_mux_len[24];
    char custom_inject_edit_mux_value[24];
    char custom_inject_set_name[32];

    CcToolId pending_tool_start_id;
    char pending_tool_start_name[24];
};

void app_set_status(App* app, const char* fmt, ...);
void app_append_monitor(App* app, const char* fmt, ...);
void app_refresh_monitor_view(App* app);
void app_refresh_live_view(App* app);
bool app_monitor_uses_dashboard(App* app);
bool app_args_set_key_value(char* args, size_t args_size, const char* key, const char* value);

bool app_connect(App* app, bool force_reconnect);
bool app_require_connected(App* app);

void app_begin_edit(App* app, char* destination, size_t destination_size, const char* header_text);
void app_apply_edit(App* app);
void app_begin_args_editor(
    App* app,
    char* destination,
    size_t destination_size,
    const char* header_text);
void app_begin_args_editor_apply(
    App* app,
    char* destination,
    size_t destination_size,
    const char* header_text,
    const char* apply_label,
    AppArgsApplyCallback apply_callback,
    CanCommanderScene next_scene);

void app_action_ping(App* app);
void app_action_get_info(App* app);
void app_action_stats(App* app);

void app_action_start_read_all(App* app);
void app_action_tool_start(App* app, CcToolId tool_id, const char* args, const char* label);
void app_action_tool_config(App* app, const char* args);
void app_action_tool_stop(App* app);
void app_action_tool_status(App* app);

void app_action_custom_inject_add(App* app, uint8_t slot_number);
void app_action_custom_inject_modify(App* app, uint8_t slot_number);
void app_action_custom_inject_remove(App* app, uint8_t slot_number);
void app_action_custom_inject_clear(App* app);
void app_action_custom_inject_list(App* app);
void app_action_custom_inject_cancel(App* app);
void app_action_custom_inject_bit(App* app);
void app_action_custom_inject_clearbit(App* app);
void app_action_custom_inject_field(App* app);
void app_action_custom_inject_inject(App* app, uint8_t slot_number);
void app_action_custom_inject_sync_slots(App* app);

void app_custom_inject_set_active_slot(App* app, uint8_t slot_index);
uint8_t app_custom_inject_get_active_slot(const App* app);
char* app_custom_inject_get_slot_args(App* app, uint8_t slot_index);
void app_custom_inject_reset_slot(App* app, uint8_t slot_index);
void app_custom_inject_reset_all_slots(App* app);
void app_custom_inject_load(App* app);
void app_custom_inject_save(App* app);
bool app_custom_inject_save_slot_set(App* app, const char* set_name);
bool app_custom_inject_load_slot_set_file(App* app, const char* file_path);

void app_action_bus_set_cfg(App* app, CcBus bus, const char* args);
void app_action_bus_get_cfg(App* app, CcBus bus);
void app_action_bus_set_filter(App* app, CcBus bus, const char* args);
void app_action_bus_clear_filter(App* app, CcBus bus);

void app_action_dbc_clear(App* app);
void app_action_dbc_add(App* app, const char* args);
void app_action_dbc_remove(App* app, const char* args);
void app_action_dbc_list(App* app);

int32_t cancommander_main(void* p);
