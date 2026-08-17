#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/loading.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bq_unlock.h"
#include "file_export.h"
#include "flipper_i2c.h"
#include "sbs_protocol.h"

#define TAG                      "BMSReader"
#define BQ30_CMD_RESET           0x0012
#define BQ30_CMD_LIFETIME_RESET  0x0028
#define BQ30_CMD_CLEAR_PF        0x0029
#define BQ30_CMD_BLACK_BOX_RESET 0x002A

typedef enum {
    AppViewScanner,
    AppViewHelp,
    AppViewDevices,
    AppViewBattery,
    AppViewBqActions,
    AppViewConfirm,
    AppViewResult,
    AppViewLoading,
    AppViewPopup,
    AppViewCount,
} AppView;

typedef enum {
    AppEventScan,
    AppEventShowHelp,
    AppEventHelpDone,
    AppEventReadBattery,
    AppEventDetectBq30,
    AppEventToggleKeyPreset,
    AppEventSave,
    AppEventShowActions,
    AppEventShowConfirm,
    AppEventExecuteAction,
    AppEventResultDone,
    AppEventPopupDone,
} AppEvent;

typedef enum {
    BatteryActionSave,
    BatteryActionDetectBq30,
    BatteryActionKeyPreset,
    BatteryActionUnseal,
    BatteryActionFullAccess,
    BatteryActionClearPf,
    BatteryActionBlackBoxReset,
    BatteryActionLifetimeReset,
    BatteryActionResetChip,
    BatteryActionSeal,
} BatteryAction;

typedef enum {
    BqActionUnseal,
    BqActionFullAccess,
    BqActionClearPf,
    BqActionBlackBoxReset,
    BqActionLifetimeReset,
    BqActionResetChip,
    BqActionSeal,
} BqAction;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Widget* scanner_widget;
    Widget* help_widget;
    Submenu* devices_submenu;
    Widget* battery_widget;
    Submenu* actions_submenu;
    Widget* confirm_widget;
    Widget* result_widget;
    Loading* loading;
    Popup* popup;
    NotificationApp* notification;
    AppView current_view;
    AppView popup_return_view;
    uint8_t addresses[I2C_SCAN_MAX_DEVICES];
    char device_labels[I2C_SCAN_MAX_DEVICES][32];
    size_t address_count;
    size_t selected_address;
    bool bmp280_found;
    uint8_t bmp280_address;
    BMSData battery;
    BQ40KeyPreset bq40_key_preset;
    BqAction selected_action;
    BQ40PfClearResult action_result;
    BQ40Status status_before;
    BQ40Status status_after;
    char battery_text[768];
    char confirm_text[192];
    char result_text[256];
    char popup_text[96];
} AppState;

static void app_switch_view(AppState* app, AppView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static bool app_is_bq30(const AppState* app) {
    return app->battery.chip_type == BQ_CHIP_BQ30Z55 || app->battery.chip_type == BQ_CHIP_BQ30Z554;
}

static void app_button_callback(GuiButtonType button_type, InputType input_type, void* context) {
    AppState* app = context;
    if(input_type != InputTypeShort) return;

    if(app->current_view == AppViewScanner && button_type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventScan);
    } else if(app->current_view == AppViewScanner && button_type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventShowHelp);
    } else if(app->current_view == AppViewHelp && button_type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventHelpDone);
    } else if(app->current_view == AppViewBattery && button_type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventShowActions);
    } else if(app->current_view == AppViewResult && button_type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventResultDone);
    }
}

static void
    app_confirm_button_callback(GuiButtonType button_type, InputType input_type, void* context) {
    AppState* app = context;
    if(button_type == GuiButtonTypeCenter && input_type == InputTypeLong) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventExecuteAction);
    }
}

static void app_device_callback(void* context, InputType input_type, uint32_t index) {
    AppState* app = context;
    if(input_type != InputTypeRelease) return;
    if(index >= app->address_count) return;
    app->selected_address = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventReadBattery);
}

