#include <furi.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <furi_hal_rtc.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../lib/hvac_daikin64/hvac_daikin64.h"

#define DAIKIN64_SETTINGS_DIR EXT_PATH("apps_data/daikin64_ac_remote")
#define DAIKIN64_SETTINGS_PATH EXT_PATH("apps_data/daikin64_ac_remote/state.bin")
#define DAIKIN64_SETTINGS_MAGIC 0xD6
#define DAIKIN64_SETTINGS_VERSION 2
#define DAIKIN64_FAVORITE_COUNT 5

typedef enum {
    Daikin64ViewRemote,
} Daikin64View;

typedef enum {
    Daikin64PageMain,
    Daikin64PageFunctions,
    Daikin64PageTimer,
    Daikin64PageFavorites,
    Daikin64PageCount,
} Daikin64Page;

typedef enum {
    Daikin64MainPower,
    Daikin64MainSend,
    Daikin64MainMode,
    Daikin64MainFan,
    Daikin64MainCount,
} Daikin64MainItem;

typedef enum {
    Daikin64FuncTurbo,
    Daikin64FuncQuiet,
    Daikin64FuncSleep,
    Daikin64FuncSwing,
    Daikin64FuncCount,
} Daikin64FuncItem;

typedef enum {
    Daikin64TimerOnValue,
    Daikin64TimerOnSet,
    Daikin64TimerOnClear,
    Daikin64TimerOffValue,
    Daikin64TimerOffSet,
    Daikin64TimerOffClear,
    Daikin64TimerCount,
} Daikin64TimerItem;

typedef enum {
    Daikin64FavoriteOne,
    Daikin64FavoriteTwo,
    Daikin64FavoriteThree,
    Daikin64FavoriteFour,
    Daikin64FavoriteFive,
    Daikin64FavoriteCount,
} Daikin64FavoriteItem;

typedef struct {
    uint8_t value;
    const char* label;
} Daikin64Option;

typedef struct {
    Daikin64State state;
    Daikin64State favorites[DAIKIN64_FAVORITE_COUNT];
    uint8_t page;
    uint8_t selected;
    uint8_t mode_index;
    uint8_t fan_index;
} Daikin64RemoteModel;

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    View* view;
} Daikin64App;

typedef struct {
    Daikin64State state;
    Daikin64State favorites[DAIKIN64_FAVORITE_COUNT];
} Daikin64PersistedState;

static const Daikin64Option daikin64_modes[] = {
    {Daikin64ModeAuto, "AUTO"},
    {Daikin64ModeCool, "COOL"},
    {Daikin64ModeHeat, "HEAT"},
    {Daikin64ModeDry, "DRY"},
    {Daikin64ModeFan, "FAN"},
};

static const Daikin64Option daikin64_fans[] = {
    {Daikin64FanLow, "LOW"},
    {Daikin64FanMedium, "MED"},
    {Daikin64FanHigh, "HIGH"},
    {Daikin64FanTurbo, "TURBO"},
    {Daikin64FanAuto, "AUTO"},
    {Daikin64FanQuiet, "QUIET"},
};

static const uint8_t daikin64_digit_map[10][5] = {
    {0x07, 0x05, 0x05, 0x05, 0x07},
    {0x02, 0x06, 0x02, 0x02, 0x07},
    {0x07, 0x01, 0x07, 0x04, 0x07},
    {0x07, 0x01, 0x07, 0x01, 0x07},
    {0x05, 0x05, 0x07, 0x01, 0x01},
    {0x07, 0x04, 0x07, 0x01, 0x07},
    {0x07, 0x04, 0x07, 0x05, 0x07},
    {0x07, 0x01, 0x02, 0x02, 0x02},
    {0x07, 0x05, 0x07, 0x05, 0x07},
    {0x07, 0x05, 0x07, 0x01, 0x07},
};

