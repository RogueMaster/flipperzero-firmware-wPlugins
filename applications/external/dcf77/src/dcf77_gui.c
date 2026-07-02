#include "dcf77_gui.h"

#include "dcf77_logic.h"
#include "dcf77_scenes.h"
#include "dcf77_icons.h"

static const char about_text[] =
    "Designed by arha, to debug my radioclocks that never seem to work just right.\n"
    "Supports DCF77, WWVB, MSF, JJY, BPC, BSF and the defunct HBG.\n"
    "Extended TX on subghz like this app does is probably not healthy; the frequency will also start drifting.\n"
    "https://github.com/arha\n";

static const char egg_text[] = "wow you found an easter egg\n";

enum {
    DCF77_ABOUT_TEXT_X = 4,
    DCF77_ABOUT_TEXT_Y = 4,
    DCF77_ABOUT_TEXT_WIDTH = 116,
    DCF77_ABOUT_TEXT_HEIGHT = 46,
    DCF77_ABOUT_SCROLLBAR_X = 124,
    DCF77_ABOUT_LINE_BUFFER_SIZE = 96,
};

typedef struct {
    uint8_t scroll_pos;
    uint8_t scroll_pos_total;
} Dcf77AboutViewModel;

static size_t dcf77_about_next_line(
    Canvas* canvas,
    const char* text,
    size_t start,
    char* line,
    size_t line_size) {
    size_t pos = start;
    size_t len = 0U;
    size_t width = 0U;
    size_t last_space_pos = 0U;
    size_t last_space_len = 0U;
    bool has_last_space = false;

    if(text[pos] == '\0') {
        line[0] = '\0';
        return pos;
    }

    if(text[pos] == '\n') {
        line[0] = '\0';
        return pos + 1U;
    }

    while(text[pos] != '\0' && text[pos] != '\n') {
        const char ch = text[pos];
        const size_t glyph_width = canvas_glyph_width(canvas, ch);

        if(len > 0U && (width + glyph_width) > DCF77_ABOUT_TEXT_WIDTH) {
            if(has_last_space) {
                pos = last_space_pos + 1U;
                len = last_space_len;
            }
            break;
        }

        if(len + 1U < line_size) {
            line[len++] = ch;
        }
        width += glyph_width;

        if(ch == ' ') {
            has_last_space = true;
            last_space_pos = pos;
            last_space_len = len - 1U;
        }

        pos++;
    }

    if(pos == start) {
        line[0] = text[pos];
        line[1] = '\0';
        pos++;
    } else {
        while(len > 0U && line[len - 1U] == ' ') {
            len--;
        }
        line[len] = '\0';
    }

    if(text[pos] == '\n') {
        pos++;
    }

    return pos;
}

static uint8_t dcf77_about_visible_lines(const CanvasFontParameters* font_params) {
    uint8_t lines = 0U;
    uint16_t y = 0U;

    while((y + font_params->descender) <= DCF77_ABOUT_TEXT_HEIGHT) {
        lines++;
        y += font_params->leading_default;
    }

    return lines > 0U ? lines : 1U;
}

static uint8_t dcf77_about_total_lines(Canvas* canvas) {
    char line[DCF77_ABOUT_LINE_BUFFER_SIZE];
    size_t pos = 0U;
    uint8_t total = 0U;

    while(about_text[pos] != '\0') {
        const size_t next = dcf77_about_next_line(canvas, about_text, pos, line, sizeof(line));
        total++;
        if(next <= pos) {
            break;
        }
        pos = next;
    }

    return total > 0U ? total : 1U;
}

static void dcf77_about_enter_callback(void* ctx) {
    AppFSM* app_fsm = ctx;

    with_view_model(
        app_fsm->about_view,
        Dcf77AboutViewModel * model,
        {
            model->scroll_pos = 0U;
            model->scroll_pos_total = 1U;
        },
        true);
}

