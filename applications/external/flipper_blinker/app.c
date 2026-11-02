#include "app.h"

#include "gpio.h"
#include "morse.h"

#include <ctype.h>
#include <gui/gui.h>
#include <gui/elements.h>

#define SETUP_BLINK_UNIT_MS (200U)

typedef enum {
    SettingsItemSpeed,
    SettingsItemRepeat,
    SettingsItemRepeatDelay,
    SettingsItemLedPin,
    SettingsItemMadeBy,
} SettingsItem;

static bool flipper_blinker_navigation_callback(void* context);
static bool flipper_blinker_custom_event_callback(void* context, uint32_t event);
static void flipper_blinker_build_settings(FlipperBlinkerApp* app);

static void flipper_blinker_switch_to_view(FlipperBlinkerApp* app, FlipperBlinkerView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static void flipper_blinker_show_widget_text(
    FlipperBlinkerApp* app,
    const char* text,
    const char* center_button,
    ButtonCallback center_callback) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget, 64, 26, AlignCenter, AlignCenter, FontPrimary, text);
    if(center_button) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, center_button, center_callback, app);
    }
    flipper_blinker_switch_to_view(app, FlipperBlinkerViewWidget);
}

static void flipper_blinker_setup_repeat_callback(
    GuiButtonType button_type,
    InputType input_type,
    void* context) {
    FlipperBlinkerApp* app = context;
    if((button_type == GuiButtonTypeCenter) && (input_type == InputTypeShort)) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipperBlinkerEventSetupRepeatTest);
    }
}

static void flipper_blinker_setup_confirm_callback(
    GuiButtonType button_type,
    InputType input_type,
    void* context) {
    FlipperBlinkerApp* app = context;
    if(input_type != InputTypeShort) return;

    if(button_type == GuiButtonTypeRight) {
        flipper_blinker_show_widget_text(app, "Setup Complete", NULL, NULL);
        furi_delay_ms(700);
        flipper_blinker_show_main_menu(app);
    } else if(button_type == GuiButtonTypeLeft) {
        widget_reset(app->widget);
        widget_add_text_scroll_element(
            app->widget,
            0,
            0,
            128,
            54,
            "\e#Wiring\e#\n"
            "PC3 -> 220 Ohm resistor -> LED -> GND\n\n"
            "Labels:\n"
            "PC3\n"
            "220 Ohm\n"
            "Long leg\n"
            "Short leg\n"
            "GND\n\n"
            "Connect the LED long leg toward PC3.\n"
            "Connect the short leg toward GND.");
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Repeat", flipper_blinker_setup_repeat_callback, app);
        flipper_blinker_switch_to_view(app, FlipperBlinkerViewWidget);
    }
}

static int32_t flipper_blinker_setup_worker(void* context) {
    FlipperBlinkerApp* app = context;
    flipper_blinker_gpio_init(app->settings.led_pin);
    for(uint8_t i = 0; i < 5U; i++) {
        led_on();
        furi_delay_ms(SETUP_BLINK_UNIT_MS);
        led_off();
        furi_delay_ms(SETUP_BLINK_UNIT_MS);
    }
    flipper_blinker_gpio_deinit(app->settings.led_pin);
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipperBlinkerEventSetupTestDone);
    return 0;
}

static bool flipper_blinker_worker_should_stop(FlipperBlinkerApp* app, uint32_t delay_ms) {
    uint32_t elapsed = 0;
    while(elapsed < delay_ms) {
        if(app->worker_stop) return true;
        uint32_t step = MIN(20U, delay_ms - elapsed);
        furi_delay_ms(step);
        elapsed += step;
    }
    return app->worker_stop;
}

static bool flipper_blinker_send_symbol(FlipperBlinkerApp* app, char symbol, uint32_t unit_ms) {
    if(symbol == '.') {
        led_on();
        if(flipper_blinker_worker_should_stop(app, unit_ms)) {
            led_off();
            return false;
        }
    } else {
        led_on();
        if(flipper_blinker_worker_should_stop(app, unit_ms * 3U)) {
            led_off();
            return false;
        }
    }
    led_off();
    return true;
}

