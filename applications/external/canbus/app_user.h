#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

#include <dialogs/dialogs.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/file_browser_worker.h>
#include <gui/modules/dialog_ex.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>
#include <gui/modules/loading.h>

#include "scenes_config/app_scene_functions.h"

#include "libraries/mcp_can_2515.h"
#include "libraries/pid_library.h"
#include "libraries/uds_library.h"
#include "libraries/frame_queue.h"

#include "canbus_icons.h"

#include <files_scaner.h>
#include <SLCAN_serial.h>
#include <log_exporter.h>
#include <hex_converter.h>

#define PROGRAM_VERSION "v1.1.6.2"

#define PATHEXPORTS APP_DATA_PATH("exports")
#define PATHLOGS    APP_DATA_PATH("logs")

#define DEVICE_NO_CONNECTED (0xFF)

#define MESSAGE_ERROR 0xF0

#define UDS_REQUEST_ID_DEFAULT  0x7e1
#define UDS_RESPONSE_ID_DEFAULT 0x7e9

#define START_TIME 1500

typedef enum {
    WorkerflagStop = (1 << 0),
    WorkerflagReceived = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerflagStop | WorkerflagReceived)

typedef struct {
    MCP2515* mcp_can;
    CANFRAME can_frame;
    CANFRAME* frameArray;
    CANFRAME* frame_to_send;
    FrameCANQueue* frame_queue;

    OBDII obdii;

    uint32_t time;
    uint32_t times[100];
    uint32_t current_time[100];

    FuriThread* thread;
    FuriThread* thread_SLCAN;
    FuriMutex* mutex;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* varList;
    TextBox* textBox;
    ByteInput* input_byte_value;
    FileBrowser* file_browser;

    FuriString* text;
    FuriString* data;
    FuriString* path;

    Storage* storage;
    DialogsApp* dialogs;
    File* log_file;
    char* log_file_path;
    bool log_file_ready;
    uint8_t save_logs;

    // New Logs Objecs
    DialogEx* dialog_ex;
    FrameCAN* frame_active;
    FileActive* file_active;
    bool* can_send_frame;
    bool* send_timestamp;
    Loading* loading;

    uint32_t sniffer_index;
    uint32_t sniffer_index_aux;

    uint32_t uds_received_id;
    uint32_t uds_send_id;

    uint8_t config_timing_index;

    uint8_t num_of_devices;
    uint8_t sender_selected_item;
    uint8_t sender_id_compose[4];

    uint32_t obdii_aux_index;
    uint8_t flags;

    uint64_t size_of_storage;

    uint8_t request_data;
} App;

// This is for the menu Options
typedef enum {
    SniffingTestOption,
    SLCANOption,
    SpeedDetectorOption,
    SenderOption,
    ObdiiOption,
    UDSOption,
    ReadLOGOption,
    PlayLOGOption,
    SettingsOption,
    AboutUsOption,
} MainMenuOptions;

// These are the events on the main menu
typedef enum {
    SniffingOptionEvent,
    SLCAN2CANOptionEvent,
    SpeedDetectorEvent,
    SenderOptionEvent,
    SettingsOptionEvent,
    ObdiiOptionEvent,
    UDSOptionEvent,
    ReadLOGOptionEvent,
    PlayLOGOptionEvent,
    AboutUsEvent,
} MainMenuEvents;

// This is for the Setting Options
typedef enum {
    BitrateOption,
    CristyalClkOption,
    SaveLogsOption
} OptionSettings;

// These are the events on the settings menu
typedef enum {
    BitrateOptionEvent,
    CristyalClkOptionEvent
} SettingsMenuEvent;

// These are the sender events
typedef enum {
    ChooseIdEvent,
    SetIdEvent,
    ReturnEvent
} SenderEvents;

// These are the player events
typedef enum {
    ChooseTimingEvent,
    ReturnTimingEvent
} PlayerEvents;

// These are the options to save
typedef enum {
    NoSave,
    SaveAll,
    OnlyAddress
} SaveOptions;

// This is for SniffingTest Options
typedef enum {
    RefreshTest,
    EntryEvent,
    ShowData
} SniffingTestEvents;

// These are the events in AboutOption
typedef enum {
    ButtonGetPressed
} ButtonPressedEvent;

// Views in the App
typedef enum {
    SubmenuView,
    ViewWidget,
    VarListView,
    TextBoxView,
    SubmenuLogView,
    DialogView,
    LoadingView,
    DialogInfoView,
    InputByteView,
    FileBrowserView,
} scenesViews;

typedef enum {
    SLCAN_SEND_LOG,
    SLCAN_SEND_FRAME,
} SLCAN_OPTIONS_SCENE;

typedef enum {
    VIEW_LOG,
    EXPORT_LOG,
    TRANSMIT_LOG,
} LOGS_OPTIONS;

typedef enum {
    THREAD_SLCAN_STOP = (1 << 0),
    THREAD_SNIFFER_STOP = (1 << 1),
} THREAD_CONTROL;

/**
 * These functions works in other scenes and widget
 */

void draw_in_development(App* app);
void draw_device_no_connected(App* app);
void draw_transmition_failure(App* app);
void draw_send_ok(App* app);
void draw_send_wrong(App* app);

// Thread to sniff
int32_t worker_sniffing(void* context);