static void
    daikin64_send_with_feedback(Daikin64App* app, const Daikin64State* state, bool power_toggle) {
    notification_message(app->notifications, &sequence_blink_white_100);
    if(power_toggle) {
        daikin64_send_power_toggle(state);
    } else {
        daikin64_send(state);
    }
    notification_message(app->notifications, &sequence_blink_stop);
}

static void daikin64_notify_only(Daikin64App* app) {
    notification_message(app->notifications, &sequence_blink_white_100);
    notification_message(app->notifications, &sequence_blink_stop);
}

static void daikin64_prepare_setting_send(Daikin64State* send_state, const Daikin64State* state) {
    *send_state = *state;
    send_state->power = false;
}

static uint8_t daikin64_page_item_count(uint8_t page) {
    switch(page) {
    case Daikin64PageMain:
        return Daikin64MainCount;
    case Daikin64PageFunctions:
        return Daikin64FuncCount;
    case Daikin64PageTimer:
        return Daikin64TimerCount;
    case Daikin64PageFavorites:
        return Daikin64FavoriteCount;
    default:
        return 1;
    }
}

static uint8_t daikin64_find_mode_index(uint8_t mode) {
    for(uint8_t i = 0; i < COUNT_OF(daikin64_modes); i++) {
        if(daikin64_modes[i].value == mode) return i;
    }
    return 1;
}

static uint8_t daikin64_find_fan_index(uint8_t fan) {
    for(uint8_t i = 0; i < COUNT_OF(daikin64_fans); i++) {
        if(daikin64_fans[i].value == fan) return i;
    }
    return 0;
}

static bool daikin64_is_valid_mode(uint8_t mode) {
    for(uint8_t i = 0; i < COUNT_OF(daikin64_modes); i++) {
        if(daikin64_modes[i].value == mode) return true;
    }
    return false;
}

static bool daikin64_is_valid_fan(uint8_t fan) {
    for(uint8_t i = 0; i < COUNT_OF(daikin64_fans); i++) {
        if(daikin64_fans[i].value == fan) return true;
    }
    return false;
}

static bool daikin64_is_valid_state(const Daikin64State* state) {
    return daikin64_is_valid_mode(state->mode) && daikin64_is_valid_fan(state->fan) &&
           (state->temperature >= DAIKIN64_MIN_TEMP) &&
           (state->temperature <= DAIKIN64_MAX_TEMP) &&
           (state->on_timer_minutes < 24U * 60U) &&
           (state->off_timer_minutes < 24U * 60U);
}

static bool daikin64_is_valid_persisted_state(const Daikin64PersistedState* persisted) {
    if(!daikin64_is_valid_state(&persisted->state)) return false;

    for(uint8_t i = 0; i < COUNT_OF(persisted->favorites); i++) {
        if(!daikin64_is_valid_state(&persisted->favorites[i])) return false;
    }

    return true;
}

static void daikin64_sync_indexes(Daikin64RemoteModel* model) {
    model->mode_index = daikin64_find_mode_index(model->state.mode);
    model->fan_index = daikin64_find_fan_index(model->state.fan);
}

static bool daikin64_load_saved_state(Daikin64RemoteModel* model) {
    Daikin64PersistedState persisted;

    if(!saved_struct_load(
           DAIKIN64_SETTINGS_PATH,
           &persisted,
           sizeof(persisted),
           DAIKIN64_SETTINGS_MAGIC,
           DAIKIN64_SETTINGS_VERSION)) {
        return false;
    }

    if(!daikin64_is_valid_persisted_state(&persisted)) return false;

    model->state = persisted.state;
    for(uint8_t i = 0; i < COUNT_OF(model->favorites); i++) {
        model->favorites[i] = persisted.favorites[i];
    }
    daikin64_sync_indexes(model);
    return true;
}

