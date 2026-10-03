#include "detector_view.h"

#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 128
#define SCREEN_H 64

#define TITLE_BASE 8
#define RULE_Y     10
#define HINT_BASE  19
#define BOX_Y      21
#define BOX_H      (SCREEN_H - BOX_Y - 1)

#define NAME_BASE  33
#define LABEL_BASE 43
#define LINE1_BASE 53
#define LINE2_BASE 62

/// The idle screen: three lines of instruction, then a status line with room
/// to spare underneath it.
#define IDLE_TEXT_Y      14
#define IDLE_STATUS_BASE 61

#define PAD_X    4
#define DETAIL_X 4
#define DETAIL_W (SCREEN_W - 2 * DETAIL_X)

/// Ticks the receive marker stays lit. The view ticks at 4 Hz.
#define FLASH_TICKS 2

typedef enum {
    PageBrands = 0,
    PageModel,
    PageTiming,
    PagePayload,
    PageCount,
} DetailPage;

static const char* const page_names[PageCount] = {
    "Brands",
    "Model",
    "Timing",
    "Payload",
};

typedef struct {
    bool has_result;
    AcResultKind kind;
    const AcProtoEntry* entry;

    char bits_str[16];
    char use_str[36];
    char shape_str[40];
    char timing_hdr[28];
    char timing_bit[32];
    char payload_a[64];
    char payload_b[64];
    char variant_str[40];

    uint8_t page;
    size_t scroll;

    uint32_t noise_count;
    uint8_t flash;
    bool flash_valid;
    uint8_t idle_anim;
} DetectorModel;

struct DetectorView {
    View* view;
    DetectorViewCallback back_cb;
    void* back_ctx;
};

// ---------------------------------------------------------------------- draw

static void draw_rx_marker(Canvas* canvas, const DetectorModel* m) {
    if(!m->flash) return;
    if(m->flash_valid) {
        canvas_draw_disc(canvas, SCREEN_W - 6, 5, 3);
    } else {
        canvas_draw_circle(canvas, SCREEN_W - 6, 5, 3);
    }
}

static void draw_header(Canvas* canvas, const DetectorModel* m) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, SCREEN_W / 2, TITLE_BASE, AlignCenter, AlignBottom, "AC Detector");
    canvas_draw_line(canvas, 0, RULE_Y, SCREEN_W - 1, RULE_Y);
    draw_rx_marker(canvas, m);
}

static void draw_idle(Canvas* canvas, const DetectorModel* m) {
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(
        canvas,
        SCREEN_W / 2,
        IDLE_TEXT_Y,
        AlignCenter,
        AlignTop,
        "Point your AC remote at\nthe IR window and press\nthe Power button");

    // One status line at the bottom, clear of the three lines above it. The
    // ignored count rides along rather than taking a line of its own.
    static const char* const dots[] = {"Listening", "Listening.", "Listening..", "Listening..."};
    char buf[40];
    if(m->noise_count) {
        snprintf(
            buf,
            sizeof(buf),
            "%s  %lu ignored",
            dots[m->idle_anim & 3],
            (unsigned long)m->noise_count);
    } else {
        snprintf(buf, sizeof(buf), "%s", dots[m->idle_anim & 3]);
    }
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, IDLE_STATUS_BASE, AlignCenter, AlignBottom, buf);
}

static void draw_page_dots(Canvas* canvas, const DetectorModel* m) {
    // One dot per page, the current one filled.
    const int16_t x0 = SCREEN_W - 4 - (PageCount * 6 - 2);
    for(uint8_t i = 0; i < PageCount; i++) {
        int16_t cx = x0 + i * 6;
        if(i == m->page) {
            canvas_draw_disc(canvas, cx, LABEL_BASE - 3, 2);
        } else {
            canvas_draw_dot(canvas, cx, LABEL_BASE - 3);
        }
    }
}

/// What the two detail rows show for the current page, and whether each row
/// is long enough to need scrolling. Either row may be NULL.
static void page_lines(
    const DetectorModel* m,
    const char** a,
    bool* scroll_a,
    const char** b,
    bool* scroll_b) {
    *a = NULL;
    *b = NULL;
    *scroll_a = false;
    *scroll_b = false;

    const bool known = (m->kind == AcResultMatch) && m->entry;

    switch(m->page) {
    case PageBrands:
        if(known && m->entry->consumer) {
            *a = "Not an A/C protocol";
        } else {
            *a = known ? m->entry->brands : "Not in the database";
        }
        *scroll_a = known && !m->entry->consumer;
        *b = m->use_str;
        break;
    case PageModel:
        *a = m->variant_str;
        *scroll_a = true;
        *b = m->shape_str;
        break;
    case PageTiming:
        *a = m->timing_hdr;
        *b = m->timing_bit;
        break;
    default:
        *a = m->payload_a;
        *scroll_a = true;
        *b = m->payload_b;
        *scroll_b = true;
        break;
    }
}

