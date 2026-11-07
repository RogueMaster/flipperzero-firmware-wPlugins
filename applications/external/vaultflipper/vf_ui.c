// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT

#include "vf_ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <string.h>

/* ---------- helpers ---------- */

void vf_draw_str_fit(Canvas* canvas, int x, int y, int max_width, const char* text) {
    if(canvas_string_width(canvas, text) <= max_width) {
        canvas_draw_str(canvas, x, y, text);
        return;
    }
    char buf[64];
    size_t n = strlcpy(buf, text, sizeof(buf));
    if(n >= sizeof(buf)) n = sizeof(buf) - 1;
    while(n > 0) {
        buf[n] = 0;
        char tmp[68];
        snprintf(tmp, sizeof(tmp), "%s..", buf);
        if(canvas_string_width(canvas, tmp) <= max_width) {
            canvas_draw_str(canvas, x, y, tmp);
            return;
        }
        n--;
    }
}

/* The font's '@' is a pixel taller than the other letters and touches the line
 * above, so subtitles draw this 7 px one instead (XBM, 5 x 7). */
static const uint8_t vf_at_bits[] = {0x0e, 0x11, 0x1d, 0x15, 0x1d, 0x01, 0x0e};
#define VF_AT_ADVANCE 6

static int vf_sub_char_width(Canvas* canvas, char c) {
    return c == '@' ? VF_AT_ADVANCE : (int)canvas_glyph_width(canvas, (uint8_t)c);
}

/* Like vf_draw_str_fit, with the compact '@'. */
static void vf_draw_sub_fit(Canvas* canvas, int x, int y, int max_width, const char* text) {
    int total = 0;
    for(const char* p = text; *p; p++)
        total += vf_sub_char_width(canvas, *p);
    bool cut = total > max_width;
    int limit = cut ? max_width - (int)canvas_string_width(canvas, "..") : max_width;
    int used = 0;
    for(const char* p = text; *p; p++) {
        int w = vf_sub_char_width(canvas, *p);
        if(used + w > limit) break;
        if(*p == '@')
            canvas_draw_xbm(canvas, x + used, y - 7, 5, 7, vf_at_bits);
        else
            canvas_draw_glyph(canvas, x + used, y, (uint8_t)*p);
        used += w;
    }
    if(cut) canvas_draw_str(canvas, x + used, y, "..");
}

void vf_draw_header(Canvas* canvas, const char* title, const char* note, const Icon* icon) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, VF_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    int x = 3;
    if(icon) {
        canvas_draw_icon(canvas, 2, 1, icon);
        x = 15;
    }
    int right = 125;
    if(note && note[0]) {
        canvas_set_font(canvas, FontSecondary);
        int w = canvas_string_width(canvas, note);
        canvas_draw_str(canvas, right - w, 8, note);
        right -= w + 4;
    }
    canvas_set_font(canvas, FontPrimary);
    vf_draw_str_fit(canvas, x, 9, right - x, title);
    canvas_set_color(canvas, ColorBlack);
}

/* ---------- list ---------- */

struct VfList {
    View* view;
};

typedef struct {
    char header[40];
    const Icon* header_icon;
    uint32_t count;
    uint32_t selected;
    uint32_t offset;
    VfListStyle style;
    VfListItemFn item_fn;
    VfListSelectFn select_fn;
    void* ctx;
    char status[16];
    uint8_t bar;
} VfListModel;

/* Plain rows are 12 px. In open lists the selected row grows to 20 px and
 * shows its subtitle. Detail rows are 18 px with title and subtitle, so two
 * and most of a third fit. The row after the last full one peeks in. */
#define VF_ROW_H        12
#define VF_ROW_OPEN_H   20
#define VF_ROW_DETAIL_H 18
#define VF_LIST_TOP     (VF_HEADER_H + 1)
#define VF_LIST_AVAIL   (64 - VF_LIST_TOP)

