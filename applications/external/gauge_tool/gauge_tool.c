#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>

#include "bq27220.h"

typedef struct {
    Bq27220BatteryStatus battery_status;
    bool battery_status_good;

    Bq27220OperationStatus operation_status;
    bool operation_status_good;

    FuriString* buffer;
} GaugeToolData;

static GaugeToolData gauge_tool_data = {};

#define REPORT_ADD(data) furi_string_cat(gauge_tool_data.buffer, data ", ");

static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    furi_string_reset(gauge_tool_data.buffer);

    if(gauge_tool_data.operation_status_good) {
        furi_string_cat(gauge_tool_data.buffer, "Status: ");
        if(gauge_tool_data.operation_status.CALMD) REPORT_ADD("calibration mode");
        if(gauge_tool_data.operation_status.SEC == 0b11) {
            REPORT_ADD("SEALED");
        } else if(gauge_tool_data.operation_status.SEC == 0b10) {
            REPORT_ADD("UNSEALED");
        } else if(gauge_tool_data.operation_status.SEC == 0b01) {
            REPORT_ADD("FULL");
        } else {
            REPORT_ADD("SEC invalid");
        }
        if(gauge_tool_data.operation_status.EDV2) REPORT_ADD("EDV2 threshold exceeded");
        if(gauge_tool_data.operation_status.VDQ) REPORT_ADD("discharge cycle qualified");
        if(gauge_tool_data.operation_status.INITCOMP) REPORT_ADD("initialization complete");
        if(gauge_tool_data.operation_status.SMTH) REPORT_ADD("smooth");
        if(gauge_tool_data.operation_status.BTPINT) REPORT_ADD("battery trip point");
        if(gauge_tool_data.operation_status.CFGUPDATE) REPORT_ADD("config update mode");

        furi_string_left(gauge_tool_data.buffer, furi_string_size(gauge_tool_data.buffer) - 2);
        furi_string_cat(gauge_tool_data.buffer, ".\n");
    } else {
        furi_string_cat(gauge_tool_data.buffer, "Status read failure\n");
    }

    if(gauge_tool_data.battery_status_good) {
        furi_string_cat(gauge_tool_data.buffer, "Battery: ");
        if(gauge_tool_data.battery_status.BATTPRES) REPORT_ADD("battery is present");
        if(gauge_tool_data.battery_status.DSG) REPORT_ADD("discharging");
        if(gauge_tool_data.battery_status.SYSDWN) REPORT_ADD("system should shut down");
        if(gauge_tool_data.battery_status.AUTH_GD) REPORT_ADD("detect inserted battery");
        if(gauge_tool_data.battery_status.FD) REPORT_ADD("full discharge");
        if(gauge_tool_data.battery_status.OCVGD) REPORT_ADD("OCV good");
        if(gauge_tool_data.battery_status.OCVCOMP) REPORT_ADD("OCV complete");
        if(gauge_tool_data.battery_status.SLEEP) REPORT_ADD("sleep");
        if(gauge_tool_data.battery_status.FC) REPORT_ADD("full charge");
        if(gauge_tool_data.battery_status.CHGINH) REPORT_ADD("charge inhibit");

        furi_string_left(gauge_tool_data.buffer, furi_string_size(gauge_tool_data.buffer) - 2);
        furi_string_cat(gauge_tool_data.buffer, ".\n");

        furi_string_cat(gauge_tool_data.buffer, "Alarms: ");
        if(gauge_tool_data.battery_status.TDA) REPORT_ADD("terminate discharge");
        if(gauge_tool_data.battery_status.TCA) REPORT_ADD("terminate charge");
        if(gauge_tool_data.battery_status.OTD) REPORT_ADD("discharge overheat");
        if(gauge_tool_data.battery_status.OTC) REPORT_ADD("charge overheat");
        if(gauge_tool_data.battery_status.OCVFAIL) REPORT_ADD("OCV read failed");

        furi_string_left(gauge_tool_data.buffer, furi_string_size(gauge_tool_data.buffer) - 2);
        furi_string_cat(gauge_tool_data.buffer, ".");
    } else {
        furi_string_cat(gauge_tool_data.buffer, "Battery read failure");
    }

    elements_text_box(
        canvas,
        0,
        0,
        128,
        64,
        AlignLeft,
        AlignTop,
        furi_string_get_cstr(gauge_tool_data.buffer),
        false);

    elements_button_left(canvas, "Unseal");
    elements_button_center(canvas, "Full");
    elements_button_right(canvas, "Reset");
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static void gauge_tool_process_input(InputEvent* event) {
    if(event->key == InputKeyLeft) {
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
        bq27220_unseal(&furi_hal_i2c_handle_power);
        furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    } else if(event->key == InputKeyOk) {
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
        bq27220_full_access(&furi_hal_i2c_handle_power);
        furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    } else if(event->key == InputKeyRight) {
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
        bq27220_reset(&furi_hal_i2c_handle_power);
        furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    }
}

static void gauge_tool_refresh() {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    gauge_tool_data.battery_status_good =
        bq27220_get_battery_status(&furi_hal_i2c_handle_power, &gauge_tool_data.battery_status);
    gauge_tool_data.operation_status_good = bq27220_get_operation_status(
        &furi_hal_i2c_handle_power, &gauge_tool_data.operation_status);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
}

int32_t gauge_tool_app(void* p) {
    UNUSED(p);
    gauge_tool_data.buffer = furi_string_alloc();
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, NULL);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    gauge_tool_refresh();

    InputEvent event;
    while(true) {
        if(furi_message_queue_get(event_queue, &event, 1000) == FuriStatusOk) {
            if(event.key == InputKeyBack) {
                break;
            } else {
                gauge_tool_process_input(&event);
            }
        }
        gauge_tool_refresh();
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);

    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_string_free(gauge_tool_data.buffer);

    return 0;
}
