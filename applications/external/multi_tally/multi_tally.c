#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define TAG "MultiTally"

#define GROUP_COUNT 6
#define NAME_LEN 12
#define VALUE_MAX 999999
#define FLASH_MS 150
#define AUTOSAVE_MS 10000

#define SAVE_PATH APP_DATA_PATH("multi_tally.save")

typedef enum {
    ViewIdMain,
    ViewIdMenu,
    ViewIdGroupMenu,
    ViewIdGroupCfg,
    ViewIdSettings,
    ViewIdTextInput,
    ViewIdResetDialog,
    ViewIdHelp,
} ViewId;

typedef enum {
    MenuIndexHelp,
    MenuIndexGroups,
    MenuIndexSettings,
    MenuIndexResetAll,
    MenuIndexFactoryReset,
} MenuIndex;

// Group order matches key_to_group(): Up, Down, Left, Right, OK, Back
typedef struct {
    char name[NAME_LEN];
    int32_t value;
    int32_t step;
    bool enabled;
} Group;

typedef struct App App;

typedef struct {
    App* app;
} MainModel;

struct App {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    Submenu* menu;
    Submenu* group_menu;
    VariableItemList* group_cfg;
    VariableItemList* settings;
    TextInput* text_input;
    DialogEx* reset_dialog;
    Widget* help;
    FuriTimer* flash_timer;

    Group groups[GROUP_COUNT];
    bool vibro;
    bool sound;
    bool led;
    bool backlight;
    uint8_t hold_idx; // index into hold_repeats[]/hold_texts[]
    bool factory_pending; // which reset the confirm dialog is for
    bool dirty;

    uint8_t cfg_group; // group currently being configured
    char name_buf[NAME_LEN];
    VariableItem* item_name;
    VariableItem* item_value;

    int8_t flash_group; // -1 = none, else group briefly highlighted
    bool hint; // "Hold OK for menu" splash on startup
    FuriTimer* hint_timer;
    bool ok_hold, ok_consumed;
    uint8_t ok_repeats;
    bool back_hold, back_consumed;
    uint8_t back_repeats;
};

static const char* const default_names[GROUP_COUNT] =
    {"Up", "Down", "Left", "Right", "OK", "Back"};

// Text markers for the Groups list, same order as groups
static const char* const group_marks[GROUP_COUNT] = {"[^]", "[v]", "[<]", "[>]", "[o]", "[Bk]"};

static const int32_t step_values[] = {1, 2, 5, 10, 20, 50, 100};
static const char* const step_texts[] = {"1", "2", "5", "10", "20", "50", "100"};

// Repeat events tick every ~150 ms after the ~350 ms long-press threshold
static const uint8_t hold_repeats[] = {4, 8, 11, 17};
static const char* const hold_texts[] = {"1 s", "1.5 s", "2 s", "3 s"};
#define HOLD_IDX_DEFAULT 2 // 2 s