static void daikin64_save_state(const Daikin64RemoteModel* model) {
    Daikin64PersistedState persisted = {
        .state = model->state,
    };

    for(uint8_t i = 0; i < COUNT_OF(model->favorites); i++) {
        persisted.favorites[i] = model->favorites[i];
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, DAIKIN64_SETTINGS_DIR);
    furi_record_close(RECORD_STORAGE);

    saved_struct_save(
        DAIKIN64_SETTINGS_PATH,
        &persisted,
        sizeof(persisted),
        DAIKIN64_SETTINGS_MAGIC,
        DAIKIN64_SETTINGS_VERSION);
}

static void daikin64_draw_button(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const char* label,
    bool selected) {
    canvas_set_font(canvas, FontSecondary);
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, x, y, width, height);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, x, y, width, height);
    }

    uint16_t text_width = canvas_string_width(canvas, label);
    int32_t text_x = x + ((int32_t)width - (int32_t)text_width) / 2;
    if(text_x < x + 1) text_x = x + 1;
    canvas_draw_str(canvas, text_x, y + height - 4, label);
    canvas_set_color(canvas, ColorBlack);
}

static void daikin64_draw_round_button(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const char* label,
    bool selected) {
    canvas_set_font(canvas, FontSecondary);
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, x, y, width, height, 4);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, x, y, width, height, 4);
    }

    uint16_t text_width = canvas_string_width(canvas, label);
    int32_t text_x = x + ((int32_t)width - (int32_t)text_width) / 2;
    if(text_x < x + 1) text_x = x + 1;
    canvas_draw_str(canvas, text_x, y + height - 4, label);
    canvas_set_color(canvas, ColorBlack);
}

static void daikin64_draw_digit(Canvas* canvas, uint8_t x, uint8_t y, uint8_t digit, uint8_t scale) {
    if(digit > 9) return;

    for(uint8_t row = 0; row < 5; row++) {
        for(uint8_t col = 0; col < 3; col++) {
            if(daikin64_digit_map[digit][row] & (1 << (2 - col))) {
                canvas_draw_box(canvas, x + col * scale, y + row * scale, scale, scale);
            }
        }
    }
}

static void daikin64_draw_big_temp(Canvas* canvas, uint8_t x, uint8_t y, uint8_t temperature) {
    daikin64_draw_digit(canvas, x, y, temperature / 10, 5);
    daikin64_draw_digit(canvas, x + 22, y, temperature % 10, 5);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 54, y + 18, "C");
}

static void daikin64_draw_header(Canvas* canvas, const char* title, const char* page) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 0, 0, 64, 128);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 18, 10, title);
    canvas_draw_str(canvas, 5, 20, "IR <");
    uint16_t page_right = 45 + canvas_string_width(canvas, "1/4");
    canvas_draw_str(canvas, page_right - canvas_string_width(canvas, page), 20, page);
    canvas_draw_line(canvas, 0, 24, 63, 24);
}

static void daikin64_draw_snowflake(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_line(canvas, x - 4, y, x + 4, y);
    canvas_draw_line(canvas, x, y - 4, x, y + 4);
    canvas_draw_line(canvas, x - 3, y - 3, x + 3, y + 3);
    canvas_draw_line(canvas, x - 3, y + 3, x + 3, y - 3);
}

static void daikin64_draw_heat_icon(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_circle(canvas, x, y, 2);
    canvas_draw_line(canvas, x, y - 6, x, y - 4);
    canvas_draw_line(canvas, x, y + 4, x, y + 6);
    canvas_draw_line(canvas, x - 6, y, x - 4, y);
    canvas_draw_line(canvas, x + 4, y, x + 6, y);
    canvas_draw_line(canvas, x - 4, y - 4, x - 3, y - 3);
    canvas_draw_line(canvas, x + 3, y + 3, x + 4, y + 4);
    canvas_draw_line(canvas, x + 4, y - 4, x + 3, y - 3);
    canvas_draw_line(canvas, x - 4, y + 4, x - 3, y + 3);
}