/// Scroll a row only when it is actually too wide, so short strings sit still
/// instead of twitching.
static void draw_detail_line(
    Canvas* canvas,
    int16_t base,
    const char* text,
    bool may_scroll,
    size_t scroll) {
    if(!text || !text[0]) return;
    if(may_scroll && canvas_string_width(canvas, text) > DETAIL_W) {
        FuriString* line = furi_string_alloc_set(text);
        elements_scrollable_text_line(canvas, DETAIL_X, base, DETAIL_W, line, scroll, false);
        furi_string_free(line);
    } else {
        canvas_draw_str(canvas, DETAIL_X, base, text);
    }
}

static void draw_result(Canvas* canvas, const DetectorModel* m) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, SCREEN_W / 2, HINT_BASE, AlignCenter, AlignBottom, "Press Power on your remote");

    elements_slightly_rounded_frame(canvas, 0, BOX_Y, SCREEN_W, BOX_H);

    const char* name = (m->kind == AcResultMatch && m->entry) ? m->entry->name : "Unknown";

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, PAD_X, NAME_BASE, name);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, SCREEN_W - PAD_X, NAME_BASE, AlignRight, AlignBottom, m->bits_str);

    canvas_draw_str(canvas, PAD_X, LABEL_BASE, page_names[m->page]);
    draw_page_dots(canvas, m);

    const char *a, *b;
    bool scroll_a, scroll_b;
    page_lines(m, &a, &scroll_a, &b, &scroll_b);

    // draw_detail_line() only scrolls a row that genuinely overflows, so
    // asking for it everywhere costs nothing and stops a long app name -
    // "Use: Mitsubishi (Full) AC Remote" - from running off the edge.
    draw_detail_line(canvas, LINE1_BASE, a, scroll_a, m->scroll);
    draw_detail_line(canvas, LINE2_BASE, b, true, m->scroll);
    (void)scroll_b;
}

static void detector_view_draw(Canvas* canvas, void* model) {
    DetectorModel* m = model;
    canvas_clear(canvas);
    draw_header(canvas, m);

    if(m->has_result) {
        draw_result(canvas, m);
    } else {
        draw_idle(canvas, m);
    }
}

// --------------------------------------------------------------------- input

static bool detector_view_input(InputEvent* event, void* context) {
    DetectorView* v = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        // Let long-press Back fall through to the dispatcher.
        return false;
    }

    switch(event->key) {
    case InputKeyLeft:
    case InputKeyRight: {
        bool changed = false;
        with_view_model(
            v->view,
            DetectorModel * m,
            {
                if(m->has_result) {
                    m->page =
                        (uint8_t)((m->page + (event->key == InputKeyRight ? 1 : PageCount - 1)) %
                                  PageCount);
                    m->scroll = 0;
                    changed = true;
                }
            },
            true);
        return changed;
    }
    case InputKeyOk:
        with_view_model(
            v->view,
            DetectorModel * m,
            {
                m->has_result = false;
                m->entry = NULL;
                m->page = 0;
                m->scroll = 0;
                m->noise_count = 0;
            },
            true);
        return true;
    case InputKeyBack:
        if(v->back_cb) v->back_cb(v->back_ctx);
        return true;
    default:
        return false;
    }
}

// ----------------------------------------------------------------- lifecycle

DetectorView* detector_view_alloc(void) {
    DetectorView* v = malloc(sizeof(DetectorView));
    v->view = view_alloc();
    v->back_cb = NULL;
    v->back_ctx = NULL;

    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(DetectorModel));
    with_view_model(v->view, DetectorModel * m, { memset(m, 0, sizeof(*m)); }, false);

    view_set_context(v->view, v);
    view_set_draw_callback(v->view, detector_view_draw);
    view_set_input_callback(v->view, detector_view_input);
    return v;
}