static uint8_t vf_list_row_h(const VfListModel* m, uint32_t index) {
    if(m->style == VfListDetail) return VF_ROW_DETAIL_H;
    return (m->style == VfListOpen && index == m->selected) ? VF_ROW_OPEN_H : VF_ROW_H;
}

/* Keep the selected row visible: scroll up to it, or down just far enough
 * that the rows from offset to selected fit. */
static void vf_list_fix_offset(VfListModel* m) {
    if(m->selected <= m->offset) {
        m->offset = m->selected;
        return;
    }
    uint32_t first = m->selected;
    uint32_t h = vf_list_row_h(m, first);
    while(first > m->offset && h + vf_list_row_h(m, first - 1) <= VF_LIST_AVAIL)
        h += vf_list_row_h(m, --first);
    m->offset = first;
}

static void vf_list_draw(Canvas* canvas, void* model) {
    VfListModel* m = model;
    canvas_clear(canvas);
    uint32_t fits = m->style == VfListPlain ? 4 : m->style == VfListOpen ? 3 : 2;
    char note[24] = "";
    if(m->status[0])
        strlcpy(note, m->status, sizeof(note));
    else if(m->count > fits)
        snprintf(note, sizeof(note), "%lu/%lu", m->selected + 1, m->count);

    int y = VF_LIST_TOP;
    for(uint32_t index = m->offset; index < m->count && y < 64; index++) {
        uint8_t row_h = vf_list_row_h(m, index);
        VfListItem item = {0};
        char sub[40] = "";
        m->item_fn(m->ctx, index, &item, sub, sizeof(sub));
        bool sel = index == m->selected;
        bool detail = m->style == VfListDetail;
        bool two = detail || (sel && m->style == VfListOpen);
        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 0, y, 123, detail ? row_h : row_h - 1, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        int x = 3;
        if(item.icon) {
            canvas_draw_icon(canvas, 2, y + (detail ? 3 : 1), item.icon);
            x = 15;
        }
        if(two && item.subtitle && item.subtitle[0]) {
            canvas_set_font(canvas, FontSecondary);
            vf_draw_sub_fit(canvas, x, y + 17, 120 - x, item.subtitle);
            /* keep a blank line under the title: cut the top of other tall glyphs */
            canvas_set_color(canvas, sel ? ColorBlack : ColorWhite);
            canvas_draw_line(canvas, x, y + 9, 121, y + 9);
            canvas_set_color(canvas, sel ? ColorWhite : ColorBlack);
        }
        int right = 120;
        int title_y = y + 9;
        canvas_set_font(canvas, two ? FontPrimary : FontSecondary);
        if(item.value && item.value[0]) {
            int w = canvas_string_width(canvas, item.value);
            canvas_draw_str(canvas, right - w, title_y, item.value);
            right -= w + 4;
        }
        vf_draw_str_fit(canvas, x, title_y, right - x, item.title ? item.title : "");
        canvas_set_color(canvas, ColorBlack);
        y += row_h;
    }
    /* header last, so a peeking row never covers it */
    vf_draw_header(canvas, m->header, note, m->header_icon);
    if(m->bar) canvas_draw_line(canvas, 0, VF_HEADER_H, m->bar - 1, VF_HEADER_H);
    if(m->count > fits) {
        elements_scrollbar_pos(canvas, 128, VF_LIST_TOP, VF_LIST_AVAIL, m->selected, m->count);
    }
}

static bool vf_list_input(InputEvent* event, void* context) {
    VfList* list = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    bool consumed = false;
    VfListSelectFn select = NULL;
    void* ctx = NULL;
    uint32_t index = 0;
    with_view_model(
        list->view,
        VfListModel * m,
        {
            if(m->count == 0) {
                consumed = event->key == InputKeyUp || event->key == InputKeyDown ||
                           event->key == InputKeyOk;
            } else if(event->key == InputKeyUp) {
                if(m->selected > 0)
                    m->selected--;
                else if(event->type == InputTypeShort)
                    m->selected = m->count - 1;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(m->selected + 1 < m->count)
                    m->selected++;
                else if(event->type == InputTypeShort)
                    m->selected = 0;
                consumed = true;
            } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                select = m->select_fn;
                ctx = m->ctx;
                index = m->selected;
                consumed = true;
            }
            vf_list_fix_offset(m);
        },
        true);
    if(select) select(ctx, index); /* outside the model lock */
    return consumed;
}