static void dcf77_about_render_callback(Canvas* canvas, void* model) {
    Dcf77AboutViewModel* about_model = model;
    char line[DCF77_ABOUT_LINE_BUFFER_SIZE];
    const CanvasFontParameters* font_params;
    uint8_t visible_lines;
    uint8_t total_lines;
    uint8_t first_line;
    uint8_t last_line;
    uint8_t line_index = 0U;
    uint8_t draw_y = DCF77_ABOUT_TEXT_Y;
    size_t pos = 0U;

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    elements_button_left(canvas, "Back");
    canvas_set_font(canvas, FontSecondary);

    font_params = canvas_get_font_params(canvas, FontSecondary);
    visible_lines = dcf77_about_visible_lines(font_params);
    total_lines = dcf77_about_total_lines(canvas);
    about_model->scroll_pos_total =
        total_lines > visible_lines ? (uint8_t)(total_lines - visible_lines + 1U) : 1U;
    if(about_model->scroll_pos >= about_model->scroll_pos_total) {
        about_model->scroll_pos = about_model->scroll_pos_total - 1U;
    }

    first_line = about_model->scroll_pos;
    last_line = first_line + visible_lines;

    while(about_text[pos] != '\0') {
        const size_t next = dcf77_about_next_line(canvas, about_text, pos, line, sizeof(line));

        if(line_index >= first_line && line_index < last_line) {
            canvas_draw_str_aligned(canvas, DCF77_ABOUT_TEXT_X, draw_y, AlignLeft, AlignTop, line);
            draw_y += font_params->leading_default;
        }

        line_index++;
        if(next <= pos || line_index >= last_line) {
            break;
        }
        pos = next;
    }

    if(about_model->scroll_pos_total > 1U) {
        elements_scrollbar_pos(
            canvas,
            DCF77_ABOUT_SCROLLBAR_X,
            DCF77_ABOUT_TEXT_Y,
            DCF77_ABOUT_TEXT_HEIGHT,
            about_model->scroll_pos,
            about_model->scroll_pos_total);
    }
}

static bool dcf77_about_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyLeft:
        case InputKeyBack:
            dcf77_app_switch_to_menu(app_fsm);
            return true;
        case InputKeyRight:
            dcf77_app_switch_to_egg(app_fsm);
            return true;
        default:
            break;
        }
    }

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        bool handled = false;

        with_view_model(
            app_fsm->about_view,
            Dcf77AboutViewModel * model,
            {
                if(event->key == InputKeyUp) {
                    if(model->scroll_pos > 0U) {
                        model->scroll_pos--;
                    }
                    handled = true;
                } else if(event->key == InputKeyDown) {
                    if(model->scroll_pos + 1U < model->scroll_pos_total) {
                        model->scroll_pos++;
                    }
                    handled = true;
                }
            },
            handled);

        if(handled) {
            return true;
        }
    }

    return false;
}

static const char* dcf77_tx_lf_state_label(const AppFSM* app_fsm) {
    return app_fsm->lf_ready ? "on" : "off";
}

