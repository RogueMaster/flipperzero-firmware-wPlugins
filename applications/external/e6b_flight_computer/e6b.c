// E6B Flight Computer v1.0 - created by Marcopiiit
// Flipper Zero / Momentum (dev). Integer inputs, float results.
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <math.h>
#include <stdio.h>

#define RAD(x) ((x) * 0.017453292519943295)
#define DEG(x) ((x) * 57.29577951308232)
#define ERR(o, m) furi_string_cat_str(o, "! " m "\n")

static int D = 1; // decimals
static bool HAPTIC = true;

static float nrm(float a) {
    a = fmodf(a, 360.0);
    return a < 0 ? a + 360.0 : a;
}
static void P(FuriString* o, const char* l, float v, const char* u) {
    furi_string_cat_printf(o, "%s: %.*f %s\n", l, D, (double)v, u);
}
static void TM(FuriString* o, const char* l, float min) {
    int t = (int)(fabsf(min) + 0.5);
    furi_string_cat_printf(o, "%s: %s%dh%02dm\n", l, min < 0 ? "-" : "", t / 60, t % 60);
}
static void conv2(FuriString* o, int32_t v, const char* a, const char* b, float k) {
    furi_string_cat_printf(
        o, "%ld %s = %.*f %s\n%ld %s = %.*f %s\n", (long)v, a, D, (double)(v * k), b, (long)v, b, D, (double)(v / k), a);
}

