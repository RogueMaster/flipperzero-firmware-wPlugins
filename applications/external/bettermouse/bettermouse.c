#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>

#include "bm_mouse.h"
#include "bm_settings.h"

#include <stddef.h>

typedef enum {
    BmViewMenu,
    BmViewMouse,
    BmViewSettings,
    BmViewHelp,
} BmView;

typedef enum {
    BmMenuIndexMouse,
    BmMenuIndexSettings,
    BmMenuIndexHelp,
} BmMenuIndex;

typedef enum {
    BmCustomEventExitMouse = 100,
} BmCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* variable_item_list;
    Widget* widget;
    BmMouse* bm_mouse;

    /* The view dispatcher has no getter for this, so we track it ourselves. */
    uint32_t current_view;

    /* Scratch for formatting setting values; the list copies the string. */
    char value_text[16];

    BmSettings settings;
} BetterMouse;

/* ------------------------------------------------------------------ settings */

/* Every numeric setting is a range, so adding a knob is one table row and one
   callback line rather than a hand written list of presets. */
typedef struct {
    const char* label;
    const BmRange* range;
    const char* unit;
    size_t offset; /* into BmSettings */
    bool wide; /* field is uint16_t rather than uint8_t */
} BmTunable;

#define BM_TUNABLE(lbl, rng, un, field, is_wide) \
    {.label = (lbl),                             \
     .range = (rng),                             \
     .unit = (un),                               \
     .offset = offsetof(BmSettings, field),      \
     .wide = (is_wide)}

static const BmTunable bm_tunables[] = {
    BM_TUNABLE("Max speed", &bm_range_max_speed, "/s", max_speed, true),
    BM_TUNABLE("Pre-ramp", &bm_range_min_speed, "/s", min_speed, true),
    BM_TUNABLE("Accel ramp", &bm_range_accel, "ms", accel_ms, true),
    BM_TUNABLE("Scroll speed", &bm_range_scroll, "/s", scroll_speed, false),
    BM_TUNABLE("Report rate", &bm_range_tick, "Hz", tick_hz, true),
    BM_TUNABLE("Ice glide", &bm_range_ice_glide, "ms", ice_glide_ms, true),
};

static uint16_t bm_tunable_get(const BetterMouse* app, const BmTunable* t) {
    const uint8_t* base = (const uint8_t*)&app->settings;
    if(t->wide) {
        uint16_t v;
        memcpy(&v, base + t->offset, sizeof(v));
        return v;
    }
    return *(base + t->offset);
}

static void bm_tunable_set(BetterMouse* app, const BmTunable* t, uint16_t value) {
    uint8_t* base = (uint8_t*)&app->settings;
    if(t->wide) {
        memcpy(base + t->offset, &value, sizeof(value));
    } else {
        *(base + t->offset) = (uint8_t)value;
    }
}

static void bm_tunable_changed(VariableItem* item) {
    BetterMouse* app = variable_item_get_context(item);
    uint8_t list_index = variable_item_list_get_selected_item_index(app->variable_item_list);
    if(list_index >= COUNT_OF(bm_tunables)) return;

    const BmTunable* t = &bm_tunables[list_index];
    uint16_t value = bm_range_value(t->range, variable_item_get_current_value_index(item));
    bm_tunable_set(app, t, value);

    snprintf(app->value_text, sizeof(app->value_text), "%u%s", (unsigned)value, t->unit);
    variable_item_set_current_value_text(item, app->value_text);
}

static void bm_toggle_changed(VariableItem* item, uint8_t* field) {
    uint8_t index = variable_item_get_current_value_index(item);
    *field = index;
    variable_item_set_current_value_text(item, bm_toggle_names[index]);
}

static void bm_setting_ice(VariableItem* item) {
    BetterMouse* app = variable_item_get_context(item);
    bm_toggle_changed(item, &app->settings.ice);
}

static void bm_setting_invert_y(VariableItem* item) {
    BetterMouse* app = variable_item_get_context(item);
    bm_toggle_changed(item, &app->settings.invert_y);
}

static void bm_setting_invert_scroll(VariableItem* item) {
    BetterMouse* app = variable_item_get_context(item);
    bm_toggle_changed(item, &app->settings.invert_scroll);
}

static void bm_setting_haptic(VariableItem* item) {
    BetterMouse* app = variable_item_get_context(item);
    bm_toggle_changed(item, &app->settings.haptic);
}

static void bm_add_toggle(
    BetterMouse* app,
    const char* label,
    uint8_t value,
    VariableItemChangeCallback callback) {
    VariableItem* item = variable_item_list_add(app->variable_item_list, label, 2, callback, app);
    variable_item_set_current_value_index(item, value);
    variable_item_set_current_value_text(item, bm_toggle_names[value]);
}