static void dcf77_tx_render_progress_bar(Canvas* const canvas, const AppFSM* app_fsm) {
    const uint8_t bar_width = 120U;
    const uint8_t bar_x = 4U;
    const uint8_t fill_y = 58U;
    const uint8_t fill_height = 4U;
    const bool frame_enabled = app_fsm->current_signal == RadioClockSignalTest ||
                               app_fsm->tx_frame_enabled;
    uint8_t second = app_fsm->tx_progress_second;
    uint32_t elapsed_ms = 0U;

    if(app_fsm->current_signal == RadioClockSignalTest) {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        second = dt.second;
        elapsed_ms = (furi_get_tick() % furi_kernel_get_tick_frequency()) * 1000U /
                     furi_kernel_get_tick_frequency();
    } else if(furi_get_tick() >= app_fsm->tx_second_start_tick) {
        elapsed_ms = (furi_get_tick() - app_fsm->tx_second_start_tick) * 1000U /
                     furi_kernel_get_tick_frequency();
    }

    if(second >= 60U) {
        second = 59U;
    }
    if(elapsed_ms > 999U) {
        elapsed_ms = 999U;
    }

    uint8_t fill_width = (uint8_t)(second * 2U);
    if(elapsed_ms >= 500U && fill_width < bar_width) {
        fill_width++;
    }
    if(second == 59U && elapsed_ms >= 500U) {
        fill_width = bar_width;
    }

    if(fill_width == 0U) {
        return;
    }

    if(fill_width == 1U) {
        canvas_draw_line(canvas, bar_x, fill_y + 1U, bar_x, fill_y + 2U);
        return;
    }

    canvas_draw_line(canvas, bar_x, fill_y + 1U, bar_x, fill_y + 2U);
    if(frame_enabled && fill_width > 2U) {
        canvas_draw_box(canvas, bar_x + 1U, fill_y, fill_width - 2U, fill_height);
    } else if(fill_width > 2U) {
        canvas_draw_line(canvas, bar_x + 1U, fill_y, bar_x + fill_width - 2U, fill_y);
        canvas_draw_line(
            canvas,
            bar_x + 1U,
            fill_y + fill_height - 1U,
            bar_x + fill_width - 2U,
            fill_y + fill_height - 1U);
    }
    canvas_draw_line(
        canvas, bar_x + fill_width - 1U, fill_y + 1U, bar_x + fill_width - 1U, fill_y + 2U);
}

static void dcf77_tx_get_display_datetime(const AppFSM* app_fsm, DateTime* dt) {
    if(app_fsm->current_signal == RadioClockSignalTest) {
        furi_hal_rtc_get_datetime(dt);
        return;
    }

    dt->year = 2000U + app_fsm->tx_year;
    dt->month = app_fsm->tx_month;
    dt->day = app_fsm->tx_day;
    dt->hour = app_fsm->tx_hour;
    dt->minute = app_fsm->tx_minute;
    dt->second = app_fsm->bit_number;
}

static void dcf77_tx_render_user_mode(Canvas* const canvas, const AppFSM* app_fsm) {
    DateTime dt;
    char buffer[64];

    dcf77_tx_get_display_datetime(app_fsm, &dt);

    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);

    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
    canvas_draw_str_aligned(canvas, 4, 12, AlignLeft, AlignBottom, buffer);
    snprintf(buffer, sizeof(buffer), "%02u.%02u.%02u", dt.day, dt.month, dt.year % 100U);
    canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignBottom, buffer);

    canvas_set_font(canvas, FontSecondary);

    if(app_fsm->lf_ready) {
        snprintf(
            buffer,
            sizeof(buffer),
            "lf: %lu.%03lu kHz",
            (unsigned long)(app_fsm->lf_freq / 1000U),
            (unsigned long)(app_fsm->lf_freq % 1000U));
    } else {
        snprintf(buffer, sizeof(buffer), "lf: off");
    }
    canvas_draw_str(canvas, 4, 24, buffer);

    if(app_fsm->subghz_ready) {
        snprintf(buffer, sizeof(buffer), "subghz: %s MHz", app_fsm->subghz_frequency_text);
    } else {
        snprintf(buffer, sizeof(buffer), "subghz: off");
    }
    canvas_draw_str(canvas, 4, 34, buffer);

    if(dcf77_app_gpio_rf_enabled(app_fsm)) {
        snprintf(
            buffer,
            sizeof(buffer),
            "GPIO: %s RF %s %s",
            app_fsm->gpio_baseband_text,
            app_fsm->gpio_rf_text,
            app_fsm->gpio_rf_duty_text);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "GPIO: %s RF %s",
            app_fsm->gpio_baseband_text,
            app_fsm->gpio_rf_text);
    }
    canvas_draw_str(canvas, 4, 44, buffer);
    dcf77_tx_render_progress_bar(canvas, app_fsm);
}