/* ---------- Wind ---------- */
static void c_wt(const int32_t* v, FuriString* o) {
    float tc = v[0], tas = v[1], wd = v[2], ws = v[3];
    if(tas <= 0) { ERR(o, "TAS must be > 0"); return; }
    float a = RAD(wd - tc), x = ws * sinf(a) / tas;
    if(fabsf(x) > 1) { ERR(o, "Wind too strong"); return; }
    float w = asinf(x);
    P(o, "WCA", DEG(w), "deg");
    P(o, "True HDG", nrm(tc + DEG(w)), "deg");
    P(o, "GS", tas * cosf(w) - ws * cosf(a), "kt");
}
static void c_wc(const int32_t* v, FuriString* o) {
    float a = RAD(v[1] - v[0]), h = v[2] * cosf(a), c = v[2] * sinf(a);
    P(o, h >= 0 ? "Headwind" : "Tailwind", fabsf(h), "kt");
    P(o, c >= 0 ? "Xwind from R" : "Xwind from L", fabsf(c), "kt");
}
static void c_fw(const int32_t* v, FuriString* o) {
    float wx = v[1] * sinf(RAD(v[0])) - v[3] * sinf(RAD(v[2]));
    float wy = v[1] * cosf(RAD(v[0])) - v[3] * cosf(RAD(v[2]));
    P(o, "Wind from", nrm(DEG(atan2f(wx, wy)) + 180), "deg");
    P(o, "Wind speed", hypotf(wx, wy), "kt");
}
static void c_160(const int32_t* v, FuriString* o) {
    if(v[1] <= 0 || v[2] <= 0) { ERR(o, "Distances must be > 0"); return; }
    float e = DEG(atanf((float)v[0] / v[1])), c = DEG(atanf((float)v[0] / v[2]));
    P(o, "Track error", e, "deg");
    P(o, "Closing angle", c, "deg");
    P(o, "Total correction", e + c, "deg");
}
/* ---------- Navigation ---------- */
static void c_mv(const int32_t* v, FuriString* o) {
    float mc = nrm(v[0] - v[1]);
    P(o, "Mag course", mc, "deg");
    P(o, "Compass HDG", nrm(mc - v[2]), "deg");
}
static void c_pnr(const int32_t* v, FuriString* o) {
    if(v[1] + v[2] <= 0 || v[1] <= 0) { ERR(o, "GS must be > 0"); return; }
    float d = (v[0] / 60.0) * v[1] * v[2] / (v[1] + v[2]);
    P(o, "PNR dist", d, "NM");
    TM(o, "PNR time", d / v[1] * 60);
}
static void c_etp(const int32_t* v, FuriString* o) {
    if(v[1] + v[2] <= 0 || v[1] <= 0) { ERR(o, "GS must be > 0"); return; }
    float x = (float)v[0] * v[2] / (v[1] + v[2]);
    P(o, "ETP dist", x, "NM");
    TM(o, "ETP time", x / v[1] * 60);
}
/* ---------- Time / Speed / Distance ---------- */
static void c_std(const int32_t* v, FuriString* o) {
    int z = (v[0] == 0) + (v[1] == 0) + (v[2] == 0);
    if(z != 1) { ERR(o, "Set ONE field to 0"); return; }
    if(v[0] == 0) P(o, "GS", v[2] / (float)v[1] * 60, "kt");
    else if(v[1] == 0) TM(o, "Time", v[2] / (float)v[0] * 60);
    else P(o, "Dist", v[0] * v[1] / 60.0, "NM");
}
static void c_eta(const int32_t* v, FuriString* o) {
    if(v[2] <= 0) { ERR(o, "GS must be > 0"); return; }
    float ete = v[1] / (float)v[2] * 60;
    int m = (int)((v[0] / 100) * 60 + v[0] % 100 + ete + 0.5);
    m = ((m % 1440) + 1440) % 1440;
    TM(o, "ETE", ete);
    furi_string_cat_printf(o, "ETA: %02d:%02d\n", m / 60, m % 60);
}
/* ---------- Fuel ---------- */
static void c_fuel(const int32_t* v, FuriString* o) {
    int z = (v[0] == 0) + (v[1] == 0) + (v[2] == 0);
    if(z != 1) { ERR(o, "Set ONE field to 0"); return; }
    if(v[0] == 0) P(o, "Flow", v[2] / (float)v[1] * 60, "/h");
    else if(v[1] == 0) TM(o, "Endurance", v[2] / (float)v[0] * 60);
    else P(o, "Fuel", v[0] * v[1] / 60.0, "");
}
static void c_fw2(const int32_t* v, FuriString* o) {
    float kg = v[0] * v[1] / 1000.0;
    P(o, "Weight", kg, "kg");
    P(o, "Weight", kg * 2.20462, "lb");
}
static void c_rng(const int32_t* v, FuriString* o) {
    if(v[2] <= 0) { ERR(o, "Flow must be > 0"); return; }
    float h = (v[0] - v[1]) / (float)v[2];
    if(h < 0) { ERR(o, "Reserve > fuel"); return; }
    TM(o, "Usable endur.", h * 60);
    P(o, "Max range", h * v[3], "NM");
}
/* ---------- Altitude / speed ---------- */
static void c_pa(const int32_t* v, FuriString* o) {
    P(o, "Press alt", v[0] + (1013.25 - v[1]) * 27, "ft");
}
static void c_da(const int32_t* v, FuriString* o) {
    float isa = 15 - 2 * v[0] / 1000.0;
    P(o, "ISA temp", isa, "C");
    P(o, "ISA dev", v[1] - isa, "C");
    P(o, "Density alt", v[0] + 120 * (v[1] - isa), "ft");
}
static void c_tas(const int32_t* v, FuriString* o) {
    float isa = 15 - 2 * v[1] / 1000.0, da = v[1] + 120 * (v[2] - isa);
    float sg = powf(1 - 6.8756e-6 * da, 4.2559), tas = v[0] / sqrtf(sg);
    float a = 38.967854 * sqrtf(v[2] + 273.15);
    P(o, "TAS", tas, "kt");
    P(o, "Sound spd", a, "kt");
    furi_string_cat_printf(o, "Mach: %.3f\n", (double)(tas / a));
}
/* ---------- Climb / descent ---------- */
static void c_tod(const int32_t* v, FuriString* o) {
    if(v[2] <= 0 || v[1] <= 0) { ERR(o, "GS/angle must be > 0"); return; }
    float t = tanf(RAD(v[2])), d = v[0] / (t * 6076.12);
    P(o, "Descent dist", d, "NM");
    P(o, "Descent rate", v[1] * t * 101.27, "fpm");
    TM(o, "Descent time", d / v[1] * 60);
}
static void c_grad(const int32_t* v, FuriString* o) {
    float t = v[0] / 6076.12;
    P(o, "Gradient", t * 100, "%");
    P(o, "Angle", DEG(atanf(t)), "deg");
    P(o, "Climb rate", v[0] * v[1] / 60.0, "fpm");
}
static void c_turn(const int32_t* v, FuriString* o) {
    float t = tanf(RAD(v[1]));
    if(v[0] <= 0 || t <= 0) { ERR(o, "TAS/bank must be > 0"); return; }
    float r = 1091 * t / v[0];
    P(o, "Turn rate", r, "deg/s");
    P(o, "360 turn", 360 / r, "s");
    P(o, "Turn radius", v[0] * (float)v[0] / (11.26 * t), "ft");
}
/* ---------- Conversions ---------- */
#define CONV(fn, A, B, K) \
    static void fn(const int32_t* v, FuriString* o) { conv2(o, v[0], A, B, K); }