static void app_action_callback(void* context, InputType input_type, uint32_t index) {
    AppState* app = context;
    if(input_type != InputTypeRelease) return;
    if(index == BatteryActionSave) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventSave);
        return;
    }
    if(index == BatteryActionDetectBq30) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventDetectBq30);
        return;
    }
    if(index == BatteryActionKeyPreset) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventToggleKeyPreset);
        return;
    }
    app->selected_action = index == BatteryActionClearPf       ? BqActionClearPf :
                           index == BatteryActionBlackBoxReset ? BqActionBlackBoxReset :
                           index == BatteryActionLifetimeReset ? BqActionLifetimeReset :
                           index == BatteryActionResetChip     ? BqActionResetChip :
                           index == BatteryActionSeal          ? BqActionSeal :
                           index == BatteryActionFullAccess    ? BqActionFullAccess :
                                                                 BqActionUnseal;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventShowConfirm);
}

static void app_popup_callback(void* context) {
    AppState* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventPopupDone);
}

static void app_build_scanner_widget(AppState* app, const char* message) {
    widget_reset(app->scanner_widget);
    widget_add_string_element(
        app->scanner_widget, 64, 8, AlignCenter, AlignCenter, FontSecondary, "BMS I2C Scanner");
    widget_add_string_multiline_element(
        app->scanner_widget,
        64,
        31,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        message ? message : "C0 - SCL    C1 - SDA\nGND - battery GND");
    widget_add_button_element(
        app->scanner_widget, GuiButtonTypeCenter, "Scan", app_button_callback, app);
    widget_add_button_element(
        app->scanner_widget, GuiButtonTypeRight, "Help", app_button_callback, app);
}

static void app_build_help_widget(AppState* app) {
    static const char help_text[] =
        "BMS Reader functions\n\n"
        "- Scan the external I2C bus.\n"
        "- Read standard SBS battery data.\n"
        "- Read SBS data without BMS state-changing commands.\n"
        "- For a recognized DJI BA-series battery, automatically read BQ40 firmware and security using read selectors in 0x44.\n"
        "- Save a dated report to the SD card.\n\n"
        "BQ40 protection\n\n"
        "BQ40 offers DJI and TI factory key presets. DJI is selected by default. Unseal attempts SEC 3 to SEC 2.\n\n"
        "Full Access is a separate SEC 2 to SEC 1 step using the same selected preset.\n\n"
        "Clear PF is available as a separate step after successful Unseal. It clears standard Permanent Failure flags when the physical fault has already been fixed.\n\n"
        "BQ30z55 protection\n\n"
        "Use Detect BQ30z55 explicitly. SHA Unseal and Full Access use the known DJI default key. Service commands require Full Access. Seal / Reset Access returns SEC to Sealed after service. Writes are blocked below 3600 mV per cell or above 200 mV imbalance.\n\n"
        "Clearing protection does not repair damaged, discharged or unbalanced cells. Check cell voltages before writing to the BMS.\n\n"
        "Connection\n"
        "C0 - SCL\n"
        "C1 - SDA\n"
        "GND - battery GND\n"
        "Use 3.3 V logic only.";

    widget_reset(app->help_widget);
    widget_add_text_scroll_element(app->help_widget, 3, 1, 122, 50, help_text);
    widget_add_button_element(
        app->help_widget, GuiButtonTypeLeft, "Back", app_button_callback, app);
}

static void app_build_devices_submenu(AppState* app) {
    submenu_reset(app->devices_submenu);

    for(size_t index = 0; index < app->address_count; index++) {
        uint8_t address = app->addresses[index];
        const char* type = "I2C device";
        if(address == SBS_I2C_ADDRESS) {
            type = "SBS/BMS";
        } else if(app->bmp280_found && address == app->bmp280_address) {
            type = "BMP280";
        }
        snprintf(
            app->device_labels[index],
            sizeof(app->device_labels[index]),
            "0x%02X  %s",
            address,
            type);
        submenu_add_item_ex(
            app->devices_submenu, app->device_labels[index], index, app_device_callback, app);
    }
}