static const NotificationSequence seq_beep_plus = {
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_beep_minus = {
    &message_note_c4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const char* const help_text =
    "\e#Multi Tally\n"
    "Each button counts its\n"
    "own group: Up, Down,\n"
    "Left, Right, OK, Back.\n"
    "\n"
    "Short press: +step\n"
    "Long press: -step\n"
    "Hold OK: menu\n"
    "Hold Back: exit\n"
    "(hold time is set\n"
    "in Settings, 2 s\n"
    "by default)\n"
    "\n"
    "Screen cells:\n"
    " Left | Up  | Right\n"
    " OK  | Down | Back\n"
    "\n"
    "In Groups you can\n"
    "rename a group, set\n"
    "its step, turn it off\n"
    "and zero its count.\n"
    "\n"
    "Settings: vibro,\n"
    "sound and LED blink\n"
    "on each count, always-\n"
    "on backlight, hold\n"
    "time for menu/exit.\n"
    "\n"
    "Everything is saved\n"
    "automatically.";

static uint8_t step_index(int32_t step) {
    for(uint8_t i = 0; i < COUNT_OF(step_values); i++) {
        if(step_values[i] == step) return i;
    }
    return 0;
}

/* ---------------- persistence ---------------- */

static void state_save(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_always(ff, SAVE_PATH)) {
        int32_t values[GROUP_COUNT], steps[GROUP_COUNT];
        uint32_t enabled[GROUP_COUNT];
        flipper_format_write_header_cstr(ff, "MultiCounter", 1);
        for(uint8_t i = 0; i < GROUP_COUNT; i++) {
            char key[8];
            snprintf(key, sizeof(key), "Name%u", i);
            flipper_format_write_string_cstr(ff, key, app->groups[i].name);
            values[i] = app->groups[i].value;
            steps[i] = app->groups[i].step;
            enabled[i] = app->groups[i].enabled ? 1 : 0;
        }
        flipper_format_write_int32(ff, "Values", values, GROUP_COUNT);
        flipper_format_write_int32(ff, "Steps", steps, GROUP_COUNT);
        flipper_format_write_uint32(ff, "Enabled", enabled, GROUP_COUNT);
        uint32_t opt;
        opt = app->vibro ? 1 : 0;
        flipper_format_write_uint32(ff, "Vibro", &opt, 1);
        opt = app->sound ? 1 : 0;
        flipper_format_write_uint32(ff, "Sound", &opt, 1);
        opt = app->led ? 1 : 0;
        flipper_format_write_uint32(ff, "Led", &opt, 1);
        opt = app->backlight ? 1 : 0;
        flipper_format_write_uint32(ff, "Backlight", &opt, 1);
        opt = app->hold_idx;
        flipper_format_write_uint32(ff, "HoldIdx", &opt, 1);
    } else {
        FURI_LOG_E(TAG, "Cannot open save file");
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    app->dirty = false;
}

static void state_defaults(App* app) {
    for(uint8_t i = 0; i < GROUP_COUNT; i++) {
        snprintf(app->groups[i].name, NAME_LEN, "%s", default_names[i]);
        app->groups[i].value = 0;
        app->groups[i].step = 1;
        app->groups[i].enabled = true;
    }
    app->vibro = true;
    app->sound = false;
    app->led = false;
    app->backlight = false;
    app->hold_idx = HOLD_IDX_DEFAULT;
}

static void state_load(App* app) {
    state_defaults(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_existing(ff, SAVE_PATH)) break;
        FuriString* str = furi_string_alloc();
        uint32_t version;
        bool header_ok = flipper_format_read_header(ff, str, &version) &&
                         furi_string_cmp_str(str, "MultiCounter") == 0;
        if(header_ok) {
            for(uint8_t i = 0; i < GROUP_COUNT; i++) {
                char key[8];
                snprintf(key, sizeof(key), "Name%u", i);
                if(flipper_format_read_string(ff, key, str)) {
                    snprintf(app->groups[i].name, NAME_LEN, "%s", furi_string_get_cstr(str));
                }
            }
            int32_t values[GROUP_COUNT], steps[GROUP_COUNT];
            uint32_t enabled[GROUP_COUNT];
            if(flipper_format_read_int32(ff, "Values", values, GROUP_COUNT)) {
                for(uint8_t i = 0; i < GROUP_COUNT; i++) app->groups[i].value = values[i];
            }
            if(flipper_format_read_int32(ff, "Steps", steps, GROUP_COUNT)) {
                for(uint8_t i = 0; i < GROUP_COUNT; i++)
                    app->groups[i].step = step_values[step_index(steps[i])];
            }
            if(flipper_format_read_uint32(ff, "Enabled", enabled, GROUP_COUNT)) {
                for(uint8_t i = 0; i < GROUP_COUNT; i++) app->groups[i].enabled = enabled[i] != 0;
            }
            uint32_t opt;
            if(flipper_format_read_uint32(ff, "Vibro", &opt, 1)) app->vibro = opt != 0;
            if(flipper_format_read_uint32(ff, "Sound", &opt, 1)) app->sound = opt != 0;
            if(flipper_format_read_uint32(ff, "Led", &opt, 1)) app->led = opt != 0;
            if(flipper_format_read_uint32(ff, "Backlight", &opt, 1)) app->backlight = opt != 0;
            if(flipper_format_read_uint32(ff, "HoldIdx", &opt, 1) && opt < COUNT_OF(hold_repeats))
                app->hold_idx = opt;
        }
        furi_string_free(str);
    } while(false);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

/* ---------------- main counter view ---------------- */

static void main_redraw(App* app) {
    with_view_model(app->main_view, MainModel * m, { (void)m; }, true);
}

// Tiny 6x6 button pictograms: arrows, a dot for OK, a return arrow for Back
static void draw_button_glyph(Canvas* canvas, uint8_t gi, int x, int y) {
    switch(gi) {
    case 0: // up arrow
        canvas_draw_line(canvas, x + 2, y, x + 2, y + 5);
        canvas_draw_line(canvas, x, y + 2, x + 2, y);
        canvas_draw_line(canvas, x + 4, y + 2, x + 2, y);
        break;
    case 1: // down arrow
        canvas_draw_line(canvas, x + 2, y, x + 2, y + 5);
        canvas_draw_line(canvas, x, y + 3, x + 2, y + 5);
        canvas_draw_line(canvas, x + 4, y + 3, x + 2, y + 5);
        break;
    case 2: // left arrow
        canvas_draw_line(canvas, x, y + 2, x + 5, y + 2);
        canvas_draw_line(canvas, x, y + 2, x + 2, y);
        canvas_draw_line(canvas, x, y + 2, x + 2, y + 4);
        break;
    case 3: // right arrow
        canvas_draw_line(canvas, x, y + 2, x + 5, y + 2);
        canvas_draw_line(canvas, x + 5, y + 2, x + 3, y);
        canvas_draw_line(canvas, x + 5, y + 2, x + 3, y + 4);
        break;
    case 4: // OK: central dot
        canvas_draw_disc(canvas, x + 2, y + 2, 2);
        break;
    default: // Back: return arrow
        canvas_draw_line(canvas, x + 5, y, x + 5, y + 3);
        canvas_draw_line(canvas, x + 1, y + 3, x + 5, y + 3);
        canvas_draw_line(canvas, x + 1, y + 3, x + 3, y + 1);
        canvas_draw_line(canvas, x + 1, y + 3, x + 3, y + 5);
        break;
    }
}

static void draw_cell(Canvas* canvas, App* app, uint8_t gi, int x, int y, int w, int h) {
    Group* g = &app->groups[gi];
    bool flash = (app->flash_group == (int8_t)gi);
    if(flash) {
        canvas_draw_box(canvas, x, y, w, h);
        canvas_set_color(canvas, ColorWhite);
    }

    draw_button_glyph(canvas, gi, x + 2, y + 2);

    canvas_set_font(canvas, FontSecondary);
    char name[NAME_LEN];
    snprintf(name, sizeof(name), "%s", g->name);
    while(canvas_string_width(canvas, name) > w - 12 && strlen(name) > 1) {
        name[strlen(name) - 1] = '\0';
    }
    canvas_draw_str_aligned(canvas, x + 10, y + 1, AlignLeft, AlignTop, name);

    if(!g->enabled) {
        canvas_draw_str_aligned(canvas, x + w / 2, y + h - 3, AlignCenter, AlignBottom, "off");
    } else {
        char val[16];
        snprintf(val, sizeof(val), "%ld", (long)g->value);
        canvas_set_font(canvas, FontBigNumbers);
        if(canvas_string_width(canvas, val) > w - 2) canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + w / 2, y + h - 2, AlignCenter, AlignBottom, val);
    }

    if(flash) canvas_set_color(canvas, ColorBlack);
}

// Cell layout mirrors the d-pad: Left/Up/Right on top row, OK/Down/Back below
static const uint8_t cell_group[2][3] = {{2, 0, 3}, {4, 1, 5}};

static void main_draw_callback(Canvas* canvas, void* model) {
    App* app = ((MainModel*)model)->app;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 42, 0, 42, 63);
    canvas_draw_line(canvas, 85, 0, 85, 63);
    canvas_draw_line(canvas, 0, 31, 127, 31);
    for(uint8_t row = 0; row < 2; row++) {
        for(uint8_t col = 0; col < 3; col++) {
            int y = (row == 0) ? 0 : 32;
            int h = (row == 0) ? 31 : 32;
            draw_cell(canvas, app, cell_group[row][col], col * 43, y, 42, h);
        }
    }

    if(app->hint) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 13, 23, 102, 18);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, 13, 23, 102, 18, 3);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Hold OK for menu");
    }
}