VfList* vf_list_alloc(void) {
    VfList* list = malloc(sizeof(VfList));
    list->view = view_alloc();
    view_allocate_model(list->view, ViewModelTypeLocking, sizeof(VfListModel));
    view_set_context(list->view, list);
    view_set_draw_callback(list->view, vf_list_draw);
    view_set_input_callback(list->view, vf_list_input);
    return list;
}

void vf_list_free(VfList* list) {
    view_free(list->view);
    free(list);
}

View* vf_list_get_view(VfList* list) {
    return list->view;
}

void vf_list_set(
    VfList* list,
    const char* header,
    const Icon* header_icon,
    uint32_t count,
    VfListStyle style,
    VfListItemFn item_fn,
    VfListSelectFn select_fn,
    void* ctx,
    uint32_t selected) {
    with_view_model(
        list->view,
        VfListModel * m,
        {
            strlcpy(m->header, header, sizeof(m->header));
            m->header_icon = header_icon;
            m->count = count;
            m->style = style;
            m->status[0] = 0;
            m->bar = 0;
            m->item_fn = item_fn;
            m->select_fn = select_fn;
            m->ctx = ctx;
            m->selected = (count && selected < count) ? selected : 0;
            m->offset = 0;
            vf_list_fix_offset(m);
        },
        true);
}

uint32_t vf_list_get_selected(VfList* list) {
    uint32_t sel = 0;
    with_view_model(list->view, VfListModel * m, { sel = m->selected; }, false);
    return sel;
}

void vf_list_refresh(VfList* list) {
    with_view_model(list->view, VfListModel * m, { UNUSED(m); }, true);
}

void vf_list_set_status(VfList* list, const char* note, uint8_t bar) {
    with_view_model(
        list->view,
        VfListModel * m,
        {
            strlcpy(m->status, note ? note : "", sizeof(m->status));
            m->bar = bar > 128 ? 128 : bar;
        },
        true);
}

/* ---------- entry card ---------- */

#include "vaultflipper_icons.h"

struct VfCard {
    View* view;
};

typedef struct {
    const char* name;
    const char* user;
    const char* pass;
    const char* uri;
    const char* note;
    const char* labels[4];
    bool show_pass;
    VfCardFn fn;
    void* ctx;
} VfCardModel;

static void vf_card_host(const char* uri, char* out, size_t size) {
    const char* p = strstr(uri, "://");
    p = p ? p + 3 : uri;
    size_t i = 0;
    while(p[i] && p[i] != '/' && p[i] != '?' && i + 1 < size) {
        out[i] = p[i];
        i++;
    }
    out[i] = 0;
}

static void vf_card_draw(Canvas* canvas, void* model) {
    VfCardModel* m = model;
    canvas_clear(canvas);
    vf_draw_header(canvas, m->name, m->note, &I_key);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_icon(canvas, 2, 14, &I_user);
    vf_draw_str_fit(canvas, 15, 22, 110, m->user[0] ? m->user : "-");

    char host[48];
    vf_card_host(m->uri, host, sizeof(host));
    canvas_draw_icon(canvas, 2, 26, &I_globe);
    vf_draw_str_fit(canvas, 15, 34, 110, host[0] ? host : "-");

    canvas_draw_icon(canvas, 2, 38, &I_lock);
    if(!m->pass[0]) {
        canvas_draw_str(canvas, 15, 46, "-");
    } else if(m->show_pass) {
        vf_draw_str_fit(canvas, 15, 46, 110, m->pass);
    } else {
        int w = canvas_string_width(canvas, m->labels[3]);
        char stars[24];
        size_t n = strlen(m->pass);
        if(n > sizeof(stars) - 1) n = sizeof(stars) - 1;
        memset(stars, '*', n);
        stars[n] = 0;
        /* as many stars as fit before the "show" hint */
        while(n > 1 && canvas_string_width(canvas, stars) > 125 - w - 10 - 15)
            stars[--n] = 0;
        canvas_draw_str(canvas, 15, 47, stars);
        canvas_draw_str(canvas, 125 - w, 46, m->labels[3]);
        canvas_draw_triangle(canvas, 125 - w - 6, 43, 5, 3, CanvasDirectionBottomToTop);
    }

    if(m->user[0]) elements_button_left(canvas, m->labels[0]);
    if(m->user[0] || m->pass[0]) elements_button_center(canvas, m->labels[2]);
    if(m->pass[0]) elements_button_right(canvas, m->labels[1]);
}