CONV(c_nmkm, "NM", "km", 1.852)
CONV(c_nmsm, "NM", "SM", 1.15078)
CONV(c_ktkmh, "kt", "km/h", 1.852)
CONV(c_ktmph, "kt", "mph", 1.15078)
CONV(c_ftm, "ft", "m", 0.3048)
CONV(c_kglb, "kg", "lb", 2.20462)
CONV(c_lgal, "L", "USgal", 0.264172)
CONV(c_hpa, "hPa", "inHg", 0.02953)
static void c_temp(const int32_t* v, FuriString* o) {
    P(o, "C to F", v[0] * 1.8 + 32, "F");
    P(o, "C to K", v[0] + 273.15, "K");
    P(o, "(as F) to C", (v[0] - 32) / 1.8, "C");
}

/* ---------- Tables ---------- */
typedef struct {
    const char* name;
    uint8_t n;
    const char* lbl[4];
    int32_t def[4];
    void (*run)(const int32_t*, FuriString*);
} Calc;

static const Calc C[] = {
    {"Wind triangle", 4, {"TC deg", "TAS kt", "Wind from deg", "Wind kt"}, {90, 100, 180, 20}, c_wt},
    {"Wind components", 3, {"RWY/Course deg", "Wind from deg", "Wind kt"}, {90, 130, 20}, c_wc},
    {"Find wind", 4, {"TC deg", "GS kt", "True HDG deg", "TAS kt"}, {90, 110, 85, 100}, c_fw},
    {"1 in 60", 3, {"Off track NM", "Flown NM", "To go NM"}, {2, 40, 60}, c_160},
    {"Mag var / dev", 3, {"TC deg", "Var E+ W-", "Dev E+ W-"}, {90, 3, -2}, c_mv},
    {"PNR", 3, {"Endur. min", "GS out kt", "GS back kt"}, {240, 100, 120}, c_pnr},
    {"ETP", 3, {"Total NM", "GS to dest", "GS to start"}, {400, 100, 120}, c_etp},
    {"GS/Time/Dist (0=?)", 3, {"GS kt", "Time min", "Dist NM"}, {120, 0, 90}, c_std},
    {"ETA", 3, {"Dep HHMM", "Dist NM", "GS kt"}, {1430, 150, 120}, c_eta},
    {"Fuel (0=?)", 3, {"Flow /h", "Time min", "Fuel"}, {40, 0, 100}, c_fuel},
    {"Fuel weight", 2, {"Litres", "Density kg/m3"}, {100, 720}, c_fw2},
    {"Range", 4, {"Fuel", "Reserve", "Flow /h", "GS kt"}, {150, 30, 40, 110}, c_rng},
    {"Pressure alt", 2, {"Elev ft", "QNH hPa"}, {1000, 1013}, c_pa},
    {"Density alt", 2, {"Press alt ft", "OAT C"}, {2000, 25}, c_da},
    {"CAS to TAS/Mach", 3, {"CAS kt", "Press alt ft", "OAT C"}, {120, 5000, 10}, c_tas},
    {"Top of descent", 3, {"Alt to lose ft", "GS kt", "Angle deg"}, {5000, 120, 3}, c_tod},
    {"Climb gradient", 2, {"Gradient ft/NM", "GS kt"}, {500, 90}, c_grad},
    {"Turn", 2, {"TAS kt", "Bank deg"}, {100, 30}, c_turn},
    {"NM <-> km", 1, {"Value"}, {10}, c_nmkm},
    {"NM <-> SM", 1, {"Value"}, {10}, c_nmsm},
    {"kt <-> km/h", 1, {"Value"}, {100}, c_ktkmh},
    {"kt <-> mph", 1, {"Value"}, {100}, c_ktmph},
    {"ft <-> m", 1, {"Value"}, {1000}, c_ftm},
    {"kg <-> lb", 1, {"Value"}, {100}, c_kglb},
    {"L <-> US gal", 1, {"Value"}, {100}, c_lgal},
    {"hPa <-> inHg", 1, {"Value"}, {1013}, c_hpa},
    {"Temperature", 1, {"Value (C)"}, {15}, c_temp},
};
#define CALC_N (sizeof(C) / sizeof(C[0]))

