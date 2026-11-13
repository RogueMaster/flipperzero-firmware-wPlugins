/*
 * FuseZero v1.0 - Designed by Marcopiiit
 * World clock for Flipper Zero (Momentum firmware).
 * Shows up to 4 cities at once, with the hour difference from your home zone.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_ABOUT_TEXT "FuseZero v1.0\nDesigned by Marcopiiit"
#define MENU_TITLE     "FuseZero by Marcopiiit"

#define MAX_CLOCKS       4
#define SETTINGS_VERSION 1
#define SETTINGS_DIR     EXT_PATH("apps_data/fusezero")
#define SETTINGS_FILE    EXT_PATH("apps_data/fusezero/settings.bin")

/* ------------------------------------------------------------------ */
/*  Time utilities (self-contained, no dependency on firmware helpers) */
/* ------------------------------------------------------------------ */

typedef struct {
    int year;
    int month; // 1..12
    int day;
    int hour;
    int minute;
    int second;
    int weekday; // 0 = Sunday
} LocalTime;

static int64_t floor_div(int64_t a, int64_t b) {
    int64_t q = a / b;
    if((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

static int64_t days_from_civil(int y, int m, int d) {
    y -= (m <= 2);
    const int64_t era = floor_div(y, 400);
    const int yoe = (int)(y - era * 400);
    const int mp = m + (m > 2 ? -3 : 9);
    const int doy = (153 * mp + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(int64_t z, int* y, int* m, int* d) {
    z += 719468;
    const int64_t era = floor_div(z, 146097);
    const int doe = (int)(z - era * 146097);
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

static int weekday_from_days(int64_t days) {
    // 1970-01-01 was a Thursday (4)
    return (int)(((days % 7) + 11) % 7);
}

static void epoch_to_local(int64_t t, LocalTime* o) {
    const int64_t days = floor_div(t, 86400);
    const int64_t sec = t - days * 86400;
    civil_from_days(days, &o->year, &o->month, &o->day);
    o->hour = (int)(sec / 3600);
    o->minute = (int)((sec % 3600) / 60);
    o->second = (int)(sec % 60);
    o->weekday = weekday_from_days(days);
}

static int days_in_month(int y, int m) {
    static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) return 29;
    return d[m - 1];
}

/* week: 1..4 = nth Sunday, 5 = last Sunday */
static int nth_sunday(int y, int m, int week) {
    const int wd1 = weekday_from_days(days_from_civil(y, m, 1));
    int first_sun = 1 + (7 - wd1) % 7;
    if(week < 5) return first_sun + 7 * (week - 1);
    int d = first_sun;
    while(d + 7 <= days_in_month(y, m)) d += 7;
    return d;
}

/* ------------------------------------------------------------------ */
/*  Daylight saving rules                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    DstNone,
    DstEU, // last Sun Mar 01:00 UTC -> last Sun Oct 01:00 UTC
    DstUS, // 2nd Sun Mar 02:00 -> 1st Sun Nov 02:00 (local)
    DstAU, // 1st Sun Oct 02:00 -> 1st Sun Apr 03:00 (local)
    DstNZ, // last Sun Sep 02:00 -> 1st Sun Apr 03:00 (local)
} DstRuleId;

typedef struct {
    uint8_t sm, sw, sh; // start month, week, hour (standard time, or UTC if utc=true)
    uint8_t em, ew, eh; // end month, week, hour (standard time, or UTC if utc=true)
    bool utc;
} DstRule;

static const DstRule dst_rules[] = {
    [DstNone] = {0, 0, 0, 0, 0, 0, false},
    [DstEU] = {3, 5, 1, 10, 5, 1, true},
    [DstUS] = {3, 2, 2, 11, 1, 1, false},
    [DstAU] = {10, 1, 2, 4, 1, 2, false},
    [DstNZ] = {9, 5, 2, 4, 1, 2, false},
};

/* ------------------------------------------------------------------ */
/*  City database (sorted by UTC offset, like the Windows list)        */
/* ------------------------------------------------------------------ */

typedef struct {
    const char* name; // max 10 chars so it fits in a small box
    int16_t off; // standard UTC offset in minutes
    uint8_t rule;
} City;

static const City cities[] = {
    {"Honolulu", -600, DstNone},
    {"Anchorage", -540, DstUS},
    {"L. Angeles", -480, DstUS},
    {"Vancouver", -480, DstUS},
    {"Denver", -420, DstUS},
    {"Phoenix", -420, DstNone},
    {"Chicago", -360, DstUS},
    {"Mexico", -360, DstNone},
    {"New York", -300, DstUS},
    {"Toronto", -300, DstUS},
    {"Bogota", -300, DstNone},
    {"Lima", -300, DstNone},
    {"Caracas", -240, DstNone},
    {"Sao Paulo", -180, DstNone},
    {"B. Aires", -180, DstNone},
    {"Reykjavik", 0, DstNone},
    {"London", 0, DstEU},
    {"Lisbon", 0, DstEU},
    {"Dublin", 0, DstEU},
    {"Casablanca", 60, DstNone},
    {"Lagos", 60, DstNone},
    {"Madrid", 60, DstEU},
    {"Paris", 60, DstEU},
    {"Rome", 60, DstEU},
    {"Berlin", 60, DstEU},
    {"Amsterdam", 60, DstEU},
    {"Brussels", 60, DstEU},
    {"Vienna", 60, DstEU},
    {"Zurich", 60, DstEU},
    {"Stockholm", 60, DstEU},
    {"Warsaw", 60, DstEU},
    {"Prague", 60, DstEU},
    {"Athens", 120, DstEU},
    {"Helsinki", 120, DstEU},
    {"Kyiv", 120, DstEU},
    {"Bucharest", 120, DstEU},
    {"Joburg", 120, DstNone},
    {"Istanbul", 180, DstNone},
    {"Moscow", 180, DstNone},
    {"Nairobi", 180, DstNone},
    {"Riyadh", 180, DstNone},
    {"Dubai", 240, DstNone},
    {"Karachi", 300, DstNone},
    {"Delhi", 330, DstNone},
    {"Dhaka", 360, DstNone},
    {"Bangkok", 420, DstNone},
    {"Jakarta", 420, DstNone},
    {"Beijing", 480, DstNone},
    {"Hong Kong", 480, DstNone},
    {"Singapore", 480, DstNone},
    {"Perth", 480, DstNone},
    {"Manila", 480, DstNone},
    {"Taipei", 480, DstNone},
    {"Tokyo", 540, DstNone},
    {"Seoul", 540, DstNone},
    {"Brisbane", 600, DstNone},
    {"Sydney", 600, DstAU},
    {"Melbourne", 600, DstAU},
    {"Auckland", 720, DstNZ},
};

#define CITY_COUNT ((uint8_t)(sizeof(cities) / sizeof(cities[0])))

static uint8_t city_find(const char* name, uint8_t fallback) {
    for(uint8_t i = 0; i < CITY_COUNT; i++) {
        if(strcmp(cities[i].name, name) == 0) return i;
    }
    return fallback;
}

static int64_t transition_utc(int y, int m, int week, int hour, bool utc_based, int std_off) {
    int64_t t = days_from_civil(y, m, nth_sunday(y, m, week)) * 86400 + hour * 3600;
    if(!utc_based) t -= (int64_t)std_off * 60;
    return t;
}

static bool city_is_dst(const City* c, int64_t utc) {
    if(c->rule == DstNone) return false;
    const DstRule* r = &dst_rules[c->rule];
    LocalTime u;
    epoch_to_local(utc, &u);
    const int64_t s = transition_utc(u.year, r->sm, r->sw, r->sh, r->utc, c->off);
    const int64_t e = transition_utc(u.year, r->em, r->ew, r->eh, r->utc, c->off);
    if(r->sm < r->em) return utc >= s && utc < e; // northern hemisphere
    return utc >= s || utc < e; // southern hemisphere
}

/* Current offset (minutes) including DST */
static int city_offset_now(const City* c, int64_t utc) {
    return c->off + (city_is_dst(c, utc) ? 60 : 0);
}

/* ------------------------------------------------------------------ */
/*  Settings                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t version;
    uint8_t count; // 1..4 visible clocks
    uint8_t city[MAX_CLOCKS];
    uint8_t home; // zone the Flipper RTC is set to
    uint8_t h24; // 1 = 24h, 0 = 12h
} Settings;

static void settings_defaults(Settings* s) {
    s->version = SETTINGS_VERSION;
    s->count = 4;
    s->city[0] = city_find("New York", 0);
    s->city[1] = city_find("London", 0);
    s->city[2] = city_find("Tokyo", 0);
    s->city[3] = city_find("Sydney", 0);
    s->home = city_find("Rome", 0);
    s->h24 = 1;
}

static bool settings_valid(const Settings* s) {
    if(s->version != SETTINGS_VERSION) return false;
    if(s->count < 1 || s->count > MAX_CLOCKS) return false;
    if(s->home >= CITY_COUNT) return false;
    for(int i = 0; i < MAX_CLOCKS; i++) {
        if(s->city[i] >= CITY_COUNT) return false;
    }
    return true;
}

static void settings_load(Settings* s) {
    settings_defaults(s);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        Settings tmp;
        if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp) && settings_valid(&tmp)) {
            *s = tmp;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void settings_save(const Settings* s) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SETTINGS_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, s, sizeof(Settings));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ------------------------------------------------------------------ */
/*  App structures                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    ViewMenu = 0,
    ViewClock,
    ViewSettings,
    ViewAbout,
} FuseZeroView;

typedef struct FuseZeroApp FuseZeroApp;

typedef struct {
    FuseZeroApp* app;
    uint8_t slot; // 0..3 = city slots, 4 = home zone
} SlotCtx;

struct FuseZeroApp {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* menu;
    VariableItemList* list;
    Widget* about;
    View* clock;
    FuriTimer* timer;
    Settings settings;
    SlotCtx slots[MAX_CLOCKS + 1];
};

typedef struct {
    Settings s;
} ClockModel;

/* ------------------------------------------------------------------ */
/*  Clock view                                                         */
/* ------------------------------------------------------------------ */

static const char* const wd_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* const mon_names[] =
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* UTC now, derived from the Flipper RTC (which holds the local time of the "home" zone) */
static int64_t current_utc(const Settings* s) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    const int64_t local = days_from_civil(dt.year, dt.month, dt.day) * 86400 + dt.hour * 3600 +
                          dt.minute * 60 + dt.second;
    const City* h = &cities[s->home];
    const int64_t guess = local - (int64_t)h->off * 60;
    const int off = h->off + (city_is_dst(h, guess) ? 60 : 0);
    return local - (int64_t)off * 60;
}

static void fmt_diff(char* buf, size_t n, int diff_min) {
    if(diff_min == 0) {
        snprintf(buf, n, "0h");
        return;
    }
    const char sign = diff_min > 0 ? '+' : '-';
    const int a = abs(diff_min);
    if(a % 60) {
        snprintf(buf, n, "%c%dh%02d", sign, a / 60, a % 60);
    } else {
        snprintf(buf, n, "%c%dh", sign, a / 60);
    }
}

static void get_rect(uint8_t count, uint8_t i, int* x, int* y, int* w, int* h) {
    switch(count) {
    case 1:
        *x = 0;
        *y = 0;
        *w = 128;
        *h = 64;
        break;
    case 2:
        *x = 0;
        *y = i * 32;
        *w = 128;
        *h = 32;
        break;
    case 3:
        if(i < 2) {
            *x = i * 64;
            *y = 0;
            *w = 64;
            *h = 32;
        } else {
            *x = 0;
            *y = 32;
            *w = 128;
            *h = 32;
        }
        break;
    default:
        *x = (i % 2) * 64;
        *y = (i / 2) * 32;
        *w = 64;
        *h = 32;
        break;
    }
}

static void draw_clock_box(
    Canvas* c,
    int x,
    int y,
    int w,
    int h,
    const City* city,
    int64_t utc,
    int home_off,
    bool h24) {
    const int off = city_offset_now(city, utc);
    LocalTime t;
    epoch_to_local(utc + (int64_t)off * 60, &t);

    char tstr[16];
    const char* ampm = "";
    int hr = t.hour;
    if(h24) {
        snprintf(tstr, sizeof(tstr), "%02d:%02d", hr, t.minute);
    } else {
        ampm = hr >= 12 ? "PM" : "AM";
        hr %= 12;
        if(hr == 0) hr = 12;
        snprintf(tstr, sizeof(tstr), "%d:%02d", hr, t.minute);
    }

    char diff[12];
    fmt_diff(diff, sizeof(diff), off - home_off);

    char line[32];
    const bool full = (w == 128 && h == 64);
    const bool wide = (w == 128 && h == 32);

    if(full) {
        snprintf(line, sizeof(line), "%s  %s %d %s", diff, wd_names[t.weekday], t.day, mon_names[t.month - 1]);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, x + 64, y + 2, AlignCenter, AlignTop, city->name);

        canvas_set_font(c, FontBigNumbers);
        const int tw = canvas_string_width(c, tstr);
        const int tx = x + (128 - tw - 16) / 2;
        canvas_draw_str(c, tx, y + 37, tstr);

        canvas_set_font(c, FontSecondary);
        char sec[4];
        snprintf(sec, sizeof(sec), "%02d", t.second);
        canvas_draw_str(c, tx + tw + 3, y + 37, sec);
        if(!h24) canvas_draw_str(c, tx + tw + 3, y + 26, ampm);
        canvas_draw_str_aligned(c, x + 64, y + 47, AlignCenter, AlignTop, line);
    } else if(wide) {
        snprintf(line, sizeof(line), "%s %s", diff, wd_names[t.weekday]);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, x + 4, y + 13, city->name);
        canvas_draw_str(c, x + 4, y + 26, line);

        canvas_set_font(c, FontBigNumbers);
        const int tw = canvas_string_width(c, tstr);
        const int right = x + w - 4 - (h24 ? 0 : 14);
        canvas_draw_str(c, right - tw, y + 23, tstr);
        if(!h24) {
            canvas_set_font(c, FontSecondary);
            canvas_draw_str(c, right + 2, y + 23, ampm);
        }
    } else {
        char tfull[20];
        snprintf(tfull, sizeof(tfull), "%s%s%s", tstr, h24 ? "" : " ", ampm);
        snprintf(line, sizeof(line), "%s %s", diff, wd_names[t.weekday]);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, x + 3, y + 9, city->name);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, x + 3, y + 21, tfull);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, x + 3, y + 30, line);
    }

    // separators
    if(y > 0) canvas_draw_line(c, x, y, x + w - 1, y);
    if(x > 0) canvas_draw_line(c, x, y, x, y + h - 1);
}