static void daikin64_draw_dry_icon(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_line(canvas, x, y - 5, x + 4, y);
    canvas_draw_line(canvas, x + 4, y, x + 2, y + 5);
    canvas_draw_line(canvas, x + 2, y + 5, x - 2, y + 5);
    canvas_draw_line(canvas, x - 2, y + 5, x - 4, y);
    canvas_draw_line(canvas, x - 4, y, x, y - 5);
}

static void daikin64_draw_auto_icon(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_line(canvas, x - 3, y - 5, x + 2, y - 5);
    canvas_draw_line(canvas, x + 2, y - 5, x + 5, y - 2);
    canvas_draw_line(canvas, x + 4, y + 5, x - 2, y + 5);
    canvas_draw_line(canvas, x - 2, y + 5, x - 5, y + 2);
    canvas_draw_line(canvas, x + 4, y - 4, x + 6, y - 2);
    canvas_draw_line(canvas, x + 6, y - 2, x + 4, y - 1);
    canvas_draw_line(canvas, x - 4, y + 4, x - 6, y + 2);
    canvas_draw_line(canvas, x - 6, y + 2, x - 4, y + 1);
    canvas_draw_line(canvas, x - 3, y + 4, x, y - 4);
    canvas_draw_line(canvas, x + 3, y + 4, x, y - 4);
    canvas_draw_line(canvas, x - 2, y + 1, x + 2, y + 1);
}

static void daikin64_draw_fan_icon(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_circle(canvas, x, y, 5);
    canvas_draw_line(canvas, x, y, x - 1, y - 4);
    canvas_draw_line(canvas, x, y, x + 1, y - 4);
    canvas_draw_line(canvas, x - 1, y - 4, x + 1, y - 4);
    canvas_draw_line(canvas, x, y, x + 4, y + 3);
    canvas_draw_line(canvas, x, y, x + 3, y + 4);
    canvas_draw_line(canvas, x + 4, y + 3, x + 3, y + 4);
    canvas_draw_line(canvas, x, y, x - 4, y + 3);
    canvas_draw_line(canvas, x, y, x - 3, y + 4);
    canvas_draw_line(canvas, x - 4, y + 3, x - 3, y + 4);
    canvas_draw_disc(canvas, x, y, 1);
}

static void daikin64_draw_mode_icon(Canvas* canvas, uint8_t mode, uint8_t x, uint8_t y) {
    switch(mode) {
    case Daikin64ModeAuto:
        daikin64_draw_auto_icon(canvas, x, y);
        break;
    case Daikin64ModeCool:
        daikin64_draw_snowflake(canvas, x, y);
        break;
    case Daikin64ModeHeat:
        daikin64_draw_heat_icon(canvas, x, y);
        break;
    case Daikin64ModeDry:
        daikin64_draw_dry_icon(canvas, x, y);
        break;
    case Daikin64ModeFan:
        daikin64_draw_fan_icon(canvas, x, y);
        break;
    default:
        daikin64_draw_snowflake(canvas, x, y);
        break;
    }
}

static void daikin64_draw_fan_indicator(Canvas* canvas, uint8_t x, uint8_t y, uint8_t fan) {
    uint8_t filled_bars = 0;

    if(fan == Daikin64FanAuto) {
        canvas_draw_frame(canvas, x, y - 4, 4, 4);
        canvas_draw_frame(canvas, x + 5, y - 7, 4, 7);
        canvas_draw_frame(canvas, x + 10, y - 10, 4, 10);
        return;
    } else if(fan == Daikin64FanQuiet) {
        canvas_draw_box(canvas, x, y - 6, 3, 5);
        canvas_draw_line(canvas, x + 3, y - 6, x + 8, y - 10);
        canvas_draw_line(canvas, x + 8, y - 10, x + 8, y + 2);
        canvas_draw_line(canvas, x + 8, y + 2, x + 3, y - 2);
        canvas_draw_line(canvas, x + 12, y - 10, x + 1, y + 2);
        return;
    } else if(fan == Daikin64FanLow) {
        filled_bars = 1;
    } else if(fan == Daikin64FanMedium) {
        filled_bars = 2;
    } else {
        filled_bars = 3;
    }

    if(filled_bars >= 1) canvas_draw_box(canvas, x, y - 4, 4, 4);
    if(filled_bars >= 2) canvas_draw_box(canvas, x + 5, y - 7, 4, 7);
    if(filled_bars >= 3) canvas_draw_box(canvas, x + 10, y - 10, 4, 10);

    if(fan == Daikin64FanTurbo) {
        canvas_draw_line(canvas, x - 1, y - 12, x + 14, y - 12);
    }
}