typedef struct {
    const char* name;
    uint8_t first, count;
} Grp;
static const Grp G[] = {
    {"Wind", 0, 4},
    {"Navigation", 4, 3},
    {"Time/Speed/Dist", 7, 2},
    {"Fuel", 9, 3},
    {"Altitude/Speed", 12, 3},
    {"Climb/Descent", 15, 3},
    {"Conversions", 18, 9},
};
#define GRP_N (sizeof(G) / sizeof(G[0]))

/* ---------- App ---------- */
typedef enum { ViewMenu, ViewNum, ViewText, ViewSettings, ViewAbout } ViewId;
typedef enum { PgMain, PgStart, PgGroup, PgCalc } Page;
typedef enum { ScrMenu, ScrNum, ScrText, ScrSettings, ScrAbout } Scr;

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* menu;
    NumberInput* num;
    TextBox* tb;
    VariableItemList* set;
    Widget* about;
    NotificationApp* notif;
    FuriString* text;
    Page page;
    Scr scr;
    Page text_back;
    uint32_t grp, cur, field, sel;
    int32_t vals[CALC_N][4];
} App;

static void menu_cb(void* ctx, uint32_t index);

static void build_menu(App* a) {
    submenu_reset(a->menu);
    char buf[40];
    switch(a->page) {
    case PgMain:
        submenu_set_header(a->menu, "E6B Flight Computer");
        submenu_add_item(a->menu, "Start", 0, menu_cb, a);
        submenu_add_item(a->menu, "Settings", 1, menu_cb, a);
        submenu_add_item(a->menu, "About", 2, menu_cb, a);
        break;
    case PgStart:
        submenu_set_header(a->menu, "E6B Flight Computer");
        for(uint32_t g = 0; g < GRP_N; g++) submenu_add_item(a->menu, G[g].name, g, menu_cb, a);
        break;
    case PgGroup:
        submenu_set_header(a->menu, G[a->grp].name);
        for(uint32_t i = G[a->grp].first; i < (uint32_t)G[a->grp].first + G[a->grp].count; i++)
            submenu_add_item(a->menu, C[i].name, i, menu_cb, a);
        break;
    case PgCalc:
        submenu_set_header(a->menu, C[a->cur].name);
        for(uint32_t f = 0; f < C[a->cur].n; f++) {
            snprintf(buf, sizeof(buf), "%s: %ld", C[a->cur].lbl[f], (long)a->vals[a->cur][f]);
            submenu_add_item(a->menu, buf, f, menu_cb, a);
        }
        submenu_add_item(a->menu, ">> CALCULATE", C[a->cur].n, menu_cb, a);
        break;
    }
    submenu_set_selected_item(a->menu, a->sel);
}

static void show_menu(App* a) {
    a->scr = ScrMenu;
    build_menu(a);
    view_dispatcher_switch_to_view(a->vd, ViewMenu);
}

static void show_text(App* a, Page back) {
    text_box_reset(a->tb);
    text_box_set_font(a->tb, TextBoxFontText);
    text_box_set_text(a->tb, furi_string_get_cstr(a->text));
    a->scr = ScrText;
    a->text_back = back;
    view_dispatcher_switch_to_view(a->vd, ViewText);
}

static void num_cb(void* ctx, int32_t number) {
    App* a = ctx;
    a->vals[a->cur][a->field] = number;
    a->sel = a->field;
    show_menu(a);
}

static void menu_cb(void* ctx, uint32_t index) {
    App* a = ctx;
    switch(a->page) {
    case PgMain:
        if(index == 0) {
            a->page = PgStart; a->sel = 0; show_menu(a);
        } else if(index == 1) {
            a->scr = ScrSettings;
            view_dispatcher_switch_to_view(a->vd, ViewSettings);
        } else {
            a->scr = ScrAbout;
            view_dispatcher_switch_to_view(a->vd, ViewAbout);
        }
        break;
    case PgStart:
        a->grp = index; a->page = PgGroup; a->sel = 0; show_menu(a);
        break;
    case PgGroup:
        a->cur = index; a->page = PgCalc; a->sel = 0; show_menu(a);
        break;
    case PgCalc:
        if(index < C[a->cur].n) {
            a->field = index;
            number_input_set_header_text(a->num, C[a->cur].lbl[index]);
            number_input_set_result_callback(
                a->num, num_cb, a, a->vals[a->cur][index], -999999, 999999);
            a->scr = ScrNum;
            view_dispatcher_switch_to_view(a->vd, ViewNum);
        } else {
            furi_string_reset(a->text);
            C[a->cur].run(a->vals[a->cur], a->text);
            a->sel = index;
            if(HAPTIC) notification_message(a->notif, &sequence_success);
            show_text(a, PgCalc);
        }
        break;
    }
}