static bool vf_card_input(InputEvent* event, void* context) {
    VfCard* card = context;
    if(event->type != InputTypeShort) return event->key != InputKeyBack;
    VfCardFn fn = NULL;
    void* ctx = NULL;
    int action = -1;
    bool consumed = true;
    with_view_model(
        card->view,
        VfCardModel * m,
        {
            fn = m->fn;
            ctx = m->ctx;
            bool has_user = m->user[0] != 0;
            bool has_pass = m->pass[0] != 0;
            switch(event->key) {
            case InputKeyLeft:
                if(has_user) action = VfCardTypeUser;
                break;
            case InputKeyRight:
                if(has_pass) action = VfCardTypePass;
                break;
            case InputKeyOk:
                action = (has_user && has_pass) ? VfCardTypeLogin :
                         has_pass               ? VfCardTypePass :
                         has_user               ? VfCardTypeUser :
                                                  -1;
                break;
            case InputKeyUp:
                m->show_pass = !m->show_pass;
                break;
            case InputKeyDown:
                action = VfCardMore;
                break;
            default:
                consumed = false;
                break;
            }
        },
        true);
    if(fn && action >= 0) fn(ctx, (VfCardAction)action);
    return consumed;
}

VfCard* vf_card_alloc(void) {
    VfCard* card = malloc(sizeof(VfCard));
    card->view = view_alloc();
    view_allocate_model(card->view, ViewModelTypeLocking, sizeof(VfCardModel));
    view_set_context(card->view, card);
    view_set_draw_callback(card->view, vf_card_draw);
    view_set_input_callback(card->view, vf_card_input);
    with_view_model(
        card->view,
        VfCardModel * m,
        {
            m->name = m->user = m->pass = m->uri = m->note = "";
            for(int i = 0; i < 4; i++)
                m->labels[i] = "";
        },
        false);
    return card;
}

void vf_card_free(VfCard* card) {
    view_free(card->view);
    free(card);
}

View* vf_card_get_view(VfCard* card) {
    return card->view;
}

void vf_card_set(
    VfCard* card,
    const char* name,
    const char* user,
    const char* pass,
    const char* uri,
    const char* note,
    const char* labels[4],
    VfCardFn fn,
    void* ctx) {
    with_view_model(
        card->view,
        VfCardModel * m,
        {
            m->name = name;
            m->user = user;
            m->pass = pass;
            m->uri = uri;
            m->note = note;
            for(int i = 0; i < 4; i++)
                m->labels[i] = labels[i];
            m->show_pass = false;
            m->fn = fn;
            m->ctx = ctx;
        },
        true);
}

void vf_card_hide_password(VfCard* card) {
    with_view_model(
        card->view,
        VfCardModel * m,
        {
            m->show_pass = false;
            m->name = m->user = m->pass = m->uri = m->note = "";
        },
        false);
}

/* ---------- 2FA code ---------- */

struct VfCode {
    View* view;
};