static void daikin64_format_timer(char* buffer, size_t size, uint16_t minutes, bool enabled) {
    UNUSED(enabled);
    minutes %= 24U * 60U;
    snprintf(buffer, size, "%02u:%02u", minutes / 60U, minutes % 60U);
}

static void daikin64_format_clock(char* buffer, size_t size) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    uint8_t hour = datetime.hour % 24U;
    uint8_t minute = datetime.minute % 60U;
    snprintf(buffer, size, "%02u:%02u", hour, minute);
}

static void daikin64_draw_main(Canvas* canvas, const Daikin64RemoteModel* model) {
    daikin64_draw_header(canvas, "DAIKIN", "1/4");

    daikin64_draw_mode_icon(canvas, model->state.mode, 13, 34);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 25, 37, daikin64_modes[model->mode_index].label);
    canvas_draw_str(canvas, 6, 49, daikin64_fans[model->fan_index].label);
    daikin64_draw_fan_indicator(canvas, 48, 49, model->state.fan);
    daikin64_draw_big_temp(canvas, 10, 58, model->state.temperature);

    daikin64_draw_button(canvas, 3, 91, 28, 13, "PWR", model->selected == Daikin64MainPower);
    daikin64_draw_button(canvas, 33, 91, 28, 13, "SEND", model->selected == Daikin64MainSend);
    daikin64_draw_button(canvas, 3, 109, 28, 13, "MODE", model->selected == Daikin64MainMode);
    daikin64_draw_button(canvas, 33, 109, 28, 13, "FAN", model->selected == Daikin64MainFan);
}

static void daikin64_draw_functions(Canvas* canvas, const Daikin64RemoteModel* model) {
    daikin64_draw_header(canvas, "FUNCS", "2/4");

    daikin64_draw_button(canvas, 6, 35, 52, 14, "TURBO", model->selected == Daikin64FuncTurbo);
    daikin64_draw_button(canvas, 6, 56, 52, 14, "QUIET", model->selected == Daikin64FuncQuiet);
    daikin64_draw_button(canvas, 6, 77, 52, 14, "SLEEP", model->selected == Daikin64FuncSleep);
    daikin64_draw_button(canvas, 6, 98, 52, 14, "SWING", model->selected == Daikin64FuncSwing);
}

static void daikin64_draw_timer(Canvas* canvas, const Daikin64RemoteModel* model) {
    char clock_time[6];
    char on_time[6];
    char off_time[6];
    uint16_t clock_width;

    daikin64_draw_header(canvas, "TIMER", "3/4");
    daikin64_format_clock(clock_time, sizeof(clock_time));
    daikin64_format_timer(
        on_time, sizeof(on_time), model->state.on_timer_minutes, model->state.on_timer_enabled);
    daikin64_format_timer(
        off_time, sizeof(off_time), model->state.off_timer_minutes, model->state.off_timer_enabled);

    canvas_set_font(canvas, FontSecondary);
    clock_width = canvas_string_width(canvas, clock_time);
    canvas_draw_str(canvas, 32 - clock_width / 2, 38, clock_time);
    canvas_draw_line(canvas, 5, 44, 58, 44);

    canvas_draw_str(canvas, 7, 57, "ON");
    daikin64_draw_button(canvas, 27, 48, 33, 14, on_time, model->selected == Daikin64TimerOnValue);
    daikin64_draw_round_button(canvas, 7, 66, 24, 13, "SET", model->selected == Daikin64TimerOnSet);
    daikin64_draw_round_button(canvas, 35, 66, 24, 13, "CLR", model->selected == Daikin64TimerOnClear);

    canvas_draw_line(canvas, 5, 83, 58, 83);

    canvas_draw_str(canvas, 7, 96, "OFF");
    daikin64_draw_button(
        canvas, 27, 87, 33, 14, off_time, model->selected == Daikin64TimerOffValue);
    daikin64_draw_round_button(canvas, 7, 105, 24, 13, "SET", model->selected == Daikin64TimerOffSet);
    daikin64_draw_round_button(canvas, 35, 105, 24, 13, "CLR", model->selected == Daikin64TimerOffClear);
}