static bool nav_cb(void* ctx) {
    App* a = ctx;
    switch(a->scr) {
    case ScrNum:
        show_menu(a);
        return true;
    case ScrText:
        a->page = a->text_back;
        show_menu(a);
        return true;
    case ScrSettings:
        a->page = PgMain; a->sel = 1; show_menu(a);
        return true;
    case ScrAbout:
        a->page = PgMain; a->sel = 2; show_menu(a);
        return true;
    case ScrMenu:
        if(a->page == PgMain) return false;
        if(a->page == PgStart) { a->page = PgMain; a->sel = 0; }
        else if(a->page == PgGroup) { a->page = PgStart; a->sel = a->grp; }
        else { a->page = PgGroup; a->sel = a->cur - G[a->grp].first; }
        show_menu(a);
        return true;
    }
    return false;
}

static void set_dec_cb(VariableItem* it) {
    uint8_t i = variable_item_get_current_value_index(it);
    D = i;
    char b[2] = {(char)('0' + i), 0};
    variable_item_set_current_value_text(it, b);
}
static void set_hap_cb(VariableItem* it) {
    uint8_t i = variable_item_get_current_value_index(it);
    HAPTIC = i;
    variable_item_set_current_value_text(it, i ? "On" : "Off");
}

int32_t e6b_main(void* p) {
    UNUSED(p);
    App* a = malloc(sizeof(App));
    memset(a, 0, sizeof(App));
    for(size_t i = 0; i < CALC_N; i++)
        for(int f = 0; f < 4; f++) a->vals[i][f] = C[i].def[f];

    a->text = furi_string_alloc();
    a->gui = furi_record_open(RECORD_GUI);
    a->notif = furi_record_open(RECORD_NOTIFICATION);
    a->vd = view_dispatcher_alloc();
    a->menu = submenu_alloc();
    a->num = number_input_alloc();
    a->tb = text_box_alloc();
    a->set = variable_item_list_alloc();
    a->about = widget_alloc();
    widget_add_string_multiline_element(
        a->about, 64, 32, AlignCenter, AlignCenter, FontPrimary,
        "E6B Flight Computer\n\nv1.0\ncreated by Marcopiiit");

    VariableItem* it = variable_item_list_add(a->set, "Decimals", 4, set_dec_cb, a);
    variable_item_set_current_value_index(it, D);
    variable_item_set_current_value_text(it, "1");
    it = variable_item_list_add(a->set, "Haptic on result", 2, set_hap_cb, a);
    variable_item_set_current_value_index(it, 1);
    variable_item_set_current_value_text(it, "On");

    view_dispatcher_add_view(a->vd, ViewMenu, submenu_get_view(a->menu));
    view_dispatcher_add_view(a->vd, ViewNum, number_input_get_view(a->num));
    view_dispatcher_add_view(a->vd, ViewText, text_box_get_view(a->tb));
    view_dispatcher_add_view(a->vd, ViewSettings, variable_item_list_get_view(a->set));
    view_dispatcher_add_view(a->vd, ViewAbout, widget_get_view(a->about));
    view_dispatcher_set_event_callback_context(a->vd, a);
    view_dispatcher_set_navigation_event_callback(a->vd, nav_cb);
    view_dispatcher_attach_to_gui(a->vd, a->gui, ViewDispatcherTypeFullscreen);

    a->page = PgMain;
    show_menu(a);
    view_dispatcher_run(a->vd);

    view_dispatcher_remove_view(a->vd, ViewMenu);
    view_dispatcher_remove_view(a->vd, ViewNum);
    view_dispatcher_remove_view(a->vd, ViewText);
    view_dispatcher_remove_view(a->vd, ViewSettings);
    view_dispatcher_remove_view(a->vd, ViewAbout);
    widget_free(a->about);
    submenu_free(a->menu);
    number_input_free(a->num);
    text_box_free(a->tb);
    variable_item_list_free(a->set);
    view_dispatcher_free(a->vd);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_string_free(a->text);
    free(a);
    return 0;
}
