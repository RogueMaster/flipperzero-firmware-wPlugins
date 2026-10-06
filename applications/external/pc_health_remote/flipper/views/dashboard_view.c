#include "dashboard_view.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>

#define PAGE_COUNT 2

typedef struct {
    PhrLinkSnapshot snap;
    char transport[16];
    char tag[4]; // short transport tag for the header
    char device[28];
    char status[24];
    uint8_t page;
    uint8_t anim;
} DashModel;

struct PhrDashboardView {
    View* view;
    PhrDashboardCallback ok_cb;
    void* ok_ctx;
};

/* ---------- drawing helpers ---------- */

/* Copies src to dst, shortening it (with a trailing "..") until it is at most max_w px wide
 * in the CURRENT canvas font. */
static void fit_str(Canvas* c, const char* src, char* dst, size_t cap, int max_w) {
    snprintf(dst, cap, "%s", src);
    if(canvas_string_width(c, dst) <= max_w) return;
    size_t n = strlen(dst);
    int dots = canvas_string_width(c, "..");
    while(n > 0 && canvas_string_width(c, dst) + dots > max_w) dst[--n] = '\0';
    if(n + 2 < cap) {
        dst[n] = '.';
        dst[n + 1] = '.';
        dst[n + 2] = '\0';
    }
}

static void draw_header(Canvas* c, const char* title, const char* right) {
    canvas_draw_box(c, 0, 0, 128, 12);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 3, 9, title);
    canvas_set_font(c, FontSecondary);
    if(right) canvas_draw_str_aligned(c, 125, 9, AlignRight, AlignBottom, right);
    canvas_set_color(c, ColorBlack);
}

static void draw_bar(Canvas* c, int x, int y, int w, int h, int pct) {
    canvas_draw_frame(c, x, y, w, h);
    if(pct < 0) return;
    if(pct > 100) pct = 100;
    int fill = ((w - 2) * pct) / 100;
    if(fill > 0) canvas_draw_box(c, x + 1, y + 1, fill, h - 2);
}

static void draw_bolt(Canvas* c, int x, int y) {
    // 5x7 lightning bolt, y = top
    canvas_draw_line(c, x + 3, y, x + 1, y + 3);
    canvas_draw_line(c, x + 1, y + 3, x + 4, y + 3);
    canvas_draw_line(c, x + 4, y + 3, x + 2, y + 6);
}

static bool pct_ok(uint8_t v) {
    return v <= 100;
}

/* One big temperature block; col_x = left edge of the 64 px column. */
static void draw_temp(Canvas* c, int col_x, const char* label, bool valid, uint8_t temp) {
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, col_x + 4, 21, label);
    canvas_set_font(c, FontBigNumbers);
    if(valid) {
        char s[8];
        snprintf(s, sizeof(s), "%u", temp);
        int w = canvas_string_width(c, s);
        canvas_draw_str(c, col_x + 4, 38, s);
        canvas_draw_circle(c, col_x + 4 + w + 4, 26, 2);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, col_x + 4 + w + 8, 38, "C");
    } else {
        canvas_draw_str(c, col_x + 4, 38, "--");
    }
    // bar: 0..100 C
    draw_bar(c, col_x + 4, 41, 54, 4, valid ? temp : -1);
}

static void draw_load(Canvas* c, int x, int base_y, const char* label, bool valid, uint8_t pct) {
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, x + 2, base_y, label);
    valid = valid && pct_ok(pct);
    // Bar sits between the (widest) label and the right-aligned number, >= 3 px from both.
    int bar_x = x + 2 + canvas_string_width(c, "VRAM") + 3;
    int num_right = x + 61;
    int bar_w = (num_right - canvas_string_width(c, "100") - 3) - bar_x;
    if(bar_w < 8) bar_w = 8;
    draw_bar(c, bar_x, base_y - 6, bar_w, 6, valid ? pct : -1);
    char s[6];
    if(valid)
        snprintf(s, sizeof(s), "%u", pct);
    else
        snprintf(s, sizeof(s), "--");
    canvas_draw_str_aligned(c, num_right, base_y, AlignRight, AlignBottom, s);
}

static void draw_page_temps(Canvas* c, const DashModel* m) {
    const PhrTelemetry* t = &m->snap.telemetry;
    bool have = m->snap.have_any;
    bool gpu = have && (t->flags & PHR_FLAG_GPU_PRESENT);
    draw_header(c, "Temps", m->tag);
    draw_temp(c, 0, "CPU", have && (t->flags & PHR_FLAG_CPU_TEMP_VALID), t->cpu_temp);
    draw_temp(
        c, 64, "GPU", gpu && (t->flags & PHR_FLAG_GPU_TEMP_VALID), t->gpu_temp);
    canvas_draw_line(c, 63, 14, 63, 45);
    draw_load(c, 0, 54, "CPU", have, t->cpu_load);
    draw_load(c, 64, 54, "GPU", gpu, t->gpu_load);
    draw_load(c, 0, 63, "RAM", have, t->ram_load);
    draw_load(c, 64, 63, "VRAM", gpu, t->vram_load);
}