static void bm_settings_build(BetterMouse* app) {
    for(size_t i = 0; i < COUNT_OF(bm_tunables); i++) {
        const BmTunable* t = &bm_tunables[i];
        uint16_t value = bm_tunable_get(app, t);

        VariableItem* item = variable_item_list_add(
            app->variable_item_list, t->label, t->range->count, bm_tunable_changed, app);
        variable_item_set_current_value_index(item, bm_range_index(t->range, value));

        snprintf(app->value_text, sizeof(app->value_text), "%u%s", (unsigned)value, t->unit);
        variable_item_set_current_value_text(item, app->value_text);
    }

    bm_add_toggle(app, "Ice mode", app->settings.ice, bm_setting_ice);
    bm_add_toggle(app, "Invert Y", app->settings.invert_y, bm_setting_invert_y);
    bm_add_toggle(app, "Invert scroll", app->settings.invert_scroll, bm_setting_invert_scroll);
    bm_add_toggle(app, "Click vibro", app->settings.haptic, bm_setting_haptic);
}

/* --------------------------------------------------------------- navigation */

static void bm_switch_to(BetterMouse* app, uint32_t view_id) {
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

static void bm_submenu_callback(void* context, uint32_t index) {
    BetterMouse* app = context;

    if(index == BmMenuIndexMouse) {
        bm_switch_to(app, BmViewMouse);
    } else if(index == BmMenuIndexSettings) {
        bm_switch_to(app, BmViewSettings);
    } else if(index == BmMenuIndexHelp) {
        bm_switch_to(app, BmViewHelp);
    }
}

static bool bm_custom_event_callback(void* context, uint32_t event) {
    BetterMouse* app = context;

    if(event == BmCustomEventExitMouse) {
        bm_switch_to(app, BmViewMenu);
        return true;
    }
    return false;
}

static bool bm_navigation_callback(void* context) {
    BetterMouse* app = context;

    /* Back inside the mouse view is consumed by the view itself, so anything
       reaching here came from the menu, settings or help screen. */
    if(app->current_view == BmViewMenu) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(app->current_view == BmViewSettings) bm_settings_save(&app->settings);

    bm_switch_to(app, BmViewMenu);
    return true;
}

/* The motion thread asks to leave via a custom event, so the actual view switch
   happens on the view dispatcher thread. */
static void bm_mouse_exit_request(void* context) {
    BetterMouse* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BmCustomEventExitMouse);
}

/* --------------------------------------------------------------------- app */

static BetterMouse* bettermouse_alloc(void) {
    BetterMouse* app = malloc(sizeof(BetterMouse));

    bm_settings_load(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, bm_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, bm_navigation_callback);

    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Mouse", BmMenuIndexMouse, bm_submenu_callback, app);
    submenu_add_item(app->submenu, "Settings", BmMenuIndexSettings, bm_submenu_callback, app);
    submenu_add_item(app->submenu, "Controls", BmMenuIndexHelp, bm_submenu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, BmViewMenu, submenu_get_view(app->submenu));

    app->bm_mouse = bm_mouse_alloc(&app->settings, app->notifications);
    bm_mouse_set_exit_callback(app->bm_mouse, bm_mouse_exit_request, app);
    view_dispatcher_add_view(app->view_dispatcher, BmViewMouse, bm_mouse_get_view(app->bm_mouse));

    app->variable_item_list = variable_item_list_alloc();
    bm_settings_build(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        BmViewSettings,
        variable_item_list_get_view(app->variable_item_list));

    app->widget = widget_alloc();
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "\e#Controls\n"
        "Arrows: move\n"
        "OK: left click\n"
        "OK hold: drag lock,\n"
        "  press OK to drop\n"
        "Back tap: right click\n"
        "\n\e#Scroll\n"
        "Back hold + OK\n"
        "toggles the wheel.\n"
        "\n\e#Menu / exit\n"
        "App opens straight\n"
        "into the mouse.\n"
        "Hold Back 0.8s for\n"
        "this menu, shows\n"
        "MENU as a warning\n"
        "first. Back again\n"
        "leaves the app.\n"
        "\n\e#Ice mode\n"
        "Pointer keeps its\n"
        "momentum and coasts\n"
        "to a stop. Ice glide\n"
        "sets how long.\n");
    view_dispatcher_add_view(app->view_dispatcher, BmViewHelp, widget_get_view(app->widget));

    return app;
}

static void bettermouse_free(BetterMouse* app) {
    view_dispatcher_remove_view(app->view_dispatcher, BmViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BmViewMouse);
    view_dispatcher_remove_view(app->view_dispatcher, BmViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, BmViewHelp);

    submenu_free(app->submenu);
    bm_mouse_free(app->bm_mouse);
    variable_item_list_free(app->variable_item_list);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t bettermouse_app(void* p) {
    UNUSED(p);

    BetterMouse* app = bettermouse_alloc();

    /* Claim the USB port as a HID device, restoring whatever was there before
       when we are done. */
    FuriHalUsbInterface* usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL) == true);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    /* Straight into the mouse, ready to go. Holding Back drops to the menu,
       and Back from the menu leaves the app. */
    bm_switch_to(app, BmViewMouse);
    view_dispatcher_run(app->view_dispatcher);

    bm_settings_save(&app->settings);

    furi_hal_usb_set_config(usb_mode_prev, NULL);

    bettermouse_free(app);

    return 0;
}
