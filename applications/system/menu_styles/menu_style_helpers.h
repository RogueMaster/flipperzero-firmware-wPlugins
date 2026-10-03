/**
 * @file menu_style_helpers.h
 * Shared helpers for main menu style plugins.
 *
 * Most of the layouts in this directory are ported from Momentum Firmware's built-in menu
 * styles (https://github.com/Next-Flip/Momentum-Firmware, GPL-3.0), reworked to ship as
 * plugins. Each file names its author.
 */

#pragma once

#include <furi.h>
#include <string.h>
#include <stdio.h>
#include <cfw/cfw.h>
#include <gui/elements.h>
#include <gui/modules/menu.h>
#include <flipper_application/flipper_application.h>

#define MENU_STYLE_PLUGIN(menu_style, menu_style_ep)                         \
    static const FlipperAppPluginDescriptor menu_style_plugin_descriptor = { \
        .appid = MENU_STYLE_PLUGIN_APP_ID,                                   \
        .ep_api_version = MENU_STYLE_PLUGIN_API_VERSION,                     \
        .entry_point = &(menu_style),                                        \
    };                                                                       \
    const FlipperAppPluginDescriptor* menu_style_ep(void) {                  \
        return &menu_style_plugin_descriptor;                                \
    }

/** Item label, optionally shortened for a layout too narrow to scroll it comfortably.
 * Only the stock names that do not fit are special-cased; anything else is returned as-is. These
 * are the name= fields of applications/main/lfrfid and .../subghz - rename either and the
 * shortening silently stops matching.
 */
static inline const char* menu_style_label(const MenuItem* item, bool shorter) {
    const char* label = item->label;
    if(label[0] == '[') {
        const char* end = strstr(label + 1, "] ");
        if(end) label = end + 2;
    }
    if(shorter) {
        if(strcmp(label, "125 kHz RFID") == 0) return "RFID";
        if(strcmp(label, "Sub-GHz") == 0) return "SubGHz";
    }
    return label;
}

/** RM exposes FuriString-based scrollable text drawing. */
static inline void menu_style_text_line(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    const char* label,
    size_t scroll,
    bool ellipsis,
    bool centered) {
    FuriString* text = furi_string_alloc_set(label);
    elements_scrollable_text_line_centered(canvas, x, y, width, text, scroll, ellipsis, centered);
    furi_string_free(text);
}

/** Wrap an offset without unsigned underflow for a small menu. */
static inline size_t menu_style_position_offset(const MenuModel* model, int32_t offset) {
    if(offset < 0) {
        return (model->position + model->count - (size_t)(-offset) % model->count) % model->count;
    }
    return (model->position + (size_t)offset) % model->count;
}

static inline size_t menu_style_scroll(const MenuModel* model, bool selected) {
    return (selected && model->scroll_counter) ? model->scroll_counter - 1 : 0;
}

static inline void menu_style_icon_centered(
    Canvas* canvas,
    IconAnimation* icon,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {
    canvas_draw_icon_animation(
        canvas,
        x + (width - icon_animation_get_width(icon)) / 2,
        y + (height - icon_animation_get_height(icon)) / 2,
        icon);
}

static inline void menu_style_scrollbar_horizontal(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t pos,
    size_t total) {
    elements_scrollbar_horizontal(canvas, x, y, width, pos, total);
}

static inline size_t menu_style_navigate_wrap(const MenuModel* model, InputKey key) {
    if(key == InputKeyLeft) return model->position ? model->position - 1 : model->count - 1;
    if(key == InputKeyRight) return (model->position + 1) % model->count;
    return model->position;
}

static inline size_t menu_style_navigate_list(const MenuModel* model, InputKey key) {
    if(key == InputKeyUp) return model->position ? model->position - 1 : model->count - 1;
    if(key == InputKeyDown) return (model->position + 1) % model->count;
    return model->position;
}

/** Jump to the other column of a two-column page of `rows` items each. The right column of the
 * last page can be empty, and then there is nowhere to go sideways at all; otherwise clamp into
 * it. Pass the column height rather than the page size so an odd page cannot be expressed.
 */
static inline size_t menu_style_navigate_two_columns(const MenuModel* model, size_t rows) {
    size_t position = model->position;
    size_t right_column = position - (position % (rows * 2)) + rows;
    if(right_column >= model->count) return position;
    size_t target = position < right_column ? position + rows : position - rows;
    return MIN(target, model->count - 1);
}