/* One grid cell: label on the left edge lx, value right-aligned at rx. */
static void row(Canvas* c, int lx, int rx, int y, const char* label, const char* value) {
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, lx, y, label);
    canvas_draw_str_aligned(c, rx, y, AlignRight, AlignBottom, value);
}

/* Heaviest-process line: "CPU  name            12%" (name truncated, percent right-aligned). */
static void proc_row(Canvas* c, int y, const char* label, const char* name, uint8_t pct) {
    const int name_x = 24;
    const int right = 126;
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, y, label);
    if(!name[0]) {
        canvas_draw_str(c, name_x, y, "--");
        return;
    }
    char pcts[8];
    snprintf(pcts, sizeof(pcts), "%u%%", pct);
    int pw = canvas_string_width(c, pcts);
    char nm[16];
    fit_str(c, name, nm, sizeof(nm), right - pw - 4 - name_x);
    canvas_draw_str(c, name_x, y, nm);
    canvas_draw_str_aligned(c, right, y, AlignRight, AlignBottom, pcts);
}

static void draw_page_details(Canvas* c, const DashModel* m) {
    const PhrTelemetry* t = &m->snap.telemetry;
    bool have = m->snap.have_any;
    char s[28];
    // Two columns: labels at x=2 / x=66, values right-aligned at x=61 / x=126.
    const int lx = 2, lr = 61, rx = 66, rr = 126;
    draw_header(c, "Details", m->tag);
    canvas_draw_line(c, 63, 14, 63, 44);

    // left column
    if(have && t->ram_total_dgb)
        snprintf(s, sizeof(s), "%u.%uGB", t->ram_total_dgb / 10, t->ram_total_dgb % 10);
    else
        snprintf(s, sizeof(s), "--");
    row(c, lx, lr, 19, "RAM", s);

    if(have && (t->flags & PHR_FLAG_GPU_PRESENT) && t->vram_total_dgb)
        snprintf(s, sizeof(s), "%u.%uGB", t->vram_total_dgb / 10, t->vram_total_dgb % 10);
    else
        snprintf(s, sizeof(s), "--");
    row(c, lx, lr, 27, "VRAM", s);

    if(have && t->cpu_clock_mhz)
        snprintf(
            s, sizeof(s), "%u.%02uGHz", t->cpu_clock_mhz / 1000, (t->cpu_clock_mhz % 1000) / 10);
    else
        snprintf(s, sizeof(s), "--");
    row(c, lx, lr, 35, "Clk", s);

    if(have && (t->flags & PHR_FLAG_FAN_VALID))
        snprintf(s, sizeof(s), "%urpm", t->fan_rpm);
    else
        snprintf(s, sizeof(s), "--");
    row(c, lx, lr, 43, "Fan", s);

    // right column
    if(have && pct_ok(t->disk_load))
        snprintf(s, sizeof(s), "%u%%", t->disk_load);
    else
        snprintf(s, sizeof(s), "--");
    row(c, rx, rr, 19, "Disk", s);

    bool bat = have && (t->flags & PHR_FLAG_BATTERY_PRESENT) && pct_ok(t->battery);
    if(bat)
        snprintf(s, sizeof(s), "%u%%", t->battery);
    else
        snprintf(s, sizeof(s), "--");
    row(c, rx, rr, 27, "Batt", s);
    if(bat && (t->flags & PHR_FLAG_CHARGING)) {
        // bolt (5 px wide) sits 3 px left of the right-aligned value
        int w = canvas_string_width(c, s);
        draw_bolt(c, rr - w - 8, 20);
    }

    if(have) {
        if(t->uptime_h >= 24)
            snprintf(s, sizeof(s), "%ud %uh", t->uptime_h / 24, t->uptime_h % 24);
        else
            snprintf(s, sizeof(s), "%uh", t->uptime_h);
    } else {
        snprintf(s, sizeof(s), "--");
    }
    row(c, rx, rr, 35, "Up", s);

    // heaviest processes
    canvas_draw_line(c, 0, 46, 127, 46);
    proc_row(c, 54, "CPU", have ? t->top_cpu_name : "", t->top_cpu_pct);
    proc_row(c, 62, "RAM", have ? t->top_ram_name : "", t->top_ram_pct);
}