static void app_build_battery_widget(AppState* app) {
    widget_reset(app->battery_widget);
    char cell_text[96] = "Cell voltages: not read\n";
    if(app->battery.cell_count > 0) {
        size_t length = 0;
        cell_text[0] = '\0';
        for(uint8_t index = 0; index < app->battery.cell_count; index++) {
            int added = snprintf(
                cell_text + length,
                sizeof(cell_text) - length,
                "Cell %u: %u mV\n",
                index + 1,
                app->battery.cell_voltage[index]);
            if(added < 0 || (size_t)added >= sizeof(cell_text) - length) break;
            length += (size_t)added;
        }
    }
    snprintf(
        app->battery_text,
        sizeof(app->battery_text),
        "Chip: %s\n"
        "I2C address: 0x%02X\n"
        "Manufacturer: %s\n"
        "Device: %s\n"
        "Chemistry: %s\n"
        "Serial: %u (0x%04X)\n"
        "DJI serial: %s\n"
        "Firmware: 0x%04X\n"
        "Firmware build: %u\n"
        "Voltage: %u mV\n"
        "%s"
        "Current: %d mA\n"
        "Temperature: %d.%d C\n"
        "State of charge: %u %%\n"
        "Remaining: %u mAh\n"
        "Full charge: %u mAh\n"
        "Design capacity: %u mAh\n"
        "Design voltage: %u mV\n"
        "Cycles: %u\n"
        "Battery status: 0x%04X\n"
        "Seal state: %s",
        bq_chip_name(app->battery.chip_type),
        app->battery.i2c_address,
        app->battery.manufacturer,
        app->battery.device_name,
        app->battery.device_chemistry,
        app->battery.serial_number,
        app->battery.serial_number,
        app->battery.dji_serial[0] ? app->battery.dji_serial : "not read",
        app->battery.firmware_version,
        app->battery.firmware_build,
        app->battery.voltage,
        cell_text,
        app->battery.current,
        (app->battery.temperature - 2731) / 10,
        abs((int)app->battery.temperature - 2731) % 10,
        app->battery.relative_state_of_charge,
        app->battery.remaining_capacity,
        app->battery.full_charge_capacity,
        app->battery.design_capacity,
        app->battery.design_voltage,
        app->battery.cycle_count,
        app->battery.battery_status,
        seal_state_name((SealState)app->battery.seal_state));
    widget_add_text_scroll_element(app->battery_widget, 3, 1, 122, 50, app->battery_text);

    widget_add_button_element(
        app->battery_widget, GuiButtonTypeCenter, "Menu", app_button_callback, app);

    submenu_reset(app->actions_submenu);
    submenu_add_item_ex(
        app->actions_submenu, "Save report", BatteryActionSave, app_action_callback, app);
    if(app->battery.chip_type == BQ_CHIP_BQ40Z307) {
        submenu_add_item_ex(
            app->actions_submenu,
            app->bq40_key_preset == BQ40_KEY_PRESET_DJI ? "Key preset: DJI" :
                                                          "Key preset: TI factory",
            BatteryActionKeyPreset,
            app_action_callback,
            app);
        submenu_add_item_ex(
            app->actions_submenu, "Unseal", BatteryActionUnseal, app_action_callback, app);
        submenu_add_item_ex(
            app->actions_submenu, "Full Access", BatteryActionFullAccess, app_action_callback, app);
        submenu_add_item_ex(
            app->actions_submenu, "Clear PF", BatteryActionClearPf, app_action_callback, app);
        submenu_add_item_ex(
            app->actions_submenu,
            "Seal / Reset Access",
            BatteryActionSeal,
            app_action_callback,
            app);
    } else if(app_is_bq30(app)) {
        submenu_add_item_ex(
            app->actions_submenu, "SHA Unseal", BatteryActionUnseal, app_action_callback, app);
        submenu_add_item_ex(
            app->actions_submenu,
            "SHA Full Access",
            BatteryActionFullAccess,
            app_action_callback,
            app);
        submenu_add_item_ex(
            app->actions_submenu, "Clear PF", BatteryActionClearPf, app_action_callback, app);
        submenu_add_item_ex(
            app->actions_submenu,
            "Black Box Reset",
            BatteryActionBlackBoxReset,
            app_action_callback,
            app);
        submenu_add_item_ex(
            app->actions_submenu,
            "Lifetime Reset",
            BatteryActionLifetimeReset,
            app_action_callback,
            app);
        submenu_add_item_ex(
            app->actions_submenu, "Reset Chip", BatteryActionResetChip, app_action_callback, app);
        submenu_add_item_ex(
            app->actions_submenu,
            "Seal / Reset Access",
            BatteryActionSeal,
            app_action_callback,
            app);
    } else {
        submenu_add_item_ex(
            app->actions_submenu,
            "Detect BQ30z55 (writes)",
            BatteryActionDetectBq30,
            app_action_callback,
            app);
    }
}

