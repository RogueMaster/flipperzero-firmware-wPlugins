#include "enc_reader.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_power.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>

#include <input/input.h>

#include <notification/notification.h>

static void app_change_view(EncApp* app, ViewIndex index);

/*******************************************************************
 *                   INPUT / OUTPUT FUNCTIONS 
 *******************************************************************/
static void enc_reader_app_interrupt_callback(void* context) {
    furi_assert(context);
    EncApp* app = context;

    if(furi_hal_gpio_read(app->input_pin.b))
        app->coordinates.abs++;
    else
        app->coordinates.abs--;
}

static void enc_reader_setup_gpio_inputs(EncApp* app) {
    app->input_pin.a = &gpio_ext_pa4;
    app->input_pin.b = &gpio_ext_pa7;

    furi_hal_gpio_init(app->input_pin.a, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_init(app->input_pin.b, GpioModeInput, GpioPullUp, GpioSpeedVeryHigh);

    furi_hal_gpio_add_int_callback(app->input_pin.a, enc_reader_app_interrupt_callback, app);
    furi_hal_gpio_enable_int_callback(app->input_pin.a);
}

static void enc_reader_update_vbus_state(VbusState state) {
    if(state == VbusON) {
        furi_hal_power_enable_otg();
    } else if(state == VbusOFF) {
        furi_hal_power_disable_otg();
    }
}

/*******************************************************************
 *                   SETTINGS VIEW 
 *******************************************************************/
static void variable_item_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    EncApp* app = context;
    if(index == ItemIndexStart) {
        //if(index == VbusON)         {notification_message(app->notifications, &vOn_led_sequence);}
        //else if(index == VbusOFF)   {notification_message(app->notifications, &vOff_led_sequence);}
        app_change_view(app, ViewIndexMain);
    }
}

static void variable_item_change_callback(VariableItem* item) {
    EncApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(item == app->var_item[ItemIndexResolution]) {
        variable_item_set_current_value_text(item, resolution_list[index].text);

        app->resolution = index;

    } else if(item == app->var_item[ItemIndexVbusState]) {
        variable_item_set_current_value_text(item, vbus_state_list[index]);
        app->Vbus_state = index;
        //enc_reader_update_vbus_state(index);
    }
}

static void app_view_settings_alloc(EncApp* app) {
    app->var_item_list = variable_item_list_alloc();

    variable_item_list_set_enter_callback(app->var_item_list, variable_item_enter_callback, app);

    app->var_item[ItemIndexStart] =
        variable_item_list_add(app->var_item_list, "Start", 0, NULL, NULL);
    app->var_item[ItemIndexResolution] = variable_item_list_add(
        app->var_item_list, "Resolution", ResolutionMAX, variable_item_change_callback, app);
    app->var_item[ItemIndexVbusState] = variable_item_list_add(
        app->var_item_list, "5V supply", VbusStatesNum, variable_item_change_callback, app);

    variable_item_set_current_value_text(
        app->var_item[ItemIndexResolution], resolution_list[app->resolution].text);
    variable_item_set_current_value_text(
        app->var_item[ItemIndexVbusState], vbus_state_list[app->Vbus_state]);
}

static void app_view_settings_free(EncApp* app) {
    furi_assert(app);
    variable_item_list_free(app->var_item_list);
}

/*******************************************************************
 *                   MAIN VIEW 
 *******************************************************************/
static void
    enc_reader_app_button_callback(GuiButtonType button_type, InputType input_type, void* context) {
    furi_assert(context);
    EncApp* app = context;
    // Only request the view switch if the user short-presses the Center button.
    if(input_type == InputTypeShort) {
        if(button_type == GuiButtonTypeLeft) {
            app_change_view(app, ViewIndexSettings);
        } else if(button_type == GuiButtonTypeCenter) {
            app->coordinates.org = app->coordinates.abs;
            notification_message(app->notifications, &button_led_sequence);
        } else if(button_type == GuiButtonTypeRight) {
            app->coordinates.org = 0;
            app->coordinates.abs = 0;
            notification_message(app->notifications, &button_led_sequence);
        }
    }
}

static void app_view_main_alloc(EncApp* app) {
    app->widget = widget_alloc();
}

static void app_view_main_free(EncApp* app) {
    furi_assert(app);
    widget_free(app->widget);
}

/*******************************************************************
 *                   VIEW DISPATCHER FUNCTIONS 
 *******************************************************************/
static void app_change_view(EncApp* app, ViewIndex index) {
    if(index < ViewIndexMAX) {
        app->current_view = index;
        enc_reader_update_vbus_state(index == ViewIndexMain ? app->Vbus_state : VbusOFF);
        view_dispatcher_switch_to_view(app->view_dispatcher, index);
    }
}