static void daikin64_draw_favorites(Canvas* canvas, const Daikin64RemoteModel* model) {
    char slot[3];
    char temp[4];

    daikin64_draw_header(canvas, "FAVS", "4/4");
    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < DAIKIN64_FAVORITE_COUNT; i++) {
        const Daikin64State* favorite = &model->favorites[i];
        uint8_t mode_index = daikin64_find_mode_index(favorite->mode);
        uint8_t y = 34 + i * 13;
        snprintf(slot, sizeof(slot), "%u", i + 1);
        snprintf(temp, sizeof(temp), "%u", favorite->temperature);

        if(model->selected == i) {
            canvas_draw_box(canvas, 4, y - 8, 56, 11);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 8, y, slot);
            canvas_draw_str(canvas, 18, y, daikin64_modes[mode_index].label);
            canvas_draw_str(canvas, 48, y, temp);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 8, y, slot);
            canvas_draw_str(canvas, 18, y, daikin64_modes[mode_index].label);
            canvas_draw_str(canvas, 48, y, temp);
        }
    }
}

static void daikin64_draw_callback(Canvas* canvas, void* context) {
    Daikin64RemoteModel* model = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(model->page) {
    case Daikin64PageMain:
        daikin64_draw_main(canvas, model);
        break;
    case Daikin64PageFunctions:
        daikin64_draw_functions(canvas, model);
        break;
    case Daikin64PageTimer:
        daikin64_draw_timer(canvas, model);
        break;
    case Daikin64PageFavorites:
        daikin64_draw_favorites(canvas, model);
        break;
    default:
        break;
    }
}

static void daikin64_apply_favorite(Daikin64RemoteModel* model, uint8_t favorite_index) {
    model->state = model->favorites[favorite_index];
    model->mode_index = daikin64_find_mode_index(model->state.mode);
    model->fan_index = daikin64_find_fan_index(model->state.fan);
}

static uint16_t daikin64_next_timer_minutes(uint16_t minutes) {
    minutes += 30U;
    if(minutes >= 24U * 60U) minutes = 0;
    return minutes;
}