static void hint_timer_callback(void* context) {
    App* app = context;
    app->hint = false;
    main_redraw(app);
}

static void flash_timer_callback(void* context) {
    App* app = context;
    app->flash_group = -1;
    main_redraw(app);
}

static void group_add(App* app, uint8_t gi, int8_t sign) {
    Group* g = &app->groups[gi];
    if(!g->enabled) return;
    int32_t value = g->value + sign * g->step;
    if(value < 0) value = 0;
    if(value > VALUE_MAX) value = VALUE_MAX;
    g->value = value;
    app->dirty = true;
    app->flash_group = gi;
    furi_timer_start(app->flash_timer, furi_ms_to_ticks(FLASH_MS));
    main_redraw(app);

    if(app->vibro) notification_message(app->notifications, &sequence_single_vibro);
    if(app->sound)
        notification_message(app->notifications, sign > 0 ? &seq_beep_plus : &seq_beep_minus);
    if(app->led)
        notification_message(
            app->notifications, sign > 0 ? &sequence_blink_green_10 : &sequence_blink_red_10);
}

static uint8_t key_to_group(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return 0;
    case InputKeyDown:
        return 1;
    case InputKeyLeft:
        return 2;
    case InputKeyRight:
        return 3;
    case InputKeyOk:
        return 4;
    default:
        return 5; // InputKeyBack
    }
}

