#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#define HID_EXFIL_VERSION                 "1.0"
#define HID_EXFIL_DATA_DIR                EXT_PATH("hid_exfil")
/* 4 KB is ample for the LED channel (~25 B/s max = 160 s to fill).
 * The previous 64 KB value consumed half the Flipper's available heap
 * on startup, guaranteeing an OOM reboot before the user did anything. */
#define HID_EXFIL_RECV_BUF_SIZE           (4 * 1024)
#define HID_EXFIL_LED_POLL_INTERVAL_MS    1
#define HID_EXFIL_CLOCK_TIMEOUT_MS        1000
#define HID_EXFIL_SYNC_TOGGLES            3
#define HID_EXFIL_SYNC_INTERVAL_MS        100
#define HID_EXFIL_EOT_TOGGLES             3
#define HID_EXFIL_DEFAULT_INJECT_SPEED_MS 10

/* Keyboard LED bitmasks (from HID spec) */
#define HID_KB_LED_NUM    (1 << 0)
#define HID_KB_LED_CAPS   (1 << 1)
#define HID_KB_LED_SCROLL (1 << 2)

/* ---- Enumerations ---- */

typedef enum {
    PayloadTypeWiFiPasswords = 0,
    PayloadTypeEnvVars,
    PayloadTypeClipboard,
    PayloadTypeSystemInfo,
    PayloadTypeSSHKeys,
    PayloadTypeBrowserBookmarks,
    PayloadTypeCustomScript,
    PayloadTypeCOUNT,
} PayloadType;

typedef enum {
    PhaseIdle = 0,
    PhaseInjecting,
    PhaseSyncing,
    PhaseReceiving,
    PhaseCleanup,
    PhaseDone,
    PhaseError,
} ExfilPhase;

typedef enum {
    TargetOSWindows = 0,
    TargetOSLinux,
    TargetOSMac,
    TargetOSCOUNT,
} TargetOS;

/* ---- View IDs ---- */

typedef enum {
    HidExfilViewWarning = 0,
    HidExfilViewPayloadSelect,
    HidExfilViewConfig,
    HidExfilViewExecution,
    HidExfilViewDataViewer,
} HidExfilView;

/* ---- Configuration ---- */

typedef struct {
    TargetOS target_os;
    uint32_t injection_speed_ms;
    bool cleanup_enabled;
} ExfilConfig;

/* ---- Runtime state ---- */

typedef struct {
    volatile ExfilPhase phase;
    volatile uint32_t bytes_received;
    volatile uint32_t total_estimated;
    volatile uint32_t start_tick;
    volatile uint8_t led_num;
    volatile uint8_t led_caps;
    volatile uint8_t led_scroll;
    volatile bool paused;
    volatile bool abort_requested;
    volatile bool led_channel_broken; /* firmware lacks Set_Report handler */
} ExfilState;

/* ---- Worker callback ---- */

typedef void (*HidExfilWorkerCallback)(ExfilPhase phase, uint32_t bytes_received, void* context);

/* ---- Worker ---- */

typedef struct {
    FuriThread* thread;
    ExfilConfig config;
    PayloadType payload_type;
    ExfilState state;
    uint8_t* recv_buffer;
    uint32_t recv_buffer_size;
    HidExfilWorkerCallback callback;
    void* callback_context;
    volatile bool running;
} HidExfilWorker;

/* ---- App ---- */

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    /* Views */
    Widget* warning;
    Submenu* payload_select;
    VariableItemList* config_list;
    View* execution_view;
    TextBox* data_viewer;

    /* Worker */
    HidExfilWorker* worker;

    /* Configuration */
    ExfilConfig config;
    PayloadType selected_payload;

    /* Received data */
    FuriString* data_text;

    /* USB state saved for restore */
    FuriHalUsbInterface* usb_prev;
} HidExfilApp;

/* ---- Label arrays (defined in hid_exfil.c) ---- */

extern const char* const payload_labels[];
extern const char* const target_os_labels[];
extern const char* const phase_labels[];

#define PHASE_LABELS_COUNT 7
