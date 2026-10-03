/**
 * @file mntm.c
 * RM's MNTM main menu layout, moved into an ABI-1 loader plugin.
 * Original layout derived from Momentum Firmware (GPL-3.0); RM retains its
 * clock, battery, charging, OTG and midnight-format preferences.
 */
#include "menu_style_helpers.h"
#include <assets_icons.h>
#include <furi_hal.h>
#include <locale/locale.h>

static void menu_style_mntm_draw(Canvas* canvas, MenuModel* model) {
    size_t position = model->position;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 62, 4, &I_Release_arrow_18x15);
    canvas_draw_line(canvas, 5, 15, 59, 15);
    canvas_draw_line(canvas, 7, 17, 61, 17);
    canvas_draw_line(canvas, 10, 19, 63, 19);
    char title[20];
    snprintf(title, sizeof(title), "%s", furi_hal_version_get_name_ptr());
    canvas_draw_str(canvas, 5, 12, title);
    DateTime curr_dt;
    furi_hal_rtc_get_datetime(&curr_dt);
    uint8_t hour = curr_dt.hour;
    uint8_t min = curr_dt.minute;
    LocaleTimeFormat time_format = locale_get_time_format();
    if(time_format == LocaleTimeFormat12h) {
        if(hour > 12) {
            hour -= 12;
        }
        if(hour == 0) {
            hour = (cfw_settings.midnight_format_00 ? 0 : 12);
        }
    }
    canvas_set_font(canvas, FontSecondary);
    char clk[20];
    snprintf(clk, sizeof(clk), "%02u:%02u", hour, min);
    canvas_draw_str(canvas, 5, 34, clk);

    bool ext5v = furi_hal_power_is_otg_enabled();
    uint8_t battery_percent = furi_hal_power_get_pct();
    bool charge_state = false;

    // Determine charge state
    if(furi_hal_power_is_charging()) {
        if(battery_percent < 100 && !furi_hal_power_is_charging_done()) {
            charge_state = true;
        }
    }

    // Display battery percentage
    char bat_display[20];
    snprintf(bat_display, sizeof(bat_display), "%d%%", battery_percent);
    canvas_draw_str(canvas, 5, 45, bat_display);

    // Display charge state icon
    if(charge_state) {
        canvas_draw_icon(canvas, 28, 33, &I_Voltage_16x16);
    }

    // Display OTG state
    char ext5v_display[20];
    snprintf(ext5v_display, sizeof(ext5v_display), "5v: %s", ext5v ? "On" : "Off");
    canvas_draw_str(canvas, 5, 56, ext5v_display);

    const MenuItem* item = &model->items[position];
    elements_bold_rounded_frame(canvas, 42, 23, 35, 33);
    menu_style_icon_centered(canvas, item->icon, 43, 24, 35, 32);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    uint8_t startY = 15;
    uint8_t itemHeight = 10;
    uint8_t itemMaxVisible = 5;
    size_t endItem = position + itemMaxVisible;
    endItem = (endItem > model->count) ? model->count : endItem;

    for(size_t i = position; i < endItem; i++) {
        const MenuItem* item = &model->items[i];
        const char* name = menu_style_label(item, true);
        uint8_t yPos = startY + ((i - position) * itemHeight);
        size_t scroll_counter = menu_style_scroll(model, i == position);
        menu_style_text_line(canvas, 83, yPos, 43, name, scroll_counter, false, false);
    }
}

static size_t menu_style_mntm_navigate(MenuModel* model, InputKey key) {
    return menu_style_navigate_list(model, key);
}

static const MenuStylePlugin style = {
    .draw = menu_style_mntm_draw,
    .navigate = menu_style_mntm_navigate};
MENU_STYLE_PLUGIN(style, menu_style_mntm_ep)