static int32_t flipper_blinker_transmission_worker(void* context) {
    FlipperBlinkerApp* app = context;
    const uint32_t unit_ms = flipper_blinker_morse_unit_ms(app->settings.wpm);
    const size_t total = flipper_blinker_morse_count_supported(app->text);

    flipper_blinker_gpio_init(app->settings.led_pin);

    do {
        size_t progress = 0;
        for(size_t i = 0; app->text[i] != '\0'; i++) {
            char character = flipper_blinker_morse_normalize_char(app->text[i]);
            if(!flipper_blinker_morse_is_supported(character)) continue;

            progress++;
            flipper_blinker_transmit_view_update(app->transmit_view, character, progress, total);

            if(character == ' ') {
                if(flipper_blinker_worker_should_stop(app, unit_ms * 7U)) break;
                continue;
            }

            const char* code = flipper_blinker_morse_get(character);
            for(size_t symbol = 0; code[symbol] != '\0'; symbol++) {
                if(!flipper_blinker_send_symbol(app, code[symbol], unit_ms)) break;
                if(code[symbol + 1] != '\0') {
                    if(flipper_blinker_worker_should_stop(app, unit_ms)) break;
                }
            }

            led_off();
            if(app->worker_stop) break;
            if((app->text[i + 1] != '\0') && (app->text[i + 1] != ' ')) {
                if(flipper_blinker_worker_should_stop(app, unit_ms * 3U)) break;
            }
        }

        if(!app->settings.repeat || app->worker_stop) break;
        flipper_blinker_transmit_view_set_status(app->transmit_view, "Repeat delay");
    } while(!flipper_blinker_worker_should_stop(app, app->settings.repeat_delay_seconds * 1000U));

    led_off();
    flipper_blinker_gpio_deinit(app->settings.led_pin);
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        app->worker_stop ? FlipperBlinkerEventTransmissionStopped :
                           FlipperBlinkerEventTransmissionDone);
    return 0;
}

static void flipper_blinker_worker_join_and_free(FlipperBlinkerApp* app) {
    if(app->worker_thread) {
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
}

void flipper_blinker_stop_transmission(FlipperBlinkerApp* app) {
    furi_assert(app);
    app->worker_stop = true;
}

static uint32_t flipper_blinker_transmit_back_callback(void* context) {
    FlipperBlinkerApp* app = context;
    flipper_blinker_stop_transmission(app);
    return VIEW_IGNORE;
}

void flipper_blinker_start_setup_test(FlipperBlinkerApp* app) {
    furi_assert(app);
    flipper_blinker_worker_join_and_free(app);
    flipper_blinker_show_widget_text(app, "Blink Test", NULL, NULL);
    app->worker_stop = false;
    app->worker_thread =
        furi_thread_alloc_ex("BlinkerSetup", 1024, flipper_blinker_setup_worker, app);
    furi_thread_start(app->worker_thread);
}

void flipper_blinker_start_transmission(FlipperBlinkerApp* app) {
    furi_assert(app);
    flipper_blinker_worker_join_and_free(app);
    app->worker_stop = false;
    flipper_blinker_transmit_view_reset(app->transmit_view, app->text);
    flipper_blinker_switch_to_view(app, FlipperBlinkerViewTransmit);
    app->worker_thread =
        furi_thread_alloc_ex("BlinkerTx", 1536, flipper_blinker_transmission_worker, app);
    furi_thread_start(app->worker_thread);
}

static void flipper_blinker_text_input_done(void* context) {
    FlipperBlinkerApp* app = context;
    for(size_t i = 0; app->text[i] != '\0'; i++) {
        app->text[i] = flipper_blinker_morse_normalize_char(app->text[i]);
        if(!flipper_blinker_morse_is_supported(app->text[i])) {
            app->text[i] = ' ';
        }
    }
    flipper_blinker_start_transmission(app);
}

static bool flipper_blinker_text_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    for(size_t i = 0; text[i] != '\0'; i++) {
        if(!flipper_blinker_morse_is_supported(text[i])) {
            furi_string_set(error, "Use A-Z, 0-9, and space");
            return false;
        }
    }
    return true;
}

static void flipper_blinker_show_text_input(FlipperBlinkerApp* app) {
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Message");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_validator(app->text_input, flipper_blinker_text_validator, app);
    text_input_set_result_callback(
        app->text_input, flipper_blinker_text_input_done, app, app->text, sizeof(app->text), false);
    flipper_blinker_switch_to_view(app, FlipperBlinkerViewTextInput);
}

static void flipper_blinker_speed_changed(VariableItem* item) {
    FlipperBlinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.wpm = FLIPPER_BLINKER_WPM_MIN + index;
    char text[8];
    snprintf(text, sizeof(text), "%u", app->settings.wpm);
    variable_item_set_current_value_text(item, text);
    flipper_blinker_settings_save(&app->settings);
}

