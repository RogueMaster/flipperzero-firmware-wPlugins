#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include "gui/view.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>

#include "settings.h"
#include "common.h"

VariableItemList* settings_list;

void settings_ui_clear() {
    if(settings_list) {
        variable_item_list_reset(settings_list);
        variable_item_list_free(settings_list);
        settings_list = NULL;
    }
}

/*--------------------------------------------------------------------------*/
/*  hard limits                                                             */
/*--------------------------------------------------------------------------*/
#define GRAVITY_MIN 1.0
#define GRAVITY_MAX 100.0

#define PLANETS_MIN 1
#define PLANETS_MAX 8

#define LAUNCH_MIN 0.10
#define LAUNCH_MAX 5.00

#define GRID_SPEED_MIN 1.0
#define GRID_SPEED_MAX 32.0

#define GRID_SPACING_MIN 1
#define GRID_SPACING_MAX 32

#define TRAIL_MIN 0
#define TRAIL_MAX 5

#define ASTEROID_SPEED_MIN 1
#define ASTEROID_SPEED_MAX 5

#define DEBRI_MIN 0
#define DEBRI_MAX 50

/*--------------------------------------------------------------------------*/
/*  one-line helpers                                                        */
/*--------------------------------------------------------------------------*/
/*
 * Generic helper macro for creating value–callback functions.
 * The generated callback performs a *smooth* linear mapping from the current
 * value index (0..steps-1) to the desired numeric range [minv, maxv].
 *
 *  steps – the number of discrete values available for the item (same value
 *           that is passed to `variable_item_list_add`).
 */

/* Callback that maps each index to exactly one unit increase (value = minv + index).
   Suitable for integer / whole-unit ranges. Steps must equal (maxv - minv + 1). */
#define MAKE_CB_UNIT(name, fld, minv, maxv, fmt, cast)        \
    static void name(VariableItem* i) {                       \
        int index = variable_item_get_current_value_index(i); \
        if(index < 0) index = 0;                              \
        cast value = (cast)(minv) + (cast)index;              \
        if(value <= (cast)(minv)) value = (cast)(minv);       \
        if(value >= (cast)(maxv)) value = (cast)(maxv);       \
        settings.fld = value;                                 \
        settings_save();                                      \
        char buf[12];                                         \
        snprintf(buf, sizeof(buf), fmt, settings.fld);        \
        variable_item_set_current_value_text(i, buf);         \
    }

#define MAKE_CB(name, fld, minv, maxv, fmt, cast, steps)                   \
    static void name(VariableItem* i) {                                    \
        /* Index is in the range 0 .. (steps-1) */                         \
        int index = variable_item_get_current_value_index(i);              \
        /* Protect against divide-by-zero and out-of-range indices */      \
        if(index < (double)0) index = (double)0;                           \
        if(index > (steps - (double)1)) index = (steps - (double)1);       \
                                                                           \
        /* Map index to numeric value */                                   \
        double ratio = (double)index / (double)((steps) - (double)1);      \
        cast value = (cast)(minv) + ratio * ((cast)(maxv) - (cast)(minv)); \
                                                                           \
        /* Clamp just in case */                                           \
        if(value <= (cast)(minv)) value = (cast)(minv);                    \
        if(value >= (cast)(maxv)) value = (cast)(maxv);                    \
                                                                           \
        settings.fld = value;                                              \
        settings_save();                                                   \
                                                                           \
        char buf[12];                                                      \
        snprintf(buf, sizeof(buf), fmt, settings.fld);                     \
        variable_item_set_current_value_text(i, buf);                      \
    }

/* call-backs (value_count matches the 3rd param of variable_item_list_add) */
MAKE_CB_UNIT(gravity_cb, gravity_force, GRAVITY_MIN, GRAVITY_MAX, "%.0f", double)
MAKE_CB_UNIT(planets_cb, num_planets, PLANETS_MIN, PLANETS_MAX, "%u", size_t)
MAKE_CB(
    launch_cb,
    launch_interval,
    LAUNCH_MIN,
    LAUNCH_MAX,
    "%.2f",
    double,
    181) /* keep previous coarse mapping */
