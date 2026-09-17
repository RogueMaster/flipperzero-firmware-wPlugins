#include "view_antenna_info.h"

typedef struct {
    bool startup;
    bool connected;
    bool alert;
    bool connecting;
    uint8_t connect_attempt;
    uint8_t connect_total;
    uint32_t return_view;
    char hardware[24];
    char software[24];
    char manufacturer[20];
} UHFAntennaInfoModel;

static void uhf_reader_antenna_info_draw(Canvas* canvas, void* model) {
    UHFAntennaInfoModel* Model = model;

    canvas_clear(canvas);

    if(Model->connecting) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignTop, "Antenna connecting");

        char attempts[20];
        snprintf(
            attempts,
            sizeof(attempts),
            "attempts [%u/%u]",
            (unsigned int)Model->connect_attempt,
            (unsigned int)Model->connect_total);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, attempts);
        return;
    }

    if(Model->alert) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 3, AlignCenter, AlignTop, "Antenna");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 21, AlignCenter, AlignTop, "Antenna Not Connected!");
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "Connect in Configure");
        elements_button_center(canvas, "OK");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        64,
        2,
        AlignCenter,
        AlignTop,
        Model->connected ? "Antenna connected" : "About Antenna");

    canvas_set_font(canvas, FontSecondary);
    if(Model->connected) {
        char line[30];
        snprintf(line, sizeof(line), "HW: %.22s", Model->hardware);
        canvas_draw_str(canvas, 2, 18, line);
        snprintf(line, sizeof(line), "SW: %.22s", Model->software);
        canvas_draw_str(canvas, 2, 31, line);
        snprintf(line, sizeof(line), "MFG: %.18s", Model->manufacturer);
        canvas_draw_str(canvas, 2, 44, line);
    } else {
        canvas_draw_str_aligned(canvas, 64, 21, AlignCenter, AlignTop, "Antenna Not Connected!");
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "Use Connection first");
    }

    if(Model->startup) {
        elements_button_center(canvas, "OK");
    } else {
        elements_button_left(canvas, "Back");
    }
}

static bool uhf_reader_antenna_info_input(InputEvent* event, void* context) {
    UHFReaderApp* App = context;
    if(event->type != InputTypeShort) return false;

    bool startup = false;
    bool alert = false;
    bool connecting = false;
    uint32_t return_view = UHFReaderViewSubmenu;
    with_view_model(
        App->ViewAntennaInfo,
        UHFAntennaInfoModel * Model,
        {
            startup = Model->startup;
            alert = Model->alert;
            connecting = Model->connecting;
            return_view = Model->return_view;
        },
        false);

    if(connecting) return true;

    if(alert) {
        if(event->key == InputKeyOk || event->key == InputKeyBack || event->key == InputKeyLeft) {
            view_dispatcher_switch_to_view(App->ViewDispatcher, return_view);
        }
        return true;
    }

    if(startup) {
        /* The startup information remains visible until the center key is pressed. */
        if(event->key == InputKeyOk) {
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSubmenu);
        }
        return true;
    }

    if(event->key == InputKeyBack || event->key == InputKeyLeft) {
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewConfigure);
        return true;
    }

    return false;
}

void uhf_reader_show_antenna_connecting(UHFReaderApp* App, uint8_t attempt, uint8_t total) {
    furi_assert(App);
    view_antenna_info_alloc(App);

    if(total == 0U) total = 1U;
    if(attempt == 0U) attempt = 1U;
    if(attempt > total) attempt = total;

    UHF_I("APP", "ANTENNA CONNECTING attempt=%u/%u", (unsigned int)attempt, (unsigned int)total);
    uhf_debug_flush();

    with_view_model(
        App->ViewAntennaInfo,
        UHFAntennaInfoModel * Model,
        {
            Model->startup = true;
            Model->connected = false;
            Model->alert = false;
            Model->connecting = true;
            Model->connect_attempt = attempt;
            Model->connect_total = total;
            Model->return_view = UHFReaderViewSubmenu;
            Model->hardware[0] = '\0';
            Model->software[0] = '\0';
            Model->manufacturer[0] = '\0';
        },
        true);

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewAntennaInfo);
}