typedef struct {
    const char* name;
    const char* user;
    const char* note;
    const char* type_label;
    char text[24];
    bool message;
    bool cycle;
    uint32_t left;
    uint32_t period;
    VfCodeFn fn;
    void* ctx;
} VfCodeModel;

/* Draw the code in two groups ("123 456"), big if it fits. */
static void vf_code_draw_text(Canvas* canvas, const char* text, bool message) {
    if(message) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, text);
        return;
    }
    size_t n = strlen(text);
    size_t half = (n + 1) / 2;
    char a[12], b[12];
    snprintf(a, sizeof(a), "%.*s", (int)half, text);
    snprintf(b, sizeof(b), "%s", text + half);
    bool digits = true;
    for(size_t i = 0; i < n; i++)
        if(text[i] < '0' || text[i] > '9') digits = false;
    canvas_set_font(canvas, digits ? FontBigNumbers : FontPrimary);
    int gap = digits ? 7 : 5;
    int wa = canvas_string_width(canvas, a), wb = canvas_string_width(canvas, b);
    if(wa + gap + wb > 116) {
        canvas_set_font(canvas, FontPrimary);
        gap = 5;
        wa = canvas_string_width(canvas, a);
        wb = canvas_string_width(canvas, b);
    }
    int x = 64 - (wa + gap + wb) / 2;
    canvas_draw_str(canvas, x, 38, a);
    canvas_draw_str(canvas, x + wa + gap, 38, b);
}

static void vf_code_draw(Canvas* canvas, void* model) {
    VfCodeModel* m = model;
    canvas_clear(canvas);
    vf_draw_header(canvas, m->name, m->note, &I_totp);
    canvas_set_font(canvas, FontSecondary);
    if(m->user[0]) {
        char user[40];
        strlcpy(user, m->user, sizeof(user));
        if(canvas_string_width(canvas, user) <= 124)
            canvas_draw_str_aligned(canvas, 64, 21, AlignCenter, AlignBottom, user);
        else
            vf_draw_str_fit(canvas, 2, 21, 124, user);
    }
    vf_code_draw_text(canvas, m->text, m->message);
    if(m->cycle) {
        canvas_draw_triangle(canvas, 3, 31, 7, 4, CanvasDirectionRightToLeft);
        canvas_draw_triangle(canvas, 124, 31, 7, 4, CanvasDirectionLeftToRight);
    }

    if(!m->message && m->period) {
        /* countdown: bar shrinks, seconds on the right */
        int full = 100;
        int w = (int)((m->left * (uint32_t)full + m->period / 2) / m->period);
        canvas_draw_rframe(canvas, 4, 42, full + 4, 6, 2);
        if(w > 0) canvas_draw_box(canvas, 6, 44, w, 2);
        char s[8];
        snprintf(s, sizeof(s), "%lus", (unsigned long)m->left);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 48, AlignRight, AlignBottom, s);
        elements_button_center(canvas, m->type_label);
    }
}

static bool vf_code_input(InputEvent* event, void* context) {
    VfCode* code = context;
    if(event->type != InputTypeShort) return event->key != InputKeyBack;
    VfCodeFn fn = NULL;
    void* ctx = NULL;
    int action = -1;
    bool consumed = true;
    with_view_model(
        code->view,
        VfCodeModel * m,
        {
            fn = m->fn;
            ctx = m->ctx;
            switch(event->key) {
            case InputKeyOk:
                if(!m->message) action = VfCodeType;
                break;
            case InputKeyLeft:
                if(m->cycle) action = VfCodePrev;
                break;
            case InputKeyRight:
                if(m->cycle) action = VfCodeNext;
                break;
            case InputKeyBack:
                consumed = false;
                break;
            default:
                break;
            }
        },
        false);
    if(fn && action >= 0) fn(ctx, (VfCodeAction)action);
    return consumed;
}

VfCode* vf_code_alloc(void) {
    VfCode* code = malloc(sizeof(VfCode));
    code->view = view_alloc();
    view_allocate_model(code->view, ViewModelTypeLocking, sizeof(VfCodeModel));
    view_set_context(code->view, code);
    view_set_draw_callback(code->view, vf_code_draw);
    view_set_input_callback(code->view, vf_code_input);
    vf_code_clear(code);
    return code;
}