static void dcf77_gpio_rf_warning_render_callback(Canvas* const canvas, void* model) {
    Dcf77AppViewModel* warning_model = model;
    UNUSED(warning_model);

    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_icon(canvas, 10, 18, &I_warn_small);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 48, 16, "LF and SubGHZ");
    canvas_draw_str(canvas, 48, 28, "disabled with");
    canvas_draw_str(canvas, 48, 40, "GPIO RF enabled");

    elements_button_left(canvas, "Back");
    elements_button_right(canvas, "Next");
}

static void dcf77_tx_render_callback(Canvas* const canvas, void* model) {
    Dcf77AppViewModel* tx_model = model;
    AppFSM* app_fsm = tx_model->app_fsm;
    char buffer[64];

    if(app_fsm->screen_mode == Dcf77ScreenModeUser) {
        dcf77_tx_render_user_mode(canvas, app_fsm);
        return;
    }

    if(app_fsm->current_signal == RadioClockSignalTest) {
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Test");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buffer, sizeof(buffer), "Pulse 0.1s every 0.5s");
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, buffer);
        snprintf(
            buffer,
            sizeof(buffer),
            "LF %s %lu Hz",
            dcf77_tx_lf_state_label(app_fsm),
            (unsigned long)app_fsm->lf_freq);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, buffer);
        snprintf(buffer, sizeof(buffer), "SubGHz %s", app_fsm->subghz_ready ? "on" : "off");
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, buffer);
        dcf77_tx_render_progress_bar(canvas, app_fsm);
        return;
    }

    if(app_fsm->current_signal == RadioClockSignalWwvb ||
       app_fsm->current_signal == RadioClockSignalMsf ||
       app_fsm->current_signal == RadioClockSignalJjy ||
       app_fsm->current_signal == RadioClockSignalBpc ||
       app_fsm->current_signal == RadioClockSignalBsf) {
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            buffer,
            sizeof(buffer),
            "%02d.%02d.%02d",
            app_fsm->tx_day,
            app_fsm->tx_month,
            app_fsm->tx_year);
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, buffer);
        canvas_set_font(canvas, FontPrimary);
        snprintf(
            buffer,
            sizeof(buffer),
            "%02d:%02d:%02d",
            app_fsm->tx_hour,
            app_fsm->tx_minute,
            app_fsm->bit_number);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, buffer);
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            buffer,
            sizeof(buffer),
            "sec %02u pulse %s",
            app_fsm->bit_number,
            dcf77_logic_get_pulse_label(app_fsm->current_pulse));
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, buffer);
        dcf77_tx_render_progress_bar(canvas, app_fsm);
        return;
    }

    const uint8_t bit_number = app_fsm->bit_number;
    const uint8_t bit_value = app_fsm->bit_value;
    uint8_t underline_x = (bit_number / 8) * 12 + 16;
    const uint8_t byte = app_fsm->dcf77_message[bit_number / 8];
    char display_bits[9] = "00000000";

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d %02d.%02d.%02d",
        app_fsm->tx_hour,
        app_fsm->tx_minute,
        app_fsm->tx_day,
        app_fsm->tx_month,
        app_fsm->tx_year);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, buffer);

    if(app_fsm->debug_flag) {
        canvas_draw_str_aligned(canvas, 8, 10, AlignCenter, AlignBottom, "D");
    }

    canvas_set_font(canvas, FontSecondary);
    snprintf(buffer, sizeof(buffer), "%1x.%1x=%01x", bit_number / 8, bit_number % 8, bit_value);
    canvas_draw_str_aligned(canvas, 64, 21, AlignCenter, AlignBottom, buffer);

    snprintf(buffer, sizeof(buffer), "%s (bit %d)", dcf77_bitnames[bit_number], bit_value);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, buffer);

    canvas_set_font(canvas, FontKeyboard);
    snprintf(
        buffer,
        sizeof(buffer),
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        app_fsm->dcf77_message[0],
        app_fsm->dcf77_message[1],
        app_fsm->dcf77_message[2],
        app_fsm->dcf77_message[3],
        app_fsm->dcf77_message[4],
        app_fsm->dcf77_message[5],
        app_fsm->dcf77_message[6],
        app_fsm->dcf77_message[7]);
    canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, buffer);

    canvas_draw_line(canvas, underline_x, 42, underline_x + 10, 42);
    for(int i = 0; i < 8; i++) {
        if(byte & (1 << (7 - i))) {
            display_bits[i] = '1';
        }
    }

    snprintf(buffer, sizeof(buffer), "%02x=%s", byte, display_bits);
    canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, buffer);
    underline_x = (bit_number % 8) * 6 + 49;
    canvas_draw_line(canvas, underline_x, 52, underline_x + 4, 52);
    dcf77_tx_render_progress_bar(canvas, app_fsm);
}

