
#include "ui.h"
#include "parser/gs1_parser.h"

#include <notification/notification_messages.h>

#define LOG_TAG "gs1_rfid_parser_parse_scene"

static void raw_data_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UNUSED(result);
    UNUSED(type);

    UI* ui = context;
    scene_manager_next_scene(ui->scene_manager, RawDataDisplay);
}

void parse_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    widget_reset(ui->parse_scene->parse_widget);
    widget_add_button_element(
        ui->parse_scene->parse_widget, GuiButtonTypeRight, "Raw Data", raw_data_callback, ui);

    ParsedTagInformation tag_info;
    ParsingResult result = parse_epc(ui->epc_data, &tag_info);

    FuriString* data_str = furi_string_alloc();
    furi_string_reserve(data_str, 1024);

    switch(result) {
    case PARSING_SUCCESS:
        furi_string_cat_printf(data_str, "\e#%s", tag_info.type);

        for(uint8_t i = 0; i < tag_info.ai_count; i++) {
            furi_string_cat_printf(
                data_str,
                "\nAI %02X: %s",
                tag_info.ai_list[i].ai_identifier,
                tag_info.ai_list[i].ai_value);
        }

        // Append new lines at the end to to prevent the button from covering the last line
        furi_string_cat_printf(data_str, "\n\n");
        break;
    case PARSING_NOT_GTIN:
        furi_string_cat_printf(data_str, "\e#Not a SGTIN EPC");
        break;
    case PARSING_MALFORMED_DATA:
        furi_string_cat_printf(data_str, "\e#Malformed EPC data");
        break;
    default:
        furi_string_cat_printf(data_str, "\e#Unknown parsing error");
    }

    widget_add_text_scroll_element(
        ui->parse_scene->parse_widget, 0, 0, 128, 64, furi_string_get_cstr(data_str));
    furi_string_free(data_str);

    notification_message(ui->notifications, &sequence_display_backlight_on);
    notification_message(
        ui->notifications,
        result == PARSING_SUCCESS ? &sequence_set_green_255 : &sequence_set_red_255);

    view_dispatcher_switch_to_view(ui->view_dispatcher, View_ParseDisplay);
}

bool parse_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }

    // Default event handler
    return false;
}

void parse_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    widget_reset(ui->parse_scene->parse_widget);
    notification_message(ui->notifications, &sequence_reset_green);
    notification_message(ui->notifications, &sequence_reset_blue);
    notification_message(ui->notifications, &sequence_reset_red);
}

ParseScene* parse_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    ParseScene* parse_scene = malloc(sizeof(ParseScene));
    parse_scene->parse_widget = widget_alloc();

    return parse_scene;
}

void parse_scene_free(ParseScene* parse_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(parse_scene);

    widget_free(parse_scene->parse_widget);
    free(parse_scene);
}