static void draw_waiting(Canvas* c, const DashModel* m) {
    draw_header(c, "PC Health Remote", NULL);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 24, AlignCenter, AlignBottom, m->transport);
    canvas_set_font(c, FontSecondary);
    char line[28];
    fit_str(c, m->device, line, sizeof(line), 122);
    canvas_draw_str_aligned(c, 64, 34, AlignCenter, AlignBottom, line);
    fit_str(c, m->status, line, sizeof(line), 122);
    canvas_draw_str_aligned(c, 64, 44, AlignCenter, AlignBottom, line);
    canvas_draw_str_aligned(c, 64, 55, AlignCenter, AlignBottom, "Start backend on PC");
    // animated dots
    int active = (m->anim / 2) % 4;
    for(int i = 0; i < 3; i++) {
        int x = 58 + i * 6;
        if(i == active)
            canvas_draw_disc(c, x, 60, 2);
        else
            canvas_draw_disc(c, x, 60, 1);
    }
}

static void draw_lost_overlay(Canvas* c, const DashModel* m) {
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 10, 17, 108, 32);
    canvas_set_color(c, ColorBlack);
    canvas_draw_rframe(c, 10, 17, 108, 32, 3);
    canvas_draw_rframe(c, 12, 19, 104, 28, 2);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 31, AlignCenter, AlignBottom, "PC lost");
    canvas_set_font(c, FontSecondary);
    char s[28];
    char s2[28];
    snprintf(s, sizeof(s), "No data for %lu s", (unsigned long)(m->snap.silence_ms / 1000));
    fit_str(c, s, s2, sizeof(s2), 100);
    canvas_draw_str_aligned(c, 64, 42, AlignCenter, AlignBottom, s2);
}

static void dash_draw(Canvas* canvas, void* model) {
    const DashModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(!m->snap.have_any) {
        draw_waiting(canvas, m);
        return;
    }
    if(m->page == 0)
        draw_page_temps(canvas, m);
    else
        draw_page_details(canvas, m);
    // page indicator "1/2" drawn in the header, right of the transport tag
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char pg[8];
    snprintf(pg, sizeof(pg), "%u/%u", m->page + 1, PAGE_COUNT);
    canvas_draw_str_aligned(canvas, 104, 9, AlignRight, AlignBottom, pg);
    canvas_set_color(canvas, ColorBlack);
    if(m->snap.state == PhrLinkLost) draw_lost_overlay(canvas, m);
}

static bool dash_input(InputEvent* event, void* context) {
    PhrDashboardView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    bool consumed = false;
    switch(event->key) {
    case InputKeyLeft:
    case InputKeyUp:
        with_view_model(
            v->view, DashModel * m, { m->page = (m->page + PAGE_COUNT - 1) % PAGE_COUNT; }, true);
        consumed = true;
        break;
    case InputKeyRight:
    case InputKeyDown:
        with_view_model(v->view, DashModel * m, { m->page = (m->page + 1) % PAGE_COUNT; }, true);
        consumed = true;
        break;
    case InputKeyOk:
        if(event->type == InputTypeShort && v->ok_cb) v->ok_cb(v->ok_ctx);
        consumed = true;
        break;
    default:
        break;
    }
    return consumed;
}

PhrDashboardView* phr_dashboard_view_alloc(void) {
    PhrDashboardView* v = malloc(sizeof(PhrDashboardView));
    memset(v, 0, sizeof(PhrDashboardView));
    v->view = view_alloc();
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(DashModel));
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, dash_draw);
    view_set_input_callback(v->view, dash_input);
    with_view_model(
        v->view,
        DashModel * m,
        {
            memset(m, 0, sizeof(DashModel));
            snprintf(m->status, sizeof(m->status), "Starting...");
        },
        false);
    return v;
}

void phr_dashboard_view_free(PhrDashboardView* v) {
    view_free(v->view);
    free(v);
}

View* phr_dashboard_view_get_view(PhrDashboardView* v) {
    return v->view;
}

void phr_dashboard_view_set_ok_callback(PhrDashboardView* v, PhrDashboardCallback cb, void* ctx) {
    v->ok_cb = cb;
    v->ok_ctx = ctx;
}

void phr_dashboard_view_update(
    PhrDashboardView* v,
    const PhrLinkSnapshot* snap,
    const char* transport,
    const char* device,
    const char* status) {
    with_view_model(
        v->view,
        DashModel * m,
        {
            m->snap = *snap;
            snprintf(m->transport, sizeof(m->transport), "%s", transport);
            snprintf(m->tag, sizeof(m->tag), "%s", transport[0] == 'U' ? "USB" : "BLE");
            snprintf(m->device, sizeof(m->device), "%s", device);
            snprintf(m->status, sizeof(m->status), "%s", status);
            m->anim++;
        },
        true);
}