static bool main_input_callback(InputEvent* event, void* context) {
    App* app = context;
    uint8_t gi = key_to_group(event->key);

    if(app->hint) {
        app->hint = false;
        furi_timer_stop(app->hint_timer);
        main_redraw(app);
    }

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
    case InputKeyLeft:
    case InputKeyRight:
        if(event->type == InputTypeShort) group_add(app, gi, +1);
        else if(event->type == InputTypeLong) group_add(app, gi, -1);
        break;

    case InputKeyOk:
        // Short = +step; long-then-release = -step; keep holding ~2s = menu
        if(event->type == InputTypeShort) {
            group_add(app, gi, +1);
        } else if(event->type == InputTypeLong) {
            app->ok_hold = true;
            app->ok_consumed = false;
            app->ok_repeats = 0;
        } else if(event->type == InputTypeRepeat && app->ok_hold && !app->ok_consumed) {
            if(++app->ok_repeats >= hold_repeats[app->hold_idx]) {
                app->ok_consumed = true;
                notification_message(app->notifications, &sequence_single_vibro);
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
            }
        } else if(event->type == InputTypeRelease) {
            if(app->ok_hold && !app->ok_consumed) group_add(app, gi, -1);
            app->ok_hold = false;
        }
        break;

    case InputKeyBack:
        // Short = +step; long-then-release = -step; keep holding ~2s = exit
        if(event->type == InputTypeShort) {
            group_add(app, gi, +1);
        } else if(event->type == InputTypeLong) {
            app->back_hold = true;
            app->back_consumed = false;
            app->back_repeats = 0;
        } else if(event->type == InputTypeRepeat && app->back_hold && !app->back_consumed) {
            if(++app->back_repeats >= hold_repeats[app->hold_idx]) {
                app->back_consumed = true;
                notification_message(app->notifications, &sequence_single_vibro);
                view_dispatcher_stop(app->view_dispatcher);
            }
        } else if(event->type == InputTypeRelease) {
            if(app->back_hold && !app->back_consumed) group_add(app, gi, -1);
            app->back_hold = false;
        }
        break;

    default:
        break;
    }
    return true; // consume everything, including short Back
}

