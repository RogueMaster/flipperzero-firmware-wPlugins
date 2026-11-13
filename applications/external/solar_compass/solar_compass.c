/*
 * Solar Compass v1.0 - Created by Marcopiiit
 *
 * The Flipper has no magnetometer, so this is a SOLAR compass.
 * It uses the RTC time and the selected city (lat/lon) to compute the sun's
 * azimuth and elevation. Point the Flipper at the sun ("Sun up" mode) and the
 * dial shows where North is.
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
#include <math.h>
#include <stdio.h>

#define DEG2RAD (0.017453292f)
#define RAD2DEG (57.295780f)

/* ---------- City list ---------- */
typedef struct {
    const char* name;
    float lat;
    float lon;
} City;

static const City CITIES[] = {
    {"Taranto", 40.47f, 17.24f},
    {"Roma", 41.90f, 12.50f},
    {"Reykjavik", 64.15f, -21.94f},
    {"Svalbard", 78.22f, 15.63f},
    {"Moscow", 55.76f, 37.62f},
    {"Cairo", 30.04f, 31.24f},
    {"Nairobi", -1.29f, 36.82f},
    {"Cape Town", -33.92f, 18.42f},
    {"Dubai", 25.20f, 55.27f},
    {"Mumbai", 19.08f, 72.88f},
    {"Singapore", 1.35f, 103.82f},
    {"Beijing", 39.90f, 116.41f},
    {"Tokyo", 35.68f, 139.69f},
    {"Sydney", -33.87f, 151.21f},
    {"Auckland", -36.85f, 174.76f},
    {"Honolulu", 21.31f, -157.86f},
    {"Anchorage", 61.22f, -149.90f},
    {"Los Angeles", 34.05f, -118.24f},
    {"Mexico", 19.43f, -99.13f},
    {"New York", 40.71f, -74.01f},
    {"Lima", -12.05f, -77.04f},
    {"Rio", -22.91f, -43.17f},
    {"B. Aires", -34.60f, -58.38f},
    {"Ushuaia", -54.80f, -68.30f},
};
#define CITY_COUNT (sizeof(CITIES) / sizeof(CITIES[0]))

/* ---------- Types / state ---------- */
typedef enum {
    ViewMenu,
    ViewCompass,
    ViewSettings,
    ViewHelp,
    ViewAbout,
} ViewId;

typedef enum {
    MenuStart,
    MenuSettings,
    MenuHelp,
    MenuAbout,
} MenuIndex;

typedef enum {
    ModeSunUp,
    ModeNorthUp,
} CompassMode;

typedef enum {
    DstOff,
    DstOn,
    DstAutoEU,
} DstMode;

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* menu;
    View* compass_view;
    VariableItemList* settings;
    Widget* help;
    Widget* about;
    FuriTimer* timer;

    VariableItem* it_city;
    VariableItem* it_utc;
    VariableItem* it_dst;
    VariableItem* it_mode;

    uint8_t city;
    int8_t utc_offset; // offset of the Flipper clock from UTC
    DstMode dst;
    CompassMode mode;
} App;

typedef struct {
    App* app;
} CompassModel;