static bool daikin64_handle_ok(
    Daikin64RemoteModel* model,
    Daikin64State* send_state,
    bool* should_send,
    bool* power_toggle,
    bool* state_changed,
    bool long_press) {
    if(model->page == Daikin64PageMain) {
        switch(model->selected) {
        case Daikin64MainPower:
            model->state.power = !model->state.power;
            *send_state = model->state;
            *power_toggle = true;
            *should_send = true;
            *state_changed = true;
            break;
        case Daikin64MainSend:
            daikin64_prepare_setting_send(send_state, &model->state);
            *should_send = true;
            break;
        case Daikin64MainMode:
            model->mode_index = (model->mode_index + 1) % COUNT_OF(daikin64_modes);
            model->state.mode = daikin64_modes[model->mode_index].value;
            daikin64_prepare_setting_send(send_state, &model->state);
            *should_send = true;
            *state_changed = true;
            break;
        case Daikin64MainFan:
            model->fan_index = (model->fan_index + 1) % COUNT_OF(daikin64_fans);
            model->state.fan = daikin64_fans[model->fan_index].value;
            daikin64_prepare_setting_send(send_state, &model->state);
            *should_send = true;
            *state_changed = true;
            break;
        default:
            break;
        }
        return true;
    }

    if(model->page == Daikin64PageFunctions) {
        switch(model->selected) {
        case Daikin64FuncTurbo:
            model->state.fan = Daikin64FanTurbo;
            break;
        case Daikin64FuncQuiet:
            model->state.fan = Daikin64FanQuiet;
            break;
        case Daikin64FuncSleep:
            model->state.sleep = !model->state.sleep;
            break;
        case Daikin64FuncSwing:
            model->state.swing = !model->state.swing;
            break;
        default:
            break;
        }

        model->fan_index = daikin64_find_fan_index(model->state.fan);
        daikin64_prepare_setting_send(send_state, &model->state);
        *should_send = true;
        *state_changed = true;
        return true;
    }

    if(model->page == Daikin64PageTimer) {
        UNUSED(long_press);
        switch(model->selected) {
        case Daikin64TimerOnValue:
            model->state.on_timer_minutes = daikin64_next_timer_minutes(model->state.on_timer_minutes);
            *state_changed = true;
            return false;
        case Daikin64TimerOnSet:
            model->state.on_timer_enabled = true;
            break;
        case Daikin64TimerOnClear:
            model->state.on_timer_enabled = false;
            break;
        case Daikin64TimerOffValue:
            model->state.off_timer_minutes =
                daikin64_next_timer_minutes(model->state.off_timer_minutes);
            *state_changed = true;
            return false;
        case Daikin64TimerOffSet:
            model->state.off_timer_enabled = true;
            break;
        case Daikin64TimerOffClear:
            model->state.off_timer_enabled = false;
            break;
        default:
            return false;
        }

        daikin64_prepare_setting_send(send_state, &model->state);
        *should_send = true;
        *state_changed = true;
        return true;
    }

    if(model->page == Daikin64PageFavorites) {
        if(model->selected < DAIKIN64_FAVORITE_COUNT) {
            if(long_press) {
                model->favorites[model->selected] = model->state;
                *state_changed = true;
                return false;
            }

            daikin64_apply_favorite(model, model->selected);
            daikin64_prepare_setting_send(send_state, &model->state);
            *should_send = true;
            *state_changed = true;
        }
        return true;
    }

    return true;
}

static bool daikin64_input_callback(InputEvent* event, void* context) {
    Daikin64App* app = context;
    Daikin64State send_state;
    bool should_send = false;
    bool power_toggle = false;
    bool notify_only = false;
    bool should_save = false;

    if((event->type != InputTypeShort) && (event->type != InputTypeLong)) return false;

    if(event->key == InputKeyBack) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if((event->type == InputTypeLong) && (event->key == InputKeyUp || event->key == InputKeyDown)) {
        with_view_model(
            app->view,
            Daikin64RemoteModel * model,
            {
                if(event->key == InputKeyUp && model->state.temperature < DAIKIN64_MAX_TEMP) {
                    model->state.temperature++;
                    daikin64_prepare_setting_send(&send_state, &model->state);
                    should_send = true;
                    should_save = true;
                } else if(event->key == InputKeyDown && model->state.temperature > DAIKIN64_MIN_TEMP) {
                    model->state.temperature--;
                    daikin64_prepare_setting_send(&send_state, &model->state);
                    should_send = true;
                    should_save = true;
                }
            },
            true);

        if(should_send) daikin64_send_with_feedback(app, &send_state, false);
        if(should_save) {
            with_view_model(
                app->view,
                Daikin64RemoteModel * model,
                {
                    daikin64_save_state(model);
                },
                false);
        }
        return true;
    }

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        with_view_model(
            app->view,
            Daikin64RemoteModel * model,
            {
                if(event->key == InputKeyLeft) {
                    model->page = (model->page + Daikin64PageCount - 1) % Daikin64PageCount;
                } else {
                    model->page = (model->page + 1) % Daikin64PageCount;
                }
                model->selected = 0;
            },
            true);
        return true;
    }

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        with_view_model(
            app->view,
            Daikin64RemoteModel * model,
            {
                uint8_t item_count = daikin64_page_item_count(model->page);
                if(event->key == InputKeyUp) {
                    model->selected = (model->selected + item_count - 1) % item_count;
                } else {
                    model->selected = (model->selected + 1) % item_count;
                }
            },
            true);
        return true;
    }

    if(event->key == InputKeyOk) {
        with_view_model(
            app->view,
            Daikin64RemoteModel * model,
            {
                bool handled = daikin64_handle_ok(
                    model,
                    &send_state,
                    &should_send,
                    &power_toggle,
                    &should_save,
                    event->type == InputTypeLong);
                notify_only = !handled || !should_send;
            },
            true);

        if(should_send) {
            daikin64_send_with_feedback(app, &send_state, power_toggle);
        } else if(notify_only) {
            daikin64_notify_only(app);
        }
        if(should_save) {
            with_view_model(
                app->view,
                Daikin64RemoteModel * model,
                {
                    daikin64_save_state(model);
                },
                false);
        }
        return true;
    }

    return true;
}