void vf_code_free(VfCode* code) {
    view_free(code->view);
    free(code);
}

View* vf_code_get_view(VfCode* code) {
    return code->view;
}

void vf_code_set(
    VfCode* code,
    const char* name,
    const char* user,
    const char* note,
    const char* type_label,
    bool cycle,
    VfCodeFn fn,
    void* ctx) {
    with_view_model(
        code->view,
        VfCodeModel * m,
        {
            m->name = name;
            m->user = user;
            m->note = note;
            m->type_label = type_label;
            m->cycle = cycle;
            m->fn = fn;
            m->ctx = ctx;
            m->text[0] = 0;
            m->message = true;
        },
        true);
}

void vf_code_update(VfCode* code, const char* text, bool message, uint32_t left, uint32_t period) {
    with_view_model(
        code->view,
        VfCodeModel * m,
        {
            strlcpy(m->text, text, sizeof(m->text));
            m->message = message;
            m->left = left;
            m->period = period;
        },
        true);
}

void vf_code_clear(VfCode* code) {
    with_view_model(
        code->view,
        VfCodeModel * m,
        {
            memset(m->text, 0, sizeof(m->text));
            m->name = m->user = m->note = m->type_label = "";
            m->message = true;
            m->cycle = false;
            m->fn = NULL;
        },
        false);
}

/* ---------- busy screen ---------- */

struct VfBusy {
    View* view;
    FuriTimer* timer;
};

typedef struct {
    char text[40];
    const Icon* icon;
    uint8_t frame;
} VfBusyModel;

static void vf_busy_draw(Canvas* canvas, void* model) {
    VfBusyModel* m = model;
    canvas_clear(canvas);
    elements_bold_rounded_frame(canvas, 14, 10, 100, 44);
    if(m->icon) canvas_draw_icon(canvas, 59, 17, m->icon);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, m->text);
    for(uint8_t i = 0; i < 3; i++) {
        int x = 56 + i * 8;
        if(i == m->frame % 4 || (m->frame % 4) == 3)
            canvas_draw_disc(canvas, x, 46, 2);
        else
            canvas_draw_circle(canvas, x, 46, 2);
    }
}

static void vf_busy_tick(void* context) {
    VfBusy* busy = context;
    with_view_model(busy->view, VfBusyModel * m, { m->frame++; }, true);
}

static void vf_busy_enter(void* context) {
    VfBusy* busy = context;
    furi_timer_start(busy->timer, 180);
}

static void vf_busy_exit(void* context) {
    VfBusy* busy = context;
    furi_timer_stop(busy->timer);
}

VfBusy* vf_busy_alloc(void) {
    VfBusy* busy = malloc(sizeof(VfBusy));
    busy->view = view_alloc();
    view_allocate_model(busy->view, ViewModelTypeLocking, sizeof(VfBusyModel));
    view_set_context(busy->view, busy);
    view_set_draw_callback(busy->view, vf_busy_draw);
    view_set_enter_callback(busy->view, vf_busy_enter);
    view_set_exit_callback(busy->view, vf_busy_exit);
    busy->timer = furi_timer_alloc(vf_busy_tick, FuriTimerTypePeriodic, busy);
    return busy;
}

void vf_busy_free(VfBusy* busy) {
    furi_timer_stop(busy->timer);
    furi_timer_free(busy->timer);
    view_free(busy->view);
    free(busy);
}

View* vf_busy_get_view(VfBusy* busy) {
    return busy->view;
}

void vf_busy_set(VfBusy* busy, const char* text, const Icon* icon) {
    with_view_model(
        busy->view,
        VfBusyModel * m,
        {
            strlcpy(m->text, text, sizeof(m->text));
            m->icon = icon;
            m->frame = 0;
        },
        true);
}