static void app_build_confirm_widget(AppState* app) {
    widget_reset(app->confirm_widget);
    const char* action_name = app->selected_action == BqActionUnseal        ? "Unseal" :
                              app->selected_action == BqActionFullAccess    ? "Full Access" :
                              app->selected_action == BqActionClearPf       ? "Clear PF" :
                              app->selected_action == BqActionBlackBoxReset ? "Black Box Reset" :
                              app->selected_action == BqActionLifetimeReset ? "Lifetime Reset" :
                              app->selected_action == BqActionSeal          ? "Seal Access" :
                                                                              "Reset Chip";
    bool bq30 = app_is_bq30(app);
    const char* action_detail =
        bq30 ?
            (app->selected_action == BqActionSeal ? "Return to SEC 3" :
             app->selected_action == BqActionUnseal || app->selected_action == BqActionFullAccess ?
                                                    "Cells >=3600 mV" :
                                                    "Requires SEC 1") :
            (app->selected_action == BqActionSeal ?
                 "Return to SEC 3" :
             app->selected_action == BqActionUnseal || app->selected_action == BqActionFullAccess ?
                 (app->bq40_key_preset == BQ40_KEY_PRESET_DJI ? "Preset DJI" :
                                                                "Preset TI factory") :
                 "Requires SEC 2");
    snprintf(
        app->confirm_text,
        sizeof(app->confirm_text),
        "%s\n"
        "Writes %s\n"
        "%s\n"
        "Hold OK",
        action_name,
        bq30 ? "BQ30" : "BQ40",
        action_detail);
    widget_add_text_box_element(
        app->confirm_widget, 4, 1, 120, 42, AlignCenter, AlignCenter, app->confirm_text, false);
    widget_add_button_element(
        app->confirm_widget, GuiButtonTypeCenter, "Hold", app_confirm_button_callback, app);
}

static const char* app_action_result_name(AppState* app) {
    bool unseal = app->selected_action == BqActionUnseal;
    bool full_access = app->selected_action == BqActionFullAccess;
    bool seal = app->selected_action == BqActionSeal;
    switch(app->action_result) {
    case BQ40_PF_CLEAR_OK:
        return unseal      ? "Unseal successful" :
               full_access ? "Full Access successful" :
               seal        ? "Access reset to Sealed" :
               app_is_bq30(app) && app->selected_action != BqActionClearPf ? "Command sent" :
                                                                             "PF cleared";
    case BQ40_PF_CLEAR_NOT_ACTIVE:
        return unseal      ? "Already unsealed" :
               full_access ? "Already Full Access" :
               seal        ? "Already sealed" :
                             "PF not active";
    case BQ40_PF_CLEAR_READ_ERROR:
        return "Status read failed";
    case BQ40_PF_CLEAR_UNSEAL_FAILED:
        return unseal      ? "Unseal rejected" :
               full_access ? "Unseal required/rejected" :
               seal        ? "Seal rejected" :
                             "Unseal required";
    case BQ40_PF_CLEAR_WRITE_ERROR:
        return "BMS write failed";
    case BQ40_PF_CLEAR_STILL_ACTIVE:
        return "PF still active";
    case BQ40_PF_CLEAR_RESULT_UNKNOWN:
        return "Result unknown; re-read";
    case BQ40_PF_CLEAR_UNSAFE_CELLS:
        return "Unsafe cell voltage/balance";
    case BQ40_PF_CLEAR_ACCESS_STATE_REQUIRED:
        return "Wrong SEC state";
    case BQ40_PF_CLEAR_KEY_REJECTED:
        return "Key rejected";
    default:
        return "Unknown result";
    }
}

