/**
 * @file ui.h
 * @brief Views, navigation, draw callbacks, and config menu handlers.
 */
#pragma once

#include <gui/canvas.h>
#include <gui/view.h>
#include <input/input.h>
#include <gui/modules/variable_item_list.h>
#include "src/fred_fm/include/types.h"

/** Back navigation from submenu; returns ViewNone to exit app. */
uint32_t fred_fm_navigation_exit_callback(void* context);

/** Back navigation from nested views to submenu. */
uint32_t fred_fm_navigation_submenu_callback(void* context);

/** Handle submenu item selection and view switch. */
void fred_fm_submenu_callback(void* context, uint32_t index);

/** Rebuild submenu entries (e.g. after RDS toggle). */
bool fred_fm_submenu_rebuild(FredFm* app);

/** Request redraw of listen view. */
void fred_fm_redraw_listen_view(View* view);

/** Request redraw of constellation view. */
void fred_fm_redraw_constellation_view(View* view);

/** Periodic UI tick: signal meter, RDS text, settings save. */
void fred_fm_tick_callback(void* context);

/** Input handler for listen view (tune, seek, presets). */
bool fred_fm_view_input_callback(InputEvent* event, void* context);

/** Input handler for constellation view (offset adjust, capture). */
bool fred_fm_constellation_view_input_callback(InputEvent* event, void* context);

/** Draw RDS constellation scatter plot. */
void fred_fm_constellation_view_draw_callback(Canvas* canvas, void* model);

/** Draw main listen screen (freq, signal, PS). */
void fred_fm_view_draw_callback(Canvas* canvas, void* model);

/** Config menu: volume level change. */
void fred_fm_volume_change(VariableItem* item);

/** Config menu: PT2257/PT2259 chip selection. */
void fred_fm_pt_chip_change(VariableItem* item);

/** Config menu: TEA5767 SNC toggle. */
void fred_fm_snc_change(VariableItem* item);

/** Config menu: de-emphasis 50/75 µs. */
void fred_fm_deemph_change(VariableItem* item);

/** Config menu: high-cut toggle. */
void fred_fm_highcut_change(VariableItem* item);

/** Config menu: force mono toggle. */
void fred_fm_mono_change(VariableItem* item);

/** Config menu: keep backlight on. */
void fred_fm_backlight_change(VariableItem* item);

/** Config menu: amplifier power. */
void fred_fm_amp_power_change(VariableItem* item);

/** Config menu: Class-D vs Class-AB mode. */
void fred_fm_amp_mode_change(VariableItem* item);

/** Draw labeled button hint top-left. */
void elements_button_top_left(Canvas* canvas, const char* str);

/** Draw labeled button hint top-right. */
void elements_button_top_right(Canvas* canvas, const char* str);