/* ---------------- group config screen ---------------- */

static void step_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->groups[app->cfg_group].step = step_values[idx];
    variable_item_set_current_value_text(item, step_texts[idx]);
    app->dirty = true;
}

static void enabled_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    bool enabled = variable_item_get_current_value_index(item) != 0;
    app->groups[app->cfg_group].enabled = enabled;
    variable_item_set_current_value_text(item, enabled ? "ON" : "OFF");
    app->dirty = true;
}

static void rename_done_callback(void* context) {
    App* app = context;
    if(strlen(app->name_buf) > 0) {
        snprintf(app->groups[app->cfg_group].name, NAME_LEN, "%s", app->name_buf);
        variable_item_set_current_value_text(app->item_name, app->groups[app->cfg_group].name);
        app->dirty = true;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdGroupCfg);
}

static void group_cfg_enter_callback(void* context, uint32_t index) {
    App* app = context;
    if(index == 0) { // Name -> on-screen keyboard
        snprintf(app->name_buf, NAME_LEN, "%s", app->groups[app->cfg_group].name);
        text_input_set_header_text(app->text_input, "Group name:");
        text_input_set_result_callback(
            app->text_input, rename_done_callback, app, app->name_buf, NAME_LEN, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdTextInput);
    } else if(index == 3) { // Count -> zero it
        app->groups[app->cfg_group].value = 0;
        variable_item_set_current_value_text(app->item_value, "0");
        app->dirty = true;
    }
}

static void group_cfg_build(App* app) {
    Group* g = &app->groups[app->cfg_group];
    VariableItem* item;
    char buf[16];

    variable_item_list_reset(app->group_cfg);

    item = variable_item_list_add(app->group_cfg, "Name", 1, NULL, app);
    variable_item_set_current_value_text(item, g->name);
    app->item_name = item;

    item = variable_item_list_add(
        app->group_cfg, "Step", COUNT_OF(step_values), step_changed_callback, app);
    uint8_t si = step_index(g->step);
    variable_item_set_current_value_index(item, si);
    variable_item_set_current_value_text(item, step_texts[si]);

    item = variable_item_list_add(app->group_cfg, "Enabled", 2, enabled_changed_callback, app);
    variable_item_set_current_value_index(item, g->enabled ? 1 : 0);
    variable_item_set_current_value_text(item, g->enabled ? "ON" : "OFF");

    item = variable_item_list_add(app->group_cfg, "Count (OK = 0)", 1, NULL, app);
    snprintf(buf, sizeof(buf), "%ld", (long)g->value);
    variable_item_set_current_value_text(item, buf);
    app->item_value = item;

    variable_item_list_set_selected_item(app->group_cfg, 0);
}

/* ---------------- settings screen ---------------- */

static void bool_item_update(VariableItem* item, bool value) {
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
}

static void vibro_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->vibro = variable_item_get_current_value_index(item) != 0;
    bool_item_update(item, app->vibro);
    if(app->vibro) notification_message(app->notifications, &sequence_single_vibro);
    app->dirty = true;
}

static void sound_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->sound = variable_item_get_current_value_index(item) != 0;
    bool_item_update(item, app->sound);
    if(app->sound) notification_message(app->notifications, &seq_beep_plus);
    app->dirty = true;
}

static void led_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->led = variable_item_get_current_value_index(item) != 0;
    bool_item_update(item, app->led);
    if(app->led) notification_message(app->notifications, &sequence_blink_green_10);
    app->dirty = true;
}