/* ---------- DST / sun position ---------- */
static int day_of_week(int y, int m, int d) { // 0 = Sunday
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if(m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static bool dst_active(const App* app, const DateTime* dt) {
    if(app->dst == DstOff) return false;
    if(app->dst == DstOn) return true;
    // Auto: EU rule (last Sunday of March -> last Sunday of October)
    if(dt->month < 3 || dt->month > 10) return false;
    if(dt->month > 3 && dt->month < 10) return true;
    int last_sun = 31 - day_of_week(dt->year, dt->month, 31);
    if(dt->month == 3) return (dt->day > last_sun) || (dt->day == last_sun && dt->hour >= 2);
    return (dt->day < last_sun) || (dt->day == last_sun && dt->hour < 3);
}

static void sun_position(
    const DateTime* dt,
    float utc_offset_h,
    float lat,
    float lon,
    float* az_out,
    float* alt_out) {
    int32_t a = (14 - dt->month) / 12;
    int32_t y = dt->year + 4800 - a;
    int32_t m = dt->month + 12 * a - 3;
    int32_t jdn = dt->day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;

    float hours = (float)dt->hour + (float)dt->minute / 60.0f + (float)dt->second / 3600.0f -
                  utc_offset_h;
    float frac = (hours - 12.0f) / 24.0f; // fraction of day since J2000
    float nint = (float)(jdn - 2451545); // whole days since J2000
    float n = nint + frac;

    // Large numbers split into integer part + fraction to keep float precision
    float L = fmodf(fmodf(280.460f + 0.9856474f * nint, 360.0f) + 0.9856474f * frac, 360.0f);
    float g = fmodf(fmodf(357.528f + 0.9856003f * nint, 360.0f) + 0.9856003f * frac, 360.0f) *
              DEG2RAD;
    float lam = (L + 1.915f * sinf(g) + 0.020f * sinf(2.0f * g)) * DEG2RAD;
    float eps = (23.439f - 0.0000004f * n) * DEG2RAD;

    float ra = atan2f(cosf(eps) * sinf(lam), cosf(lam)) * RAD2DEG;
    float dec = asinf(sinf(eps) * sinf(lam));

    float gmst = fmodf(
        fmodf(280.46061837f + 0.98564736629f * nint, 360.0f) + 360.98564736629f * frac, 360.0f);
    float ha = (gmst + lon - ra) * DEG2RAD;

    float latr = lat * DEG2RAD;
    float sin_alt = sinf(latr) * sinf(dec) + cosf(latr) * cosf(dec) * cosf(ha);
    if(sin_alt > 1.0f) sin_alt = 1.0f;
    if(sin_alt < -1.0f) sin_alt = -1.0f;
    float alt = asinf(sin_alt) * RAD2DEG;
    float az = atan2f(sinf(ha), cosf(ha) * sinf(latr) - tanf(dec) * cosf(latr)) * RAD2DEG + 180.0f;
    az = fmodf(az + 720.0f, 360.0f);

    *az_out = az;
    *alt_out = alt;
}

/* ---------- Persistent settings ---------- */
#define SETTINGS_PARENT_DIR "/ext/apps_data"
#define SETTINGS_DIR        "/ext/apps_data/solar_compass"
#define SETTINGS_FILE       "/ext/apps_data/solar_compass/settings.bin"
#define SETTINGS_MAGIC      0x53434D32

typedef struct {
    uint32_t magic;
    uint8_t city;
    int8_t utc_offset;
    uint8_t dst;
    uint8_t mode;
} SavedSettings;

static void settings_save(App* app) {
    SavedSettings data = {
        .magic = SETTINGS_MAGIC,
        .city = app->city,
        .utc_offset = app->utc_offset,
        .dst = (uint8_t)app->dst,
        .mode = (uint8_t)app->mode,
    };

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SETTINGS_PARENT_DIR);
    storage_simply_mkdir(storage, SETTINGS_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &data, sizeof(data));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void settings_load(App* app) {
    SavedSettings data;
    bool ok = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = (storage_file_read(file, &data, sizeof(data)) == sizeof(data));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    // Only accept sane values, otherwise keep the defaults
    if(ok && data.magic == SETTINGS_MAGIC && data.city < CITY_COUNT && data.utc_offset >= -12 &&
       data.utc_offset <= 14 && data.dst <= (uint8_t)DstAutoEU && data.mode <= (uint8_t)ModeNorthUp) {
        app->city = data.city;
        app->utc_offset = data.utc_offset;
        app->dst = (DstMode)data.dst;
        app->mode = (CompassMode)data.mode;
    }
}

/* ---------- Drawing ---------- */
static void polar(int cx, int cy, float r, float ang_deg, int* x, int* y) {
    float a = ang_deg * DEG2RAD;
    *x = cx + (int)lroundf(r * sinf(a));
    *y = cy - (int)lroundf(r * cosf(a));
}

static void fmt_deg(char* buf, size_t len, const char* prefix, float v) {
    int t = (int)lroundf(v * 10.0f);
    char sign = (t < 0) ? '-' : ' ';
    if(t < 0) t = -t;
    snprintf(buf, len, "%s%c%d.%d", prefix, sign, t / 10, t % 10);
}

static void compass_draw(Canvas* canvas, void* _model) {
    CompassModel* model = _model;
    App* app = model->app;
    const City* city = &CITIES[app->city];

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    float off = (float)app->utc_offset + (dst_active(app, &dt) ? 1.0f : 0.0f);

    float az, alt;
    sun_position(&dt, off, city->lat, city->lon, &az, &alt);

    const int cx = 30, cy = 32, r = 26;
    float north_ang = (app->mode == ModeSunUp) ? -az : 0.0f;
    float sun_ang = (app->mode == ModeSunUp) ? 0.0f : az;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_circle(canvas, cx, cy, r);

    // Ticks and cardinal letters
    static const char* lbl[4] = {"N", "E", "S", "W"};
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < 4; i++) {
        int x1, y1, x2, y2;
        polar(cx, cy, r, north_ang + 90.0f * (float)i, &x1, &y1);
        polar(cx, cy, r - 3, north_ang + 90.0f * (float)i, &x2, &y2);
        canvas_draw_line(canvas, x1, y1, x2, y2);
        polar(cx, cy, r - 8, north_ang + 90.0f * (float)i, &x1, &y1);
        canvas_draw_str_aligned(canvas, x1, y1, AlignCenter, AlignCenter, lbl[i]);
    }

    // Needle pointing North
    int nx, ny, sx, sy;
    polar(cx, cy, r - 14, north_ang, &nx, &ny);
    canvas_draw_line(canvas, cx, cy, nx, ny);
    canvas_draw_disc(canvas, cx, cy, 2);

    // Sun
    polar(cx, cy, r, sun_ang, &sx, &sy);
    if(alt > 0.0f) {
        canvas_draw_disc(canvas, sx, sy, 3);
        for(int i = 0; i < 8; i++) {
            int rx1, ry1, rx2, ry2;
            polar(sx, sy, 4, 45.0f * (float)i, &rx1, &ry1);
            polar(sx, sy, 6, 45.0f * (float)i, &rx2, &ry2);
            canvas_draw_line(canvas, rx1, ry1, rx2, ry2);
        }
    } else {
        canvas_draw_circle(canvas, sx, sy, 3);
    }

    // Text
    char buf[24];
    canvas_draw_str(canvas, 64, 9, city->name);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
    canvas_draw_str(canvas, 64, 20, buf);
    fmt_deg(buf, sizeof(buf), "Az", az);
    canvas_draw_str(canvas, 64, 31, buf);
    fmt_deg(buf, sizeof(buf), "El", alt);
    canvas_draw_str(canvas, 64, 42, buf);
    canvas_draw_str(canvas, 64, 53, (app->mode == ModeSunUp) ? "Sun up" : "North up");
    canvas_draw_str(canvas, 64, 63, (alt > 0.0f) ? "Day" : "Night");
}

static bool compass_input(InputEvent* event, void* ctx) {
    App* app = ctx;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool handled = true;
    switch(event->key) {
    case InputKeyLeft:
        app->city = (app->city + CITY_COUNT - 1) % CITY_COUNT;
        break;
    case InputKeyRight:
        app->city = (app->city + 1) % CITY_COUNT;
        break;
    case InputKeyOk:
        app->mode = (app->mode == ModeSunUp) ? ModeNorthUp : ModeSunUp;
        break;
    default:
        handled = false;
        break;
    }
    if(handled) {
        settings_save(app);
        with_view_model(app->compass_view, CompassModel * model, { UNUSED(model); }, true);
    }
    return handled;
}

static void compass_timer_cb(void* ctx) {
    App* app = ctx;
    with_view_model(app->compass_view, CompassModel * model, { UNUSED(model); }, true);
}

static void compass_enter(void* ctx) {
    App* app = ctx;
    furi_timer_start(app->timer, furi_ms_to_ticks(1000));
}

static void compass_exit(void* ctx) {
    App* app = ctx;
    furi_timer_stop(app->timer);
}

/* ---------- Settings ---------- */
static void set_utc_text(App* app) {
    char buf[8];
    snprintf(buf, sizeof(buf), "UTC%+d", app->utc_offset);
    variable_item_set_current_value_text(app->it_utc, buf);
}

static void set_dst_text(App* app) {
    static const char* t[] = {"Off", "On", "Auto EU"};
    variable_item_set_current_value_text(app->it_dst, t[app->dst]);
}

static void set_mode_text(App* app) {
    variable_item_set_current_value_text(
        app->it_mode, (app->mode == ModeSunUp) ? "Sun up" : "North up");
}

static void settings_sync(App* app) {
    variable_item_set_current_value_index(app->it_city, app->city);
    variable_item_set_current_value_text(app->it_city, CITIES[app->city].name);
    variable_item_set_current_value_index(app->it_utc, app->utc_offset + 12);
    set_utc_text(app);
    variable_item_set_current_value_index(app->it_dst, app->dst);
    set_dst_text(app);
    variable_item_set_current_value_index(app->it_mode, app->mode);
    set_mode_text(app);
}

static void city_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->city = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, CITIES[app->city].name);
    settings_save(app);
}