void view_antenna_info_alloc(UHFReaderApp* App) {
    furi_assert(App);
    if(App->ViewAntennaInfo) return;

    App->ViewAntennaInfo = view_alloc();
    view_allocate_model(App->ViewAntennaInfo, ViewModelTypeLocking, sizeof(UHFAntennaInfoModel));
    view_set_context(App->ViewAntennaInfo, App);
    view_set_draw_callback(App->ViewAntennaInfo, uhf_reader_antenna_info_draw);
    view_set_input_callback(App->ViewAntennaInfo, uhf_reader_antenna_info_input);
    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewAntennaInfo, App->ViewAntennaInfo);
}

void uhf_reader_show_antenna_info(UHFReaderApp* App, bool startup) {
    furi_assert(App);
    view_antenna_info_alloc(App);

    const bool connected = App->ReaderConnected && uhf_worker_is_ready(App->YRM100XWorker);
    const char* hardware = NULL;
    const char* software = NULL;
    const char* manufacturer = NULL;

    if(connected) {
        M100Module* module = App->YRM100XWorker->module;
        hardware = m100_get_hardware_version(module);
        software = m100_get_software_version(module);
        manufacturer = m100_get_manufacturers(module);

        UHF_I(
            "APP",
            "READER INFO HW='%s' SW='%s' MFG='%s' source=%s",
            hardware ? hardware : "n/a",
            software ? software : "n/a",
            manufacturer ? manufacturer : "n/a",
            startup ? "startup" : "configure");
    } else {
        UHF_W("APP", "READER INFO unavailable source=%s", startup ? "startup" : "configure");
    }
    uhf_debug_flush();

    with_view_model(
        App->ViewAntennaInfo,
        UHFAntennaInfoModel * Model,
        {
            Model->startup = startup;
            Model->connected = connected;
            Model->alert = false;
            Model->connecting = false;
            Model->connect_attempt = 0U;
            Model->connect_total = 0U;
            Model->return_view = startup ? UHFReaderViewSubmenu : UHFReaderViewConfigure;
            if(connected) {
                snprintf(
                    Model->hardware, sizeof(Model->hardware), "%.22s", hardware ? hardware : "n/a");
                snprintf(
                    Model->software, sizeof(Model->software), "%.22s", software ? software : "n/a");
                snprintf(
                    Model->manufacturer,
                    sizeof(Model->manufacturer),
                    "%.18s",
                    manufacturer ? manufacturer : "n/a");
            } else {
                Model->hardware[0] = '\0';
                Model->software[0] = '\0';
                Model->manufacturer[0] = '\0';
            }
        },
        true);

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewAntennaInfo);
}

void uhf_reader_show_antenna_not_connected(UHFReaderApp* App, uint32_t return_view) {
    furi_assert(App);
    view_antenna_info_alloc(App);
    uhf_notify_error(App);

    UHF_W("APP", "RFID action blocked: antenna not connected");
    uhf_debug_flush();

    with_view_model(
        App->ViewAntennaInfo,
        UHFAntennaInfoModel * Model,
        {
            Model->startup = false;
            Model->connected = false;
            Model->alert = true;
            Model->connecting = false;
            Model->connect_attempt = 0U;
            Model->connect_total = 0U;
            Model->return_view = return_view;
            Model->hardware[0] = '\0';
            Model->software[0] = '\0';
            Model->manufacturer[0] = '\0';
        },
        true);

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewAntennaInfo);
}

void view_antenna_info_free(UHFReaderApp* App) {
    furi_assert(App);
    if(!App->ViewAntennaInfo) return;

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewAntennaInfo);
    view_free(App->ViewAntennaInfo);
    App->ViewAntennaInfo = NULL;
}