static void backlight_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->backlight = variable_item_get_current_value_index(item) != 0;
    bool_item_update(item, app->backlight);
    notification_message(
        app->notifications,
        app->backlight ? &sequence_display_backlight_enforce_on :
                         &sequence_display_backlight_enforce_auto);
    app->dirty = true;
}

static void hold_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->hold_idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, hold_texts[app->hold_idx]);
    app->dirty = true;
}

static void settings_build(App* app) {
    VariableItem* item;
    variable_item_list_reset(app->settings);

    item = variable_item_list_add(app->settings, "Vibro on count", 2, vibro_changed_callback, app);
    variable_item_set_current_value_index(item, app->vibro ? 1 : 0);
    bool_item_update(item, app->vibro);

    item = variable_item_list_add(app->settings, "Sound on count", 2, sound_changed_callback, app);
    variable_item_set_current_value_index(item, app->sound ? 1 : 0);
    bool_item_update(item, app->sound);

    item = variable_item_list_add(app->settings, "LED on count", 2, led_changed_callback, app);
    variable_item_set_current_value_index(item, app->led ? 1 : 0);
    bool_item_update(item, app->led);

    item = variable_item_list_add(
        app->settings, "Backlight always", 2, backlight_changed_callback, app);
    variable_item_set_current_value_index(item, app->backlight ? 1 : 0);
    bool_item_update(item, app->backlight);

    item = variable_item_list_add(
        app->settings, "Hold OK/Back", COUNT_OF(hold_repeats), hold_changed_callback, app);
    variable_item_set_current_value_index(item, app->hold_idx);
    variable_item_set_current_value_text(item, hold_texts[app->hold_idx]);

    variable_item_list_set_selected_item(app->settings, 0);
}

/* ---------------- menus ---------------- */

static void group_menu_callback(void* context, uint32_t index) {
    App* app = context;
    app->cfg_group = index;
    group_cfg_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdGroupCfg);
}

static void group_menu_build(App* app) {
    submenu_reset(app->group_menu);
    submenu_set_header(app->group_menu, "Groups");
    for(uint8_t i = 0; i < GROUP_COUNT; i++) {
        Group* g = &app->groups[i];
        char label[40];
        snprintf(
            label,
            sizeof(label),
            "%s %s: %ld%s",
            group_marks[i],
            g->name,
            (long)g->value,
            g->enabled ? "" : " (off)");
        submenu_add_item(app->group_menu, label, i, group_menu_callback, app);
    }
}

static void menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case MenuIndexHelp:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdHelp);
        break;
    case MenuIndexGroups:
        group_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdGroupMenu);
        break;
    case MenuIndexSettings:
        settings_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSettings);
        break;
    case MenuIndexResetAll:
        app->factory_pending = false;
        dialog_ex_set_header(
            app->reset_dialog, "Reset all counters?", 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->reset_dialog, "All counts will be\nset to zero.", 64, 26, AlignCenter, AlignTop);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdResetDialog);
        break;
    case MenuIndexFactoryReset:
        app->factory_pending = true;
        dialog_ex_set_header(
            app->reset_dialog, "Reset to defaults?", 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->reset_dialog,
            "Counts, names, steps\nand settings will be\nreset to defaults.",
            64,
            22,
            AlignCenter,
            AlignTop);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdResetDialog);
        break;
    default:
        break;
    }
}

static void reset_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    if(result == DialogExResultRight) {
        if(app->factory_pending) {
            state_defaults(app);
            // defaults have backlight enforcement off
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        } else {
            for(uint8_t i = 0; i < GROUP_COUNT; i++) app->groups[i].value = 0;
        }
        app->dirty = true;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMain);
    } else if(result == DialogExResultLeft) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
    }
}

/* ---------------- navigation ---------------- */

static uint32_t nav_to_main(void* context) {
    UNUSED(context);
    return ViewIdMain;
}

static uint32_t nav_to_menu(void* context) {
    UNUSED(context);
    return ViewIdMenu;
}