static bool daikin64_navigation_callback(void* context) {
    Daikin64App* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static void daikin64_init_favorites(Daikin64RemoteModel* model) {
    for(uint8_t i = 0; i < DAIKIN64_FAVORITE_COUNT; i++) {
        daikin64_init(&model->favorites[i]);
    }

    model->favorites[0].mode = Daikin64ModeCool;
    model->favorites[0].temperature = 27;
    model->favorites[0].fan = Daikin64FanAuto;

    model->favorites[1].mode = Daikin64ModeHeat;
    model->favorites[1].temperature = 25;
    model->favorites[1].fan = Daikin64FanLow;

    model->favorites[2].mode = Daikin64ModeDry;
    model->favorites[2].temperature = 26;
    model->favorites[2].fan = Daikin64FanAuto;

    model->favorites[3].mode = Daikin64ModeFan;
    model->favorites[3].temperature = 27;
    model->favorites[3].fan = Daikin64FanHigh;

    model->favorites[4].mode = Daikin64ModeAuto;
    model->favorites[4].temperature = 27;
    model->favorites[4].fan = Daikin64FanAuto;
}

static Daikin64App* daikin64_app_alloc(void) {
    Daikin64App* app = malloc(sizeof(Daikin64App));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();

    view_set_orientation(app->view, ViewOrientationVertical);
    view_set_context(app->view, app);
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(Daikin64RemoteModel));
    view_set_draw_callback(app->view, daikin64_draw_callback);
    view_set_input_callback(app->view, daikin64_input_callback);

    with_view_model(
        app->view,
        Daikin64RemoteModel * model,
        {
            daikin64_init(&model->state);
            model->state.on_timer_minutes = 14U * 60U;
            model->state.off_timer_minutes = 18U * 60U;
            model->page = Daikin64PageMain;
            model->selected = Daikin64MainSend;
            daikin64_init_favorites(model);
            daikin64_sync_indexes(model);
            daikin64_load_saved_state(model);
        },
        true);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, daikin64_navigation_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->view_dispatcher, Daikin64ViewRemote, app->view);

    return app;
}

static void daikin64_app_free(Daikin64App* app) {
    view_dispatcher_remove_view(app->view_dispatcher, Daikin64ViewRemote);
    view_free(app->view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t daikin64_ac_remote_app(void* p) {
    UNUSED(p);

    Daikin64App* app = daikin64_app_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, Daikin64ViewRemote);
    view_dispatcher_run(app->view_dispatcher);
    daikin64_app_free(app);

    return 0;
}