static void enc_reader_app_tick_event_callback(void* context) {
    furi_assert(context);
    EncApp* app = context;

    if(app->current_view == ViewIndexMain) {
        static const uint8_t screen_width = 128;
        static const uint8_t offset_vertical = 2;
        static const uint8_t offset_horizontal = 2;
        static const uint8_t offset_bottom = 14;
        static const uint8_t gap = 16;

        app->coordinates.rel = app->coordinates.abs - app->coordinates.org;

        char string_abs[] = "00000000 ";
        char string_rel[] = "00000000 ";

        int32_t asb_pos = app->coordinates.abs / resolution_list[app->resolution].divider;
        int32_t rel_pos = app->coordinates.rel / resolution_list[app->resolution].divider;

        snprintf(string_abs, strlen(string_abs), "%8d", (int)asb_pos);
        snprintf(string_rel, strlen(string_rel), "%8d", (int)rel_pos);

        widget_reset(app->widget);

        widget_add_frame_element(app->widget, 0, 0, screen_width, 64 - offset_bottom, 2);

        widget_add_string_element(
            app->widget,
            offset_horizontal,
            offset_vertical,
            AlignLeft,
            AlignTop,
            FontSecondary,
            app->Vbus_state == VbusON ? "5V ON " : "5V OFF");
        widget_add_string_element(
            app->widget,
            screen_width - offset_horizontal,
            offset_vertical,
            AlignRight,
            AlignTop,
            FontSecondary,
            resolution_list[app->resolution].text);

        widget_add_string_element(
            app->widget,
            offset_horizontal,
            offset_vertical + gap,
            AlignLeft,
            AlignTop,
            FontPrimary,
            "Abs:");
        widget_add_string_element(
            app->widget,
            offset_horizontal,
            offset_vertical + gap * 2,
            AlignLeft,
            AlignTop,
            FontPrimary,
            "Rel:");

        widget_add_string_element(
            app->widget,
            screen_width - offset_horizontal,
            offset_vertical + gap,
            AlignRight,
            AlignTop,
            FontBigNumbers,
            string_abs);
        widget_add_string_element(
            app->widget,
            screen_width - offset_horizontal,
            offset_vertical + gap * 2,
            AlignRight,
            AlignTop,
            FontBigNumbers,
            string_rel);

        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "Config", enc_reader_app_button_callback, app);
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Org", enc_reader_app_button_callback, app);
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Reset", enc_reader_app_button_callback, app);
    }
}

static bool enc_reader_app_navigation_callback(
    void* context) { // This function is called when the user has pressed the Back key.
    furi_assert(context);
    EncApp* app = context;

    if(app->current_view == ViewIndexMain) {
        app_change_view(app, ViewIndexSettings);
    } else {
        view_dispatcher_stop(app->view_dispatcher);
    }
    return true;
}

static void app_view_dispatcher_alloc(EncApp* app) {
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_add_view(app->view_dispatcher, ViewIndexMain, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIndexSettings, variable_item_list_get_view(app->var_item_list));

    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, enc_reader_app_tick_event_callback, 40);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, enc_reader_app_navigation_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
}

static void app_view_dispatcher_free(EncApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, ViewIndexSettings);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIndexMain);

    view_dispatcher_free(app->view_dispatcher);
}

/*******************************************************************
 *                   ALLOC FUNCTIONS 
 *******************************************************************/
EncApp* enc_reader_app_alloc() {
    EncApp* app = malloc(sizeof(EncApp));

    app->gui = furi_record_open(RECORD_GUI);
    //app->view_port	= view_port_alloc();
    //app->event_queue	= furi_message_queue_alloc(8, sizeof(InputEvent));

    enc_reader_setup_gpio_inputs(app);
    app_view_settings_alloc(app);
    app_view_main_alloc(app);
    app_view_dispatcher_alloc(app);

    //gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    //view_port_draw_callback_set(app->view_port, enc_reader_app_draw_callback, app);
    //view_port_input_callback_set(app->view_port, enc_reader_app_input_callback, app->event_queue);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    return app;
}

void enc_reader_app_free(EncApp* app) {
    furi_assert(app);

    app_view_dispatcher_free(app);
    app_view_settings_free(app);
    app_view_main_free(app);

    furi_hal_gpio_disable_int_callback(app->input_pin.a);
    furi_hal_gpio_remove_int_callback(app->input_pin.a);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
}

static void enc_reader_app_run(EncApp* app) {
    app->resolution = ResolutionRaw;
    app->Vbus_state = VbusOFF;

    app_change_view(app, ViewIndexSettings);

    variable_item_set_current_value_index(app->var_item[ItemIndexResolution], app->resolution);
    variable_item_set_current_value_index(app->var_item[ItemIndexVbusState], app->Vbus_state);

    view_dispatcher_run(app->view_dispatcher);
}

/*******************************************************************
 *                  vvv START HERE vvv
 *******************************************************************/
int32_t enc_reader_app(void* p) {
    UNUSED(p);
    EncApp* app = enc_reader_app_alloc();
    enc_reader_app_run(app);
    furi_hal_power_disable_otg();
    enc_reader_app_free(app);
    return 0;
}