static uint32_t nav_to_group_menu(void* context) {
    UNUSED(context);
    return ViewIdGroupMenu;
}

static uint32_t nav_to_group_cfg(void* context) {
    UNUSED(context);
    return ViewIdGroupCfg;
}

static void tick_callback(void* context) {
    App* app = context;
    if(app->dirty) state_save(app);
}

/* ---------------- app lifecycle ---------------- */

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    app->flash_group = -1;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, tick_callback, AUTOSAVE_MS);

    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(MainModel));
    with_view_model(app->main_view, MainModel * m, { m->app = app; }, false);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, main_draw_callback);
    view_set_input_callback(app->main_view, main_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMain, app->main_view);

    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "Multi Tally");
    submenu_add_item(app->menu, "How to use", MenuIndexHelp, menu_callback, app);
    submenu_add_item(app->menu, "Groups", MenuIndexGroups, menu_callback, app);
    submenu_add_item(app->menu, "Settings", MenuIndexSettings, menu_callback, app);
    submenu_add_item(app->menu, "Reset all counters", MenuIndexResetAll, menu_callback, app);
    submenu_add_item(app->menu, "Reset to defaults", MenuIndexFactoryReset, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->menu), nav_to_main);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMenu, submenu_get_view(app->menu));

    app->group_menu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->group_menu), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdGroupMenu, submenu_get_view(app->group_menu));

    app->group_cfg = variable_item_list_alloc();
    variable_item_list_set_enter_callback(app->group_cfg, group_cfg_enter_callback, app);
    view_set_previous_callback(variable_item_list_get_view(app->group_cfg), nav_to_group_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdGroupCfg, variable_item_list_get_view(app->group_cfg));

    app->settings = variable_item_list_alloc();
    view_set_previous_callback(variable_item_list_get_view(app->settings), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdSettings, variable_item_list_get_view(app->settings));

    app->text_input = text_input_alloc();
    text_input_set_minimum_length(app->text_input, 1);
    view_set_previous_callback(text_input_get_view(app->text_input), nav_to_group_cfg);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdTextInput, text_input_get_view(app->text_input));

    app->reset_dialog = dialog_ex_alloc();
    dialog_ex_set_left_button_text(app->reset_dialog, "Cancel");
    dialog_ex_set_right_button_text(app->reset_dialog, "Reset");
    dialog_ex_set_context(app->reset_dialog, app);
    dialog_ex_set_result_callback(app->reset_dialog, reset_dialog_callback);
    view_set_previous_callback(dialog_ex_get_view(app->reset_dialog), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdResetDialog, dialog_ex_get_view(app->reset_dialog));

    app->help = widget_alloc();
    widget_add_text_scroll_element(app->help, 0, 0, 128, 64, help_text);
    view_set_previous_callback(widget_get_view(app->help), nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdHelp, widget_get_view(app->help));

    app->flash_timer = furi_timer_alloc(flash_timer_callback, FuriTimerTypeOnce, app);
    app->hint_timer = furi_timer_alloc(hint_timer_callback, FuriTimerTypeOnce, app);

    return app;
}

static void app_free(App* app) {
    furi_timer_stop(app->flash_timer);
    furi_timer_free(app->flash_timer);
    furi_timer_stop(app->hint_timer);
    furi_timer_free(app->hint_timer);

    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMain);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdGroupMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdGroupCfg);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSettings);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdResetDialog);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdHelp);

    view_free(app->main_view);
    submenu_free(app->menu);
    submenu_free(app->group_menu);
    variable_item_list_free(app->group_cfg);
    variable_item_list_free(app->settings);
    text_input_free(app->text_input);
    dialog_ex_free(app->reset_dialog);
    widget_free(app->help);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t multi_tally_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    state_load(app);
    if(app->backlight)
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    app->hint = true;
    furi_timer_start(app->hint_timer, furi_ms_to_ticks(4000));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMain);
    view_dispatcher_run(app->view_dispatcher);

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    state_save(app);
    app_free(app);
    return 0;
}