static void clock_draw(Canvas* canvas, void* model) {
    const ClockModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    const int64_t utc = current_utc(&m->s);
    const int home_off = city_offset_now(&cities[m->s.home], utc);

    for(uint8_t i = 0; i < m->s.count; i++) {
        int x, y, w, h;
        get_rect(m->s.count, i, &x, &y, &w, &h);
        draw_clock_box(canvas, x, y, w, h, &cities[m->s.city[i]], utc, home_off, m->s.h24);
    }
}

static bool clock_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // Back is handled by the previous-callback
}

static void clock_enter(void* context) {
    FuseZeroApp* app = context;
    settings_save(&app->settings);
    with_view_model(
        app->clock, ClockModel * model, { model->s = app->settings; }, true);
    furi_timer_start(app->timer, furi_ms_to_ticks(1000));
}

static void clock_exit(void* context) {
    FuseZeroApp* app = context;
    furi_timer_stop(app->timer);
}

static void tick_callback(void* context) {
    FuseZeroApp* app = context;
    with_view_model(
        app->clock, ClockModel * model, { UNUSED(model); }, true);
}

/* ------------------------------------------------------------------ */
/*  Settings view                                                      */
/* ------------------------------------------------------------------ */

static const char* const count_txt[] = {"1", "2", "3", "4"};
static const char* const city_labels[] = {"City 1", "City 2", "City 3", "City 4"};
static const char* const fmt_txt[] = {"12h", "24h"};

