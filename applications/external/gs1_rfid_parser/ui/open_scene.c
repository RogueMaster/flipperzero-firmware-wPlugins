
#include "ui.h"
#include "missing_api.h"

#include <notification/notification_messages.h>

#define LOG_TAG "gs1_rfid_parser_open_scene"

static void update_widget_display(UI* ui);

static void scan_for_tags_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UNUSED(result);
    UNUSED(type);

    UI* ui = context;
    scene_manager_next_scene(ui->scene_manager, ScanDisplay);
}

static void retry_module_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UNUSED(result);
    UNUSED(type);

    UI* ui = context;
    update_widget_display(ui);
}

static void update_widget_display(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);

    widget_reset(ui->open_scene->open_widget);

    char hw_version[64] = "HW: ";
    size_t hw_version_prefix_len = local_strnlen(hw_version, 64);

    char sw_version[64] = "SW: ";
    size_t sw_version_prefix_len = local_strnlen(sw_version, 64);

    char manufacturer[64] = "Manufacturer: ";
    size_t manufacturer_len = local_strnlen(manufacturer, 64);

    int32_t result = uhf_u107_get_hw_version(
        ui->uhf_device, &hw_version[hw_version_prefix_len], 64 - hw_version_prefix_len);

    if(result > 0) {
        result = uhf_u107_get_sw_version(
            ui->uhf_device, &sw_version[sw_version_prefix_len], 64 - sw_version_prefix_len);
    }

    if(result > 0) {
        result = uhf_u107_get_manufacturer(
            ui->uhf_device, &manufacturer[manufacturer_len], 64 - manufacturer_len);
    }

    if(result > 0) {
        result = uhf_u107_configure_device(ui->uhf_device);
    }

    widget_add_string_element(
        ui->open_scene->open_widget,
        0,
        0,
        AlignLeft,
        AlignTop,
        FontPrimary,
        "GS1 RFID Parsing v" FAP_VERSION);
    if(result > 0) {
        widget_add_string_element(
            ui->open_scene->open_widget, 0, 15, AlignLeft, AlignTop, FontSecondary, hw_version);
        widget_add_string_element(
            ui->open_scene->open_widget, 0, 25, AlignLeft, AlignTop, FontSecondary, sw_version);
        widget_add_string_element(
            ui->open_scene->open_widget, 0, 35, AlignLeft, AlignTop, FontSecondary, manufacturer);
        widget_add_button_element(
            ui->open_scene->open_widget,
            GuiButtonTypeCenter,
            "Scan for Tags",
            scan_for_tags_callback,
            ui);
    } else if(result == 0) {
        widget_add_string_element(
            ui->open_scene->open_widget,
            0,
            15,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "Failed to find UHF Antenna");
        widget_add_button_element(
            ui->open_scene->open_widget,
            GuiButtonTypeCenter,
            "Search for UHF Reader",
            retry_module_callback,
            ui);
    } else {
        FURI_LOG_E(LOG_TAG, "Failed to get information from UHF controller: %ld", result);
        widget_add_string_element(
            ui->open_scene->open_widget,
            0,
            15,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "Error Getting Device Information");
        widget_add_string_element(
            ui->open_scene->open_widget,
            0,
            25,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "See logs for details");
        widget_add_button_element(
            ui->open_scene->open_widget,
            GuiButtonTypeCenter,
            "Search for UHF Reader",
            retry_module_callback,
            ui);
    }
}

void open_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    update_widget_display(ui);
    view_dispatcher_switch_to_view(ui->view_dispatcher, View_OpenDisplay);

    notification_message(ui->notifications, &sequence_display_backlight_on);
}

bool open_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }

    // Default event handler
    return false;
}

void open_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    widget_reset(ui->open_scene->open_widget);
}

OpenScene* open_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    OpenScene* open_scene = malloc(sizeof(OpenScene));
    open_scene->open_widget = widget_alloc();

    return open_scene;
}

void open_scene_free(OpenScene* open_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(open_scene);

    widget_free(open_scene->open_widget);
    free(open_scene);
}