static void flipper_blinker_repeat_changed(VariableItem* item) {
    FlipperBlinkerApp* app = variable_item_get_context(item);
    app->settings.repeat = variable_item_get_current_value_index(item) != 0;
    variable_item_set_current_value_text(item, app->settings.repeat ? "On" : "Off");
    flipper_blinker_settings_save(&app->settings);
}

static void flipper_blinker_repeat_delay_show_value(VariableItem* item) {
    FlipperBlinkerApp* app = variable_item_get_context(item);
    char text[12];
    snprintf(text, sizeof(text), "%us", app->settings.repeat_delay_seconds);
    variable_item_set_current_value_text(item, text);
}

static void flipper_blinker_led_pin_changed(VariableItem* item) {
    FlipperBlinkerApp* app = variable_item_get_context(item);
    app->settings.led_pin = FlipperBlinkerLedPinPc3;
    variable_item_set_current_value_text(
        item, flipper_blinker_settings_pin_name(app->settings.led_pin));
    flipper_blinker_settings_save(&app->settings);
}

static void flipper_blinker_made_by_show_value(VariableItem* item) {
    variable_item_set_current_value_text(item, "T3chieJack");
}

static void flipper_blinker_repeat_delay_input_done(void* context, int32_t number) {
    FlipperBlinkerApp* app = context;
    app->settings.repeat_delay_seconds = number;
    flipper_blinker_settings_save(&app->settings);
    flipper_blinker_build_settings(app);
    flipper_blinker_switch_to_view(app, FlipperBlinkerViewSettings);
}

static void flipper_blinker_settings_enter_callback(void* context, uint32_t index) {
    FlipperBlinkerApp* app = context;
    if(index == SettingsItemRepeatDelay) {
        number_input_set_header_text(app->number_input, "Repeat Delay");
        number_input_set_result_callback(
            app->number_input,
            flipper_blinker_repeat_delay_input_done,
            app,
            app->settings.repeat_delay_seconds,
            FLIPPER_BLINKER_REPEAT_DELAY_MIN,
            FLIPPER_BLINKER_REPEAT_DELAY_MAX);
        flipper_blinker_switch_to_view(app, FlipperBlinkerViewNumberInput);
    }
}

static void flipper_blinker_build_settings(FlipperBlinkerApp* app) {
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, flipper_blinker_settings_enter_callback, app);

    VariableItem* item = variable_item_list_add(
        app->settings_list,
        "Speed (WPM)",
        FLIPPER_BLINKER_WPM_MAX - FLIPPER_BLINKER_WPM_MIN + 1,
        flipper_blinker_speed_changed,
        app);
    variable_item_set_current_value_index(item, app->settings.wpm - FLIPPER_BLINKER_WPM_MIN);
    flipper_blinker_speed_changed(item);

    item = variable_item_list_add(
        app->settings_list, "Repeat", 2, flipper_blinker_repeat_changed, app);
    variable_item_set_current_value_index(item, app->settings.repeat ? 1 : 0);
    flipper_blinker_repeat_changed(item);

    item = variable_item_list_add(
        app->settings_list, "Repeat Delay", 1, flipper_blinker_repeat_delay_show_value, app);
    variable_item_set_current_value_index(item, 0);
    flipper_blinker_repeat_delay_show_value(item);

    item = variable_item_list_add(
        app->settings_list, "LED Pin", 1, flipper_blinker_led_pin_changed, app);
    variable_item_set_current_value_index(item, 0);
    flipper_blinker_led_pin_changed(item);

    item = variable_item_list_add(
        app->settings_list, "Made by", 1, flipper_blinker_made_by_show_value, app);
    variable_item_set_current_value_index(item, 0);
    flipper_blinker_made_by_show_value(item);

    variable_item_list_set_selected_item(app->settings_list, SettingsItemSpeed);
}

static void flipper_blinker_show_about(FlipperBlinkerApp* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Flipper Blinker\n"
        "Version 1.0\n\n"
        "External Morse LED transmitter\n"
        "Uses GPIO PC3");
    flipper_blinker_switch_to_view(app, FlipperBlinkerViewWidget);
}

static void flipper_blinker_menu_callback(void* context, uint32_t index) {
    FlipperBlinkerApp* app = context;
    if(index == FlipperBlinkerMenuSetup) {
        flipper_blinker_start_setup_test(app);
    } else if(index == FlipperBlinkerMenuSendMessage) {
        flipper_blinker_show_text_input(app);
    } else if(index == FlipperBlinkerMenuSettings) {
        flipper_blinker_build_settings(app);
        flipper_blinker_switch_to_view(app, FlipperBlinkerViewSettings);
    } else if(index == FlipperBlinkerMenuAbout) {
        flipper_blinker_show_about(app);
    }
}