MAKE_CB_UNIT(grid_speed_cb, grid_speed, GRID_SPEED_MIN, GRID_SPEED_MAX, "%.0f", double)
MAKE_CB_UNIT(grid_space_cb, grid_spacing, GRID_SPACING_MIN, GRID_SPACING_MAX, "%u", size_t)
MAKE_CB_UNIT(trail_cb, trail_duration, TRAIL_MIN, TRAIL_MAX, "%u", size_t)
MAKE_CB_UNIT(asteroid_cb, asteroid_speed, ASTEROID_SPEED_MIN, ASTEROID_SPEED_MAX, "%u", size_t)
MAKE_CB_UNIT(debri_cb, debri_count, DEBRI_MIN, DEBRI_MAX, "%u", size_t)

void create_items() {
    variable_item_list_reset(settings_list);

    VariableItem* it;

    /* Helper macro to calculate and set the initial index so that the
       slider/selector starts at the *actual* saved value rather than at either
       extreme. */
#define INIT_INDEX(it, val, minv, maxv)                           \
    do {                                                          \
        int clamped_val = (int)(val);                             \
        if(clamped_val <= (int)(minv)) clamped_val = (int)(minv); \
        if(clamped_val >= (int)(maxv)) clamped_val = (int)(maxv); \
        double num = (double)(clamped_val - (double)(minv));      \
        double denom = (double)((maxv) - (minv));                 \
        double ratio = num / denom;                               \
        int idx = (ratio) * (double)(maxv - minv + 1);            \
        variable_item_set_current_value_index(it, idx);           \
    } while(0)

    it = variable_item_list_add(
        settings_list, "Gravity", (GRAVITY_MAX - GRAVITY_MIN + 1), gravity_cb, NULL);
    INIT_INDEX(it, settings.gravity_force, GRAVITY_MIN, GRAVITY_MAX);
    gravity_cb(it);

    it = variable_item_list_add(
        settings_list, "Planets", (PLANETS_MAX - PLANETS_MIN + 1), planets_cb, NULL);
    INIT_INDEX(it, settings.num_planets, PLANETS_MIN, PLANETS_MAX);
    planets_cb(it);

    it = variable_item_list_add(settings_list, "Asteroid Interval", 40, launch_cb, NULL);
    INIT_INDEX(it, settings.launch_interval, LAUNCH_MIN, LAUNCH_MAX);
    launch_cb(it);

    it = variable_item_list_add(
        settings_list, "Grid speed", (GRID_SPEED_MAX - GRID_SPEED_MIN + 1), grid_speed_cb, NULL);
    INIT_INDEX(it, settings.grid_speed, GRID_SPEED_MIN, GRID_SPEED_MAX);
    grid_speed_cb(it);

    it = variable_item_list_add(
        settings_list,
        "Grid spacing",
        (GRID_SPACING_MAX - GRID_SPACING_MIN + 1),
        grid_space_cb,
        NULL);
    INIT_INDEX(it, settings.grid_spacing, GRID_SPACING_MIN, GRID_SPACING_MAX);
    grid_space_cb(it);

    it = variable_item_list_add(
        settings_list, "Trail duration", (TRAIL_MAX - TRAIL_MIN + 1), trail_cb, NULL);
    INIT_INDEX(it, settings.trail_duration, TRAIL_MIN, TRAIL_MAX);
    trail_cb(it);

    it = variable_item_list_add(
        settings_list,
        "Asteroid speed",
        (ASTEROID_SPEED_MAX - ASTEROID_SPEED_MIN + 1),
        asteroid_cb,
        NULL);
    INIT_INDEX(it, settings.asteroid_speed, ASTEROID_SPEED_MIN, ASTEROID_SPEED_MAX);
    asteroid_cb(it);

    it = variable_item_list_add(
        settings_list, "Debri count", (DEBRI_MAX - DEBRI_MIN + 1), debri_cb, NULL);
    INIT_INDEX(it, settings.debri_count, DEBRI_MIN, DEBRI_MAX);
    debri_cb(it);
#undef INIT_INDEX
}

/*--------------------------------------------------------------------------*/
void settings_ui_init(void) {
    settings_ui_clear();
    settings_list = variable_item_list_alloc();

    create_items();
}

void settings_ui_open(ViewDispatcher* view_dispatcher) {
    view_dispatcher_switch_to_view(view_dispatcher, SETTINGS_VIEW_ID);
}

uint32_t settings_ui_previous_callback(void* ctx) {
    UNUSED(ctx);
    view_dispatcher_switch_to_view(view_dispatcher, MAIN_VIEW_ID);

    return 0;
}

#endif