void detector_view_free(DetectorView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* detector_view_get_view(DetectorView* v) {
    furi_assert(v);
    return v->view;
}

void detector_view_set_back_callback(DetectorView* v, DetectorViewCallback cb, void* context) {
    furi_assert(v);
    v->back_cb = cb;
    v->back_ctx = context;
}

// -------------------------------------------------------------------- update

static void format_result(DetectorModel* m, const AcDetection* d) {
    m->has_result = true;
    m->kind = d->kind;
    m->entry = (d->kind == AcResultMatch) ? d->entry : NULL;
    m->scroll = 0;

    uint16_t frame_bits = d->repeats > 1 ? (uint16_t)(d->bits / d->repeats) : d->bits;

    if(d->repeats > 1) {
        snprintf(m->bits_str, sizeof(m->bits_str), "%ub x%u", frame_bits, d->repeats);
    } else {
        snprintf(m->bits_str, sizeof(m->bits_str), "%ub", frame_bits);
    }

    if(m->entry && m->entry->consumer) {
        // No remote app can help here: there is no mode, fan or temperature
        // field to drive. The stock Infrared app replays these fine.
        snprintf(m->use_str, sizeof(m->use_str), "Use the Infrared app");
    } else if(m->entry && strcmp(m->entry->app, "-")) {
        snprintf(m->use_str, sizeof(m->use_str), "Use: %s", m->entry->app);
    } else if(m->entry) {
        snprintf(m->use_str, sizeof(m->use_str), "No app for this one yet");
    } else {
        snprintf(m->use_str, sizeof(m->use_str), "%u edges captured", d->timings_count);
    }

    if(m->entry && m->entry->variant[0]) {
        snprintf(m->variant_str, sizeof(m->variant_str), "%s", m->entry->variant);
    } else if(m->entry) {
        snprintf(m->variant_str, sizeof(m->variant_str), "only one variant known");
    } else {
        snprintf(m->variant_str, sizeof(m->variant_str), "no match in the database");
    }

    if(d->sections > 1) {
        snprintf(
            m->shape_str,
            sizeof(m->shape_str),
            "%u bit, %u sections",
            frame_bits,
            d->sections / (d->repeats ? d->repeats : 1));
    } else {
        snprintf(m->shape_str, sizeof(m->shape_str), "%u bit", frame_bits);
    }

    snprintf(m->timing_hdr, sizeof(m->timing_hdr), "hdr %u/%u", d->hdr_mark, d->hdr_space);
    snprintf(
        m->timing_bit,
        sizeof(m->timing_bit),
        "bit %u  1/0 %u/%u",
        d->bit_mark,
        d->one_space,
        d->zero_space);

    // Split the payload evenly between the two rows, on a byte boundary. Both
    // rows scroll, so a long frame stays readable.
    char hex[AC_HEX_STR_LEN];
    ac_detection_format_hex(d, hex, sizeof(hex));

    size_t half_bytes = (size_t)(d->data_len + 1) / 2;
    size_t cut = half_bytes * 3; // "AA " per byte
    size_t len = strlen(hex);
    if(cut > len) cut = len;
    if(cut && hex[cut - 1] == ' ') cut--;

    size_t copy = cut < sizeof(m->payload_a) - 1 ? cut : sizeof(m->payload_a) - 1;
    memcpy(m->payload_a, hex, copy);
    m->payload_a[copy] = '\0';

    const char* rest = hex + cut;
    while(*rest == ' ')
        rest++;
    snprintf(m->payload_b, sizeof(m->payload_b), "%s", rest);
}

void detector_view_set_result(DetectorView* v, const AcDetection* d) {
    furi_assert(v);
    if(!d || d->kind == AcResultNoise) return;

    with_view_model(
        v->view,
        DetectorModel * m,
        {
            format_result(m, d);
            m->flash = FLASH_TICKS;
            m->flash_valid = true;
        },
        true);
}

void detector_view_note_noise(DetectorView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        DetectorModel * m,
        {
            if(m->noise_count < 99999) m->noise_count++;
            m->flash = FLASH_TICKS;
            m->flash_valid = false;
        },
        true);
}

void detector_view_tick(DetectorView* v) {
    furi_assert(v);
    bool redraw = false;
    with_view_model(
        v->view,
        DetectorModel * m,
        {
            if(m->flash) {
                m->flash--;
                redraw = true;
            }
            if(m->has_result) {
                m->scroll++;
                redraw = true;
            } else {
                m->idle_anim++;
                redraw = true;
            }
        },
        redraw);
}