void flipper_blinker_show_main_menu(FlipperBlinkerApp* app) {
    furi_assert(app);
    flipper_blinker_switch_to_view(app, FlipperBlinkerViewMainMenu);
}

static bool flipper_blinker_navigation_callback(void* context) {
    FlipperBlinkerApp* app = context;
    if(app->worker_thread &&
       (furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped)) {
        app->worker_stop = true;
        return true;
    }
    if(app->current_view == FlipperBlinkerViewMainMenu) {
        view_dispatcher_stop(app->view_dispatcher);
    } else {
        flipper_blinker_show_main_menu(app);
    }
    return true;
}

static bool flipper_blinker_custom_event_callback(void* context, uint32_t event) {
    FlipperBlinkerApp* app = context;
    if(event == FlipperBlinkerEventSetupTestDone) {
        flipper_blinker_worker_join_and_free(app);
        widget_reset(app->widget);
        widget_add_string_multiline_element(
            app->widget, 64, 26, AlignCenter, AlignCenter, FontPrimary, "Is the LED blinking?");
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "No", flipper_blinker_setup_confirm_callback, app);
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Yes", flipper_blinker_setup_confirm_callback, app);
        flipper_blinker_switch_to_view(app, FlipperBlinkerViewWidget);
    } else if(event == FlipperBlinkerEventSetupRepeatTest) {
        flipper_blinker_start_setup_test(app);
    } else if(
        (event == FlipperBlinkerEventTransmissionDone) ||
        (event == FlipperBlinkerEventTransmissionStopped)) {
        flipper_blinker_worker_join_and_free(app);
        flipper_blinker_show_main_menu(app);
    }
    return true;
}

FlipperBlinkerApp* flipper_blinker_app_alloc(void) {
    FlipperBlinkerApp* app = malloc(sizeof(FlipperBlinkerApp));
    memset(app, 0, sizeof(FlipperBlinkerApp));

    flipper_blinker_settings_load(&app->settings);
    strlcpy(app->text, "SOS", sizeof(app->text));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flipper_blinker_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flipper_blinker_navigation_callback);

    app->main_menu = submenu_alloc();
    submenu_set_header(app->main_menu, "Flipper Blinker");
    submenu_add_item(
        app->main_menu, "Setup", FlipperBlinkerMenuSetup, flipper_blinker_menu_callback, app);
    submenu_add_item(
        app->main_menu,
        "Send Message",
        FlipperBlinkerMenuSendMessage,
        flipper_blinker_menu_callback,
        app);
    submenu_add_item(
        app->main_menu, "Settings", FlipperBlinkerMenuSettings, flipper_blinker_menu_callback, app);
    submenu_add_item(
        app->main_menu, "About", FlipperBlinkerMenuAbout, flipper_blinker_menu_callback, app);

    app->text_input = text_input_alloc();
    app->number_input = number_input_alloc();
    app->settings_list = variable_item_list_alloc();
    app->widget = widget_alloc();
    app->transmit_view = flipper_blinker_transmit_view_alloc();
    flipper_blinker_transmit_view_set_callback(
        app->transmit_view, flipper_blinker_transmit_back_callback, app);

    view_dispatcher_add_view(
        app->view_dispatcher, FlipperBlinkerViewMainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, FlipperBlinkerViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperBlinkerViewNumberInput,
        number_input_get_view(app->number_input));
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperBlinkerViewSettings,
        variable_item_list_get_view(app->settings_list));
    view_dispatcher_add_view(
        app->view_dispatcher, FlipperBlinkerViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperBlinkerViewTransmit,
        flipper_blinker_transmit_view_get_view(app->transmit_view));

    return app;
}

void flipper_blinker_app_free(FlipperBlinkerApp* app) {
    furi_assert(app);

    app->worker_stop = true;
    flipper_blinker_worker_join_and_free(app);
    flipper_blinker_settings_save(&app->settings);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperBlinkerViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperBlinkerViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperBlinkerViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperBlinkerViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperBlinkerViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperBlinkerViewTransmit);

    submenu_free(app->main_menu);
    text_input_free(app->text_input);
    number_input_free(app->number_input);
    variable_item_list_free(app->settings_list);
    widget_free(app->widget);
    flipper_blinker_transmit_view_free(app->transmit_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    free(app);
}