static void count_changed(VariableItem* item) {
    FuseZeroApp* app = variable_item_get_context(item);
    const uint8_t i = variable_item_get_current_value_index(item);
    app->settings.count = i + 1;
    variable_item_set_current_value_text(item, count_txt[i]);
}

static void city_changed(VariableItem* item) {
    SlotCtx* sc = variable_item_get_context(item);
    const uint8_t i = variable_item_get_current_value_index(item);
    if(sc->slot < MAX_CLOCKS) {
        sc->app->settings.city[sc->slot] = i;
    } else {
        sc->app->settings.home = i;
    }
    variable_item_set_current_value_text(item, cities[i].name);
}

static void format_changed(VariableItem* item) {
    FuseZeroApp* app = variable_item_get_context(item);
    const uint8_t i = variable_item_get_current_value_index(item);
    app->settings.h24 = i;
    variable_item_set_current_value_text(item, fmt_txt[i]);
}

static void settings_build(FuseZeroApp* app) {
    VariableItem* item;

    item = variable_item_list_add(app->list, "Clocks", MAX_CLOCKS, count_changed, app);
    variable_item_set_current_value_index(item, app->settings.count - 1);
    variable_item_set_current_value_text(item, count_txt[app->settings.count - 1]);

    for(uint8_t i = 0; i < MAX_CLOCKS; i++) {
        app->slots[i].app = app;
        app->slots[i].slot = i;
        item = variable_item_list_add(
            app->list, city_labels[i], CITY_COUNT, city_changed, &app->slots[i]);
        variable_item_set_current_value_index(item, app->settings.city[i]);
        variable_item_set_current_value_text(item, cities[app->settings.city[i]].name);
    }

    app->slots[MAX_CLOCKS].app = app;
    app->slots[MAX_CLOCKS].slot = MAX_CLOCKS;
    item = variable_item_list_add(
        app->list, "Home (RTC)", CITY_COUNT, city_changed, &app->slots[MAX_CLOCKS]);
    variable_item_set_current_value_index(item, app->settings.home);
    variable_item_set_current_value_text(item, cities[app->settings.home].name);

    item = variable_item_list_add(app->list, "Format", 2, format_changed, app);
    variable_item_set_current_value_index(item, app->settings.h24);
    variable_item_set_current_value_text(item, fmt_txt[app->settings.h24]);
}

