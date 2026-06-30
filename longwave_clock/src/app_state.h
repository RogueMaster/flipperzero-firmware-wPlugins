#ifndef LWC_STATE_HEADERS
#define LWC_STATE_HEADERS

#include "flipper.h"
#include "logic_dcf77.h"
#include "logic_msf.h"
#include "protocols.h"
#include "scenes.h"
#include "gpio.h"

typedef struct ProtoConfig {
    LWCRunMode run_mode;
    LWCDataMode data_mode;
    LWCDataPin data_pin;
} ProtoConfig;

typedef enum {
    EventReceiveSync,
    EventReceiveZero,
    EventReceiveOne,
    EventReceiveUnknown
} LWCEventType;

typedef struct AppState {
    LWCType lwc_type;
    ProtoConfig* proto_configs[__lwc_number_of_protocols];
    GPIOContext* gpio;
    FuriTimer* seconds_timer;
    MinuteData* simulation_data;
    Storage* storage;
    bool display_on;
} AppState;

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    NotificationApp* notifications;
    VariableItemList* sub_menu;
    TextBox* about;
    TextBox* info_text;
    View* dcf77_view;
    View* msf_view;
    AppState* state;
} App;

App* app_alloc();
AppState* app_state_alloc();
void app_quit(App* app);
void app_free(App* app);
void app_init_lwc(App* app, LWCType rtype);
LWCScene lwc_get_start_scene_for_protocol(AppState* app_state);
void store_proto_config(AppState* app_state);

ProtoConfig* lwc_get_protocol_config(AppState* app_state);

#endif