static void utc_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->utc_offset = (int8_t)variable_item_get_current_value_index(item) - 12;
    set_utc_text(app);
    settings_save(app);
}

static void dst_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->dst = variable_item_get_current_value_index(item);
    set_dst_text(app);
    settings_save(app);
}

static void mode_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->mode = variable_item_get_current_value_index(item);
    set_mode_text(app);
    settings_save(app);
}

/* ---------- Navigation ---------- */
static uint32_t nav_to_menu(void* ctx) {
    UNUSED(ctx);
    return ViewMenu;
}

static uint32_t nav_exit(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

static void menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    switch(index) {
    case MenuStart:
        view_dispatcher_switch_to_view(app->vd, ViewCompass);
        break;
    case MenuSettings:
        settings_sync(app);
        view_dispatcher_switch_to_view(app->vd, ViewSettings);
        break;
    case MenuHelp:
        view_dispatcher_switch_to_view(app->vd, ViewHelp);
        break;
    case MenuAbout:
        view_dispatcher_switch_to_view(app->vd, ViewAbout);
        break;
    }
}

/* ---------- Alloc / free ---------- */
static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->city = 0; // Taranto
    app->utc_offset = 1;
    app->dst = DstAutoEU;
    app->mode = ModeSunUp;
    settings_load(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    // Main menu
    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "Solar Compass");
    submenu_add_item(app->menu, "Start", MenuStart, menu_cb, app);
    submenu_add_item(app->menu, "Settings", MenuSettings, menu_cb, app);
    submenu_add_item(app->menu, "How to use", MenuHelp, menu_cb, app);
    submenu_add_item(app->menu, "About", MenuAbout, menu_cb, app);
    view_set_previous_callback(submenu_get_view(app->menu), nav_exit);
    view_dispatcher_add_view(app->vd, ViewMenu, submenu_get_view(app->menu));

    // Compass view
    app->compass_view = view_alloc();
    view_set_context(app->compass_view, app);
    view_allocate_model(app->compass_view, ViewModelTypeLockFree, sizeof(CompassModel));
    with_view_model(app->compass_view, CompassModel * model, { model->app = app; }, false);
    view_set_draw_callback(app->compass_view, compass_draw);
    view_set_input_callback(app->compass_view, compass_input);
    view_set_enter_callback(app->compass_view, compass_enter);
    view_set_exit_callback(app->compass_view, compass_exit);
    view_set_previous_callback(app->compass_view, nav_to_menu);
    view_dispatcher_add_view(app->vd, ViewCompass, app->compass_view);
    app->timer = furi_timer_alloc(compass_timer_cb, FuriTimerTypePeriodic, app);

    // Settings
    app->settings = variable_item_list_alloc();
    app->it_city = variable_item_list_add(app->settings, "City", CITY_COUNT, city_changed, app);
    app->it_utc = variable_item_list_add(app->settings, "Clock", 27, utc_changed, app);
    app->it_dst = variable_item_list_add(app->settings, "DST", 3, dst_changed, app);
    app->it_mode = variable_item_list_add(app->settings, "Mode", 2, mode_changed, app);
    settings_sync(app);
    view_set_previous_callback(variable_item_list_get_view(app->settings), nav_to_menu);
    view_dispatcher_add_view(app->vd, ViewSettings, variable_item_list_get_view(app->settings));

    // How to use (scrollable text)
    app->help = widget_alloc();
    widget_add_text_scroll_element(
        app->help,
        0,
        0,
        128,
        64,
        "\e#How to use\e#\n"
        "The Flipper has no magnetic sensor, so this is a SOLAR compass: it "
        "computes where the sun is from the Flipper clock and the selected "
        "city.\n\n"
        "\e#1. Setup\e#\n"
        "In Settings set the city, then Clock to your Flipper's UTC offset "
        "(Italy = UTC+1) and DST (Auto EU is fine for Europe).\n\n"
        "\e#2. Start\e#\n"
        "Choose Start. The dial shows the sun and North, plus time, "
        "azimuth (Az) and elevation (El).\n\n"
        "\e#3. Find North\e#\n"
        "Use mode Sun up: hold the Flipper flat and rotate until the sun "
        "marker at the top of the dial points at the real sun (never look "
        "straight at it; use the shadow of a stick instead). The N on the "
        "dial now shows the real North.\n\n"
        "\e#Modes (press OK)\e#\n"
        "Sun up: the sun marker stays at the top and the N turns around the "
        "dial. Use it outdoors to find North.\n"
        "North up: the N stays fixed at the top and the sun marker moves "
        "around the dial at its real compass bearing (Az). Use it when you "
        "already know where North is, to see where the sun is now, or to "
        "check the sunrise and sunset direction in any city. If the marker "
        "does not match the real sun, check city, Clock and DST.\n\n"
        "\e#Controls\e#\n"
        "Left/Right: change city\n"
        "OK: switch Sun up / North up\n"
        "Back: return to menu\n\n"
        "\e#Notes\e#\n"
        "Works only when the sun is up (Day). At Night the marker is hollow "
        "and the result is not usable. The Flipper clock must be correct.\n"
        "All settings are saved automatically.");
    view_set_previous_callback(widget_get_view(app->help), nav_to_menu);
    view_dispatcher_add_view(app->vd, ViewHelp, widget_get_view(app->help));

    // About
    app->about = widget_alloc();
    widget_add_text_box_element(
        app->about,
        0,
        0,
        128,
        64,
        AlignCenter,
        AlignCenter,
        "\e#Solar Compass\e#\n\e#v1.0\e#\nCreated by Marcopiiit",
        false);
    view_set_previous_callback(widget_get_view(app->about), nav_to_menu);
    view_dispatcher_add_view(app->vd, ViewAbout, widget_get_view(app->about));

    return app;
}

static void app_free(App* app) {
    settings_save(app);
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    view_dispatcher_remove_view(app->vd, ViewMenu);
    view_dispatcher_remove_view(app->vd, ViewCompass);
    view_dispatcher_remove_view(app->vd, ViewSettings);
    view_dispatcher_remove_view(app->vd, ViewHelp);
    view_dispatcher_remove_view(app->vd, ViewAbout);

    submenu_free(app->menu);
    view_free(app->compass_view);
    variable_item_list_free(app->settings);
    widget_free(app->help);
    widget_free(app->about);
    view_dispatcher_free(app->vd);

    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t solar_compass_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_switch_to_view(app->vd, ViewMenu);
    view_dispatcher_run(app->vd);
    app_free(app);
    return 0;
}