/* ------------------------------------------------------------------ */
/*  Navigation                                                         */
/* ------------------------------------------------------------------ */

static void menu_callback(void* context, uint32_t index) {
    FuseZeroApp* app = context;
    view_dispatcher_switch_to_view(app->vd, index);
}

static uint32_t nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t nav_to_menu(void* context) {
    UNUSED(context);
    return ViewMenu;
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                        */
/* ------------------------------------------------------------------ */

int32_t fusezero_app(void* p) {
    UNUSED(p);

    FuseZeroApp* app = malloc(sizeof(FuseZeroApp));
    memset(app, 0, sizeof(FuseZeroApp));
    settings_load(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();

    // Main menu
    app->menu = submenu_alloc();
    submenu_set_header(app->menu, MENU_TITLE);
    submenu_add_item(app->menu, "View", ViewClock, menu_callback, app);
    submenu_add_item(app->menu, "Settings", ViewSettings, menu_callback, app);
    submenu_add_item(app->menu, "About", ViewAbout, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->menu), nav_exit);
    view_dispatcher_add_view(app->vd, ViewMenu, submenu_get_view(app->menu));

    // Clock view
    app->clock = view_alloc();
    view_allocate_model(app->clock, ViewModelTypeLocking, sizeof(ClockModel));
    view_set_context(app->clock, app);
    view_set_draw_callback(app->clock, clock_draw);
    view_set_input_callback(app->clock, clock_input);
    view_set_previous_callback(app->clock, nav_to_menu);
    view_set_enter_callback(app->clock, clock_enter);
    view_set_exit_callback(app->clock, clock_exit);
    view_dispatcher_add_view(app->vd, ViewClock, app->clock);
    app->timer = furi_timer_alloc(tick_callback, FuriTimerTypePeriodic, app);

    // Settings view
    app->list = variable_item_list_alloc();
    settings_build(app);
    view_set_previous_callback(variable_item_list_get_view(app->list), nav_to_menu);
    view_dispatcher_add_view(app->vd, ViewSettings, variable_item_list_get_view(app->list));

    // About view
    app->about = widget_alloc();
    widget_add_string_multiline_element(
        app->about, 64, 32, AlignCenter, AlignCenter, FontPrimary, APP_ABOUT_TEXT);
    view_set_previous_callback(widget_get_view(app->about), nav_to_menu);
    view_dispatcher_add_view(app->vd, ViewAbout, widget_get_view(app->about));

    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->vd, ViewMenu);
    view_dispatcher_run(app->vd);

    // Cleanup
    settings_save(&app->settings);

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    view_dispatcher_remove_view(app->vd, ViewMenu);
    view_dispatcher_remove_view(app->vd, ViewClock);
    view_dispatcher_remove_view(app->vd, ViewSettings);
    view_dispatcher_remove_view(app->vd, ViewAbout);

    submenu_free(app->menu);
    view_free(app->clock);
    variable_item_list_free(app->list);
    widget_free(app->about);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