void dcf77_gui_init(AppFSM* app_fsm) {
    app_fsm->startup_widget = widget_alloc();
    widget_add_string_multiline_element(
        app_fsm->startup_widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        "DCF77 TX\n\ngithub.com/arha/\nflipper-dcf77");
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewStartup, widget_get_view(app_fsm->startup_widget));

    app_fsm->submenu = submenu_alloc();
    submenu_add_item(app_fsm->submenu, "Start", Dcf77MenuItemStart, dcf77_menu_callback, app_fsm);
    submenu_add_item(
        app_fsm->submenu, "Signal & LF", Dcf77MenuItemLfSettings, dcf77_menu_callback, app_fsm);
    submenu_add_item(
        app_fsm->submenu, "Advanced", Dcf77MenuItemAdvanced, dcf77_menu_callback, app_fsm);
    submenu_add_item(app_fsm->submenu, "About", Dcf77MenuItemAbout, dcf77_menu_callback, app_fsm);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewMenu, submenu_get_view(app_fsm->submenu));

    app_fsm->advanced_menu = submenu_alloc();
    submenu_add_item(
        app_fsm->advanced_menu,
        "SubGHz",
        Dcf77AdvancedMenuItemSubGhzSettings,
        dcf77_advanced_menu_callback,
        app_fsm);
    submenu_add_item(
        app_fsm->advanced_menu,
        "Debug",
        Dcf77AdvancedMenuItemDebugSettings,
        dcf77_advanced_menu_callback,
        app_fsm);
    submenu_add_item(
        app_fsm->advanced_menu,
        "Time warp",
        Dcf77AdvancedMenuItemExperimentalTimeSettings,
        dcf77_advanced_menu_callback,
        app_fsm);
    submenu_add_item(
        app_fsm->advanced_menu,
        "Tx Ratio",
        Dcf77AdvancedMenuItemTxRatioSettings,
        dcf77_advanced_menu_callback,
        app_fsm);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewAdvancedMenu, submenu_get_view(app_fsm->advanced_menu));

    app_fsm->lf_settings = variable_item_list_alloc();
    app_fsm->lf_signal_item = variable_item_list_add(
        app_fsm->lf_settings, "Signal", radio_clock_visible_signal_count(), NULL, app_fsm);
    variable_item_set_current_value_index(
        app_fsm->lf_signal_item, radio_clock_visible_signal_index(app_fsm->current_signal));
    variable_item_set_current_value_text(
        app_fsm->lf_signal_item, radio_clock_signal_get_label(app_fsm->current_signal));

    app_fsm->lf_tx_enabled_item = variable_item_list_add(
        app_fsm->lf_settings, "LF TX enabled", 2, dcf77_lf_transmit_change_callback, app_fsm);
    variable_item_set_current_value_index(
        app_fsm->lf_tx_enabled_item, app_fsm->lf_transmit_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        app_fsm->lf_tx_enabled_item, app_fsm->lf_transmit_enabled ? "Yes" : "No");

    app_fsm->lf_freq_item = variable_item_list_add(
        app_fsm->lf_settings,
        "kHz",
        DCF77_LF_FREQ_ITEM_VALUES,
        dcf77_lf_frequency_change_callback,
        app_fsm);
    variable_item_set_current_value_index(
        app_fsm->lf_freq_item, dcf77_app_get_lf_freq_index(app_fsm->lf_freq));
    variable_item_set_current_value_text(app_fsm->lf_freq_item, app_fsm->lf_freq_text);

    app_fsm->lf_default_freq_item =
        variable_item_list_add(app_fsm->lf_settings, "Default frequency", 1, NULL, app_fsm);
    variable_item_set_current_value_text(app_fsm->lf_default_freq_item, "");

    variable_item_list_set_enter_callback(
        app_fsm->lf_settings, dcf77_lf_settings_enter_callback, app_fsm);
    view_set_context(variable_item_list_get_view(app_fsm->lf_settings), app_fsm);
    view_set_input_callback(
        variable_item_list_get_view(app_fsm->lf_settings), dcf77_lf_settings_input_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewLfSettings,
        variable_item_list_get_view(app_fsm->lf_settings));

    app_fsm->tx_ratio_settings = variable_item_list_alloc();
    app_fsm->lf_tx_ratio_x_item =
        variable_item_list_add(app_fsm->tx_ratio_settings, "Send x times", 1, NULL, app_fsm);
    variable_item_set_current_value_text(app_fsm->lf_tx_ratio_x_item, app_fsm->tx_ratio_x_text);
    app_fsm->lf_tx_ratio_y_item = variable_item_list_add(
        app_fsm->tx_ratio_settings, "Out of", (uint8_t)dcf77_tx_ratio_y_count(), NULL, app_fsm);
    variable_item_set_current_value_index(app_fsm->lf_tx_ratio_y_item, app_fsm->tx_ratio_y_index);
    variable_item_set_current_value_text(app_fsm->lf_tx_ratio_y_item, app_fsm->tx_ratio_y_text);
    view_set_context(variable_item_list_get_view(app_fsm->tx_ratio_settings), app_fsm);
    view_set_input_callback(
        variable_item_list_get_view(app_fsm->tx_ratio_settings),
        dcf77_tx_ratio_settings_input_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewTxRatioSettings,
        variable_item_list_get_view(app_fsm->tx_ratio_settings));

    app_fsm->experimental_time_settings_view = variable_item_list_alloc();
    app_fsm->experimental_time_enabled_item = variable_item_list_add(
        app_fsm->experimental_time_settings_view, "Enabled", 2, NULL, app_fsm);
    app_fsm->experimental_time_source_item = variable_item_list_add(
        app_fsm->experimental_time_settings_view,
        "Time source",
        Dcf77ExperimentalTimeSourceCount,
        NULL,
        app_fsm);
    app_fsm->experimental_preset_item = variable_item_list_add(
        app_fsm->experimental_time_settings_view, "Preset time", 1, NULL, app_fsm);
    app_fsm->experimental_direction_item = variable_item_list_add(
        app_fsm->experimental_time_settings_view, "Time direction", 3, NULL, app_fsm);
    app_fsm->experimental_speed_item = variable_item_list_add(
        app_fsm->experimental_time_settings_view, "Speed 1 min =", 15, NULL, app_fsm);
    variable_item_list_set_enter_callback(
        app_fsm->experimental_time_settings_view,
        dcf77_experimental_time_settings_enter_callback,
        app_fsm);
    view_set_context(
        variable_item_list_get_view(app_fsm->experimental_time_settings_view), app_fsm);
    view_set_input_callback(
        variable_item_list_get_view(app_fsm->experimental_time_settings_view),
        dcf77_experimental_time_settings_input_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewExperimentalTimeSettings,
        variable_item_list_get_view(app_fsm->experimental_time_settings_view));

    app_fsm->subghz_settings = variable_item_list_alloc();
    app_fsm->subghz_tx_item =
        variable_item_list_add(app_fsm->subghz_settings, "SubGHz TX", 4, NULL, app_fsm);
    app_fsm->subghz_tone_item = variable_item_list_add(
        app_fsm->subghz_settings, "FSK tone", (uint8_t)dcf77_subghz_note_count(), NULL, app_fsm);
    app_fsm->subghz_band_item = variable_item_list_add(
        app_fsm->subghz_settings, "Band", app_fsm->subghz_band_count, NULL, app_fsm);
    app_fsm->subghz_frequency_item =
        variable_item_list_add(app_fsm->subghz_settings, "Frequency", 1, NULL, app_fsm);
    app_fsm->subghz_manual_item =
        variable_item_list_add(app_fsm->subghz_settings, "KHz", 1, NULL, app_fsm);
    app_fsm->subghz_timeout_item = variable_item_list_add(
        app_fsm->subghz_settings,
        "TX Timeout",
        (uint8_t)dcf77_subghz_tx_timeout_count(),
        NULL,
        app_fsm);
    variable_item_list_set_enter_callback(
        app_fsm->subghz_settings, dcf77_subghz_settings_enter_callback, app_fsm);
    view_set_context(variable_item_list_get_view(app_fsm->subghz_settings), app_fsm);
    view_set_input_callback(
        variable_item_list_get_view(app_fsm->subghz_settings),
        dcf77_subghz_settings_input_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewSubGhzSettings,
        variable_item_list_get_view(app_fsm->subghz_settings));

    app_fsm->subghz_freq_input = number_input_alloc();
    number_input_set_header_text(app_fsm->subghz_freq_input, "khz");
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewSubGhzFreqInput,
        number_input_get_view(app_fsm->subghz_freq_input));

    app_fsm->preset_time_input = dcf77_experimental_time_input_alloc();
    dcf77_experimental_time_input_set_previous_callback(
        app_fsm->preset_time_input, dcf77_preset_time_input_previous_callback, app_fsm);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewPresetTimeInput,
        dcf77_experimental_time_input_get_view(app_fsm->preset_time_input));

    app_fsm->debug_settings = variable_item_list_alloc();
    app_fsm->debug_gpio_baseband_item = variable_item_list_add(
        app_fsm->debug_settings,
        "GPIO baseband",
        (uint8_t)dcf77_debug_gpio_baseband_pin_count(),
        NULL,
        app_fsm);
    app_fsm->debug_gpio_rf_item = variable_item_list_add(
        app_fsm->debug_settings,
        "GPIO RF",
        (uint8_t)dcf77_debug_gpio_rf_pin_count(),
        NULL,
        app_fsm);
    app_fsm->debug_gpio_duty_item = variable_item_list_add(
        app_fsm->debug_settings,
        "GPIO duty cycle",
        (uint8_t)dcf77_debug_gpio_rf_duty_count(),
        NULL,
        app_fsm);
    app_fsm->debug_led_item = variable_item_list_add(
        app_fsm->debug_settings, "LED", (uint8_t)dcf77_debug_led_color_count(), NULL, app_fsm);
    app_fsm->debug_screen_item = variable_item_list_add(
        app_fsm->debug_settings, "Screen", (uint8_t)dcf77_debug_screen_mode_count(), NULL, app_fsm);
    app_fsm->debug_speaker_item = variable_item_list_add(
        app_fsm->debug_settings,
        "Speaker",
        (uint8_t)(dcf77_subghz_note_count() + 1U),
        NULL,
        app_fsm);
    view_set_context(variable_item_list_get_view(app_fsm->debug_settings), app_fsm);
    view_set_input_callback(
        variable_item_list_get_view(app_fsm->debug_settings), dcf77_debug_settings_input_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewDebugSettings,
        variable_item_list_get_view(app_fsm->debug_settings));

    app_fsm->gpio_rf_warning_view = view_alloc();
    view_allocate_model(
        app_fsm->gpio_rf_warning_view, ViewModelTypeLockFree, sizeof(Dcf77AppViewModel));
    with_view_model(
        app_fsm->gpio_rf_warning_view,
        Dcf77AppViewModel * warning_model,
        { warning_model->app_fsm = app_fsm; },
        false);
    view_set_context(app_fsm->gpio_rf_warning_view, app_fsm);
    view_set_draw_callback(app_fsm->gpio_rf_warning_view, dcf77_gpio_rf_warning_render_callback);
    view_set_input_callback(app_fsm->gpio_rf_warning_view, dcf77_gpio_rf_warning_input_callback);
    view_set_previous_callback(
        app_fsm->gpio_rf_warning_view, dcf77_gpio_rf_warning_previous_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewGpioRfWarning, app_fsm->gpio_rf_warning_view);

    app_fsm->tx_view = view_alloc();
    view_allocate_model(app_fsm->tx_view, ViewModelTypeLockFree, sizeof(Dcf77AppViewModel));
    with_view_model(
        app_fsm->tx_view, Dcf77AppViewModel * tx_model, { tx_model->app_fsm = app_fsm; }, false);
    view_set_context(app_fsm->tx_view, app_fsm);
    view_set_draw_callback(app_fsm->tx_view, dcf77_tx_render_callback);
    view_set_input_callback(app_fsm->tx_view, dcf77_tx_input_callback);
    view_set_previous_callback(app_fsm->tx_view, dcf77_tx_previous_callback);
    view_dispatcher_add_view(app_fsm->view_dispatcher, Dcf77ViewTx, app_fsm->tx_view);

    app_fsm->about_view = view_alloc();
    view_allocate_model(app_fsm->about_view, ViewModelTypeLockFree, sizeof(Dcf77AboutViewModel));
    with_view_model(
        app_fsm->about_view,
        Dcf77AboutViewModel * about_model,
        {
            about_model->scroll_pos = 0U;
            about_model->scroll_pos_total = 1U;
        },
        false);
    view_set_context(app_fsm->about_view, app_fsm);
    view_set_draw_callback(app_fsm->about_view, dcf77_about_render_callback);
    view_set_input_callback(app_fsm->about_view, dcf77_about_input_callback);
    view_set_enter_callback(app_fsm->about_view, dcf77_about_enter_callback);
    view_set_previous_callback(app_fsm->about_view, dcf77_text_previous_callback);
    view_dispatcher_add_view(app_fsm->view_dispatcher, Dcf77ViewAbout, app_fsm->about_view);

    app_fsm->egg_widget = widget_alloc();
    widget_add_frame_element(app_fsm->egg_widget, 0, 0, 128, 64, 0);
    widget_add_text_scroll_element(app_fsm->egg_widget, 4, 4, 120, 46, egg_text);
    widget_add_button_element(
        app_fsm->egg_widget, GuiButtonTypeLeft, "Back", dcf77_egg_button_callback, app_fsm);
    view_set_context(widget_get_view(app_fsm->egg_widget), app_fsm);
    view_set_input_callback(widget_get_view(app_fsm->egg_widget), dcf77_egg_input_callback);
    view_set_previous_callback(widget_get_view(app_fsm->egg_widget), dcf77_text_previous_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewEgg, widget_get_view(app_fsm->egg_widget));
}

void dcf77_gui_deinit(AppFSM* app_fsm) {
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewEgg);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewAbout);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewGpioRfWarning);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewDebugSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewSubGhzFreqInput);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewSubGhzSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewPresetTimeInput);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewExperimentalTimeSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewTxRatioSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewLfSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewAdvancedMenu);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewTx);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewMenu);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewStartup);
    widget_free(app_fsm->egg_widget);
    view_free(app_fsm->about_view);
    view_free(app_fsm->gpio_rf_warning_view);
    view_free(app_fsm->tx_view);
    dcf77_experimental_time_input_free(app_fsm->preset_time_input);
    number_input_free(app_fsm->subghz_freq_input);
    variable_item_list_free(app_fsm->debug_settings);
    variable_item_list_free(app_fsm->subghz_settings);
    variable_item_list_free(app_fsm->experimental_time_settings_view);
    variable_item_list_free(app_fsm->tx_ratio_settings);
    variable_item_list_free(app_fsm->lf_settings);
    submenu_free(app_fsm->advanced_menu);
    submenu_free(app_fsm->submenu);
    widget_free(app_fsm->startup_widget);
    view_dispatcher_free(app_fsm->view_dispatcher);
}