static void app_build_result_widget(AppState* app) {
    widget_reset(app->result_widget);
    bool success = app->action_result == BQ40_PF_CLEAR_OK ||
                   app->action_result == BQ40_PF_CLEAR_NOT_ACTIVE;
    const char* result_heading = success ? "SUCCESSFUL" :
                                 app->action_result == BQ40_PF_CLEAR_RESULT_UNKNOWN ? "UNKNOWN" :
                                                                                      "WRONG";
    widget_add_string_element(
        app->result_widget, 64, 7, AlignCenter, AlignCenter, FontPrimary, result_heading);
    if(app->action_result == BQ40_PF_CLEAR_KEY_REJECTED) {
        if(app_is_bq30(app)) {
            snprintf(
                app->result_text,
                sizeof(app->result_text),
                "SHA key rejected\n"
                "SEC unchanged: %u\n"
                "No alternate preset",
                app->status_after.security_mode);
        } else {
            snprintf(
                app->result_text,
                sizeof(app->result_text),
                "Key preset rejected\n"
                "Preset: %s\n"
                "SEC unchanged: %u",
                app->bq40_key_preset == BQ40_KEY_PRESET_DJI ? "DJI" : "TI factory",
                app->status_after.security_mode);
        }
        widget_add_text_box_element(
            app->result_widget, 4, 14, 120, 32, AlignLeft, AlignTop, app->result_text, false);
        widget_add_button_element(
            app->result_widget, GuiButtonTypeCenter, "Done", app_button_callback, app);
        return;
    }
    bool show_bq30_operation = app_is_bq30(app) && app->selected_action != BqActionClearPf;
    snprintf(
        app->result_text,
        sizeof(app->result_text),
        "%s\n"
        "Security: %u -> %u\n"
        "Before: %08lX\n"
        "After:  %08lX",
        app_action_result_name(app),
        app->status_before.security_mode,
        app->status_after.security_mode,
        show_bq30_operation ? app->status_before.operation_status : app->status_before.pf_status,
        show_bq30_operation ? app->status_after.operation_status : app->status_after.pf_status);
    widget_add_text_box_element(
        app->result_widget, 4, 14, 120, 32, AlignLeft, AlignTop, app->result_text, false);
    widget_add_button_element(
        app->result_widget, GuiButtonTypeCenter, "Done", app_button_callback, app);
}

static void
    app_show_popup(AppState* app, const char* header, const char* text, AppView return_view) {
    popup_reset(app->popup);
    app->popup_return_view = return_view;
    snprintf(app->popup_text, sizeof(app->popup_text), "%s\n%s", header, text);
    popup_set_header(app->popup, NULL, 0, 0, AlignCenter, AlignCenter);
    popup_set_text(app->popup, app->popup_text, 64, 32, AlignCenter, AlignCenter);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, app_popup_callback);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);
    app_switch_view(app, AppViewPopup);
}

static void app_scan_bus(AppState* app) {
    app_switch_view(app, AppViewLoading);
    app->address_count = 0;
    app->selected_address = 0;
    app->bmp280_found = false;

    I2CResult result = bms_i2c_scan(app->addresses, sizeof(app->addresses), &app->address_count);
    if(result != I2C_OK) {
        app_build_scanner_widget(
            app, result == I2C_ERROR_BUSY ? "BUS STUCK\nCheck SDA and SCL" : "Scan failed");
        notification_message(app->notification, &sequence_blink_red_100);
        app_switch_view(app, AppViewScanner);
        return;
    }

    for(size_t index = 0; index < app->address_count; index++) {
        uint8_t address = app->addresses[index];
        if(address != BMP280_ADDRESS_LOW && address != BMP280_ADDRESS_HIGH) continue;
        uint8_t chip_id = 0;
        if(bms_i2c_read_register(address, BMP280_CHIP_ID_REGISTER, &chip_id) == I2C_OK &&
           chip_id == BMP280_CHIP_ID) {
            app->bmp280_found = true;
            app->bmp280_address = address;
            break;
        }
    }

    if(app->address_count == 0) {
        app_build_scanner_widget(app, "No I2C devices found\nCheck wiring and power");
        notification_message(app->notification, &sequence_blink_red_100);
        app_switch_view(app, AppViewScanner);
    } else {
        app_build_devices_submenu(app);
        notification_message(app->notification, &sequence_blink_green_100);
        app_switch_view(app, AppViewDevices);
    }
    FURI_LOG_I(TAG, "Scan complete, devices found: %u", (unsigned)app->address_count);
}

static void app_read_battery(AppState* app) {
    if(app->selected_address >= app->address_count) return;
    app_switch_view(app, AppViewLoading);
    uint8_t address = app->addresses[app->selected_address];
    I2CResult result = sbs_read_readonly(address, &app->battery);
    if(result == I2C_OK) {
        bool dji_profile = app->battery.i2c_address == SBS_I2C_ADDRESS &&
                           strncmp(app->battery.device_name, "BA", 2) == 0;
        if(dji_profile) {
            I2CResult extended_result = sbs_read_bq40_extended(&app->battery);
            if(extended_result != I2C_OK) {
                FURI_LOG_W(TAG, "Automatic DJI BQ40 extended read failed: %d", extended_result);
            }
        }
        app_build_battery_widget(app);
        notification_message(app->notification, &sequence_blink_green_100);
        app_switch_view(app, AppViewBattery);
    } else {
        notification_message(app->notification, &sequence_blink_red_100);
        app_show_popup(app, "Read failed", "Device is not SBS", AppViewDevices);
        FURI_LOG_W(TAG, "Read-only SBS probe failed at 0x%02X: %d", address, result);
    }
}

static void app_detect_bq30(AppState* app) {
    app_switch_view(app, AppViewLoading);
    I2CResult result = sbs_read_bq30_extended(&app->battery);
    if(result == I2C_OK && app_is_bq30(app)) {
        app_build_battery_widget(app);
        notification_message(app->notification, &sequence_blink_green_100);
        app_show_popup(
            app, bq_chip_name(app->battery.chip_type), "SHA actions enabled", AppViewBattery);
    } else {
        notification_message(app->notification, &sequence_blink_red_100);
        app_show_popup(app, "BQ30 not detected", "No supported response", AppViewBqActions);
        FURI_LOG_W(TAG, "Explicit BQ30 extended read failed: %d", result);
    }
}

static void app_execute_action(AppState* app) {
    app_switch_view(app, AppViewLoading);
    uint8_t sbs_version = (app->battery.spec_info >> 4) & 0x0F;
    bms_i2c_set_pec(&g_bms_i2c, sbs_version >= 3);

    if(app_is_bq30(app)) {
        if(app->selected_action == BqActionSeal) {
            app->action_result = bq30_seal(&g_bms_i2c, &app->status_before, &app->status_after);
        } else if(app->selected_action == BqActionUnseal) {
            app->action_result = bq30_unseal_sha1(
                &g_bms_i2c, &app->battery, &app->status_before, &app->status_after);
        } else if(app->selected_action == BqActionFullAccess) {
            app->action_result = bq30_full_access_sha1(
                &g_bms_i2c, &app->battery, &app->status_before, &app->status_after);
        } else {
            uint16_t command =
                app->selected_action == BqActionClearPf       ? BQ30_CMD_CLEAR_PF :
                app->selected_action == BqActionBlackBoxReset ? BQ30_CMD_BLACK_BOX_RESET :
                app->selected_action == BqActionLifetimeReset ? BQ30_CMD_LIFETIME_RESET :
                                                                BQ30_CMD_RESET;
            app->action_result = bq30_service_command(
                &g_bms_i2c, &app->battery, command, &app->status_before, &app->status_after);
        }
    } else {
        if(app->selected_action == BqActionSeal) {
            app->action_result = bq40_seal(&g_bms_i2c, &app->status_before, &app->status_after);
        } else if(app->selected_action == BqActionUnseal) {
            app->action_result = bq40_unseal_preset(
                &g_bms_i2c, app->bq40_key_preset, &app->status_before, &app->status_after);
        } else if(app->selected_action == BqActionFullAccess) {
            app->action_result = bq40_full_access(
                &g_bms_i2c, app->bq40_key_preset, &app->status_before, &app->status_after);
        } else {
            app->action_result =
                bq40_clear_pf(&g_bms_i2c, &app->status_before, &app->status_after);
        }
    }
    if(app->action_result == BQ40_PF_CLEAR_OK || app->action_result == BQ40_PF_CLEAR_NOT_ACTIVE) {
        app->battery.seal_state = app->status_after.security_mode == 3 ? SEAL_SEALED :
                                  app->status_after.security_mode == 2 ? SEAL_UNSEALED :
                                  app->status_after.security_mode == 1 ? SEAL_FULL :
                                                                         SEAL_UNKNOWN;
        app_build_battery_widget(app);
    }
    app_build_result_widget(app);
    bool success = app->action_result == BQ40_PF_CLEAR_OK ||
                   app->action_result == BQ40_PF_CLEAR_NOT_ACTIVE;
    if(app->action_result == BQ40_PF_CLEAR_RESULT_UNKNOWN) {
        notification_message(app->notification, &sequence_blink_blue_100);
    } else {
        notification_message(
            app->notification, success ? &sequence_blink_green_100 : &sequence_blink_red_100);
    }
    app_switch_view(app, AppViewResult);
}

static bool app_custom_event_callback(void* context, uint32_t event) {
    AppState* app = context;
    switch(event) {
    case AppEventScan:
        app_scan_bus(app);
        break;
    case AppEventShowHelp:
        app_switch_view(app, AppViewHelp);
        break;
    case AppEventHelpDone:
        app_switch_view(app, AppViewScanner);
        break;
    case AppEventReadBattery:
        app_read_battery(app);
        break;
    case AppEventDetectBq30:
        app_detect_bq30(app);
        break;
    case AppEventToggleKeyPreset:
        app->bq40_key_preset = app->bq40_key_preset == BQ40_KEY_PRESET_DJI ?
                                   BQ40_KEY_PRESET_TI_FACTORY :
                                   BQ40_KEY_PRESET_DJI;
        app_build_battery_widget(app);
        app_switch_view(app, AppViewBqActions);
        break;
    case AppEventSave: {
        bool saved = save_bms_data_to_sd(&app->battery);
        notification_message(
            app->notification, saved ? &sequence_blink_green_100 : &sequence_blink_red_100);
        app_show_popup(
            app,
            saved ? "Saved" : "Save failed",
            saved ? "Dated report on SD" : "Check SD card",
            AppViewBattery);
        break;
    }
    case AppEventShowActions:
        app_switch_view(app, AppViewBqActions);
        break;
    case AppEventShowConfirm:
        app_build_confirm_widget(app);
        app_switch_view(app, AppViewConfirm);
        break;
    case AppEventExecuteAction:
        app_execute_action(app);
        break;
    case AppEventResultDone:
        app_switch_view(app, AppViewBqActions);
        break;
    case AppEventPopupDone:
        app_switch_view(app, app->popup_return_view);
        break;
    default:
        return false;
    }
    return true;
}

static bool app_navigation_callback(void* context) {
    AppState* app = context;
    switch(app->current_view) {
    case AppViewDevices:
        app_switch_view(app, AppViewScanner);
        return true;
    case AppViewHelp:
        app_switch_view(app, AppViewScanner);
        return true;
    case AppViewBattery:
        app_switch_view(app, AppViewDevices);
        return true;
    case AppViewBqActions:
        app_switch_view(app, AppViewBattery);
        return true;
    case AppViewConfirm:
    case AppViewResult:
        app_switch_view(app, AppViewBqActions);
        return true;
    case AppViewPopup:
        app_switch_view(app, app->popup_return_view);
        return true;
    case AppViewLoading:
        return true;
    case AppViewScanner:
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

static AppState* app_alloc(void) {
    AppState* app = malloc(sizeof(AppState));
    if(!app) return NULL;
    memset(app, 0, sizeof(AppState));

    app->scanner_widget = widget_alloc();
    app->help_widget = widget_alloc();
    app->devices_submenu = submenu_alloc();
    app->battery_widget = widget_alloc();
    app->actions_submenu = submenu_alloc();
    app->confirm_widget = widget_alloc();
    app->result_widget = widget_alloc();
    app->loading = loading_alloc();
    app->popup = popup_alloc();
    app->view_dispatcher = view_dispatcher_alloc();
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    app_build_scanner_widget(app, NULL);
    app_build_help_widget(app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_navigation_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewScanner, widget_get_view(app->scanner_widget));
    view_dispatcher_add_view(app->view_dispatcher, AppViewHelp, widget_get_view(app->help_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewDevices, submenu_get_view(app->devices_submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewBattery, widget_get_view(app->battery_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewBqActions, submenu_get_view(app->actions_submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewConfirm, widget_get_view(app->confirm_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewResult, widget_get_view(app->result_widget));
    view_dispatcher_add_view(app->view_dispatcher, AppViewLoading, loading_get_view(app->loading));
    view_dispatcher_add_view(app->view_dispatcher, AppViewPopup, popup_get_view(app->popup));

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void app_free(AppState* app) {
    for(uint32_t view = 0; view < AppViewCount; view++) {
        view_dispatcher_remove_view(app->view_dispatcher, view);
    }
    view_dispatcher_free(app->view_dispatcher);
    widget_free(app->scanner_widget);
    widget_free(app->help_widget);
    submenu_free(app->devices_submenu);
    widget_free(app->battery_widget);
    submenu_free(app->actions_submenu);
    widget_free(app->confirm_widget);
    widget_free(app->result_widget);
    loading_free(app->loading);
    popup_free(app->popup);
    notification_message_block(app->notification, &sequence_reset_rgb);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t battery_reader_app(void* argument) {
    UNUSED(argument);
    AppState* app = app_alloc();
    if(!app) return 1;

    app_switch_view(app, AppViewScanner);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
