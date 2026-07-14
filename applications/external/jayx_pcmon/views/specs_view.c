#include "../jayx.h"

static void draw_centered_fit(Canvas* canvas, int cx, int y, const char* text, size_t max_w) {
    if(!text || !text[0]) return;
    FuriString* fs = furi_string_alloc_set(text);
    elements_string_fit_width(canvas, fs, max_w);
    canvas_draw_str_aligned(canvas, cx, y, AlignCenter, AlignBottom, furi_string_get_cstr(fs));
    furi_string_free(fs);
}

/** Break a long string on a natural separator for two readable lines. */
static void split_value(const char* src, char* line_a, size_t a_len, char* line_b, size_t b_len) {
    line_a[0] = '\0';
    line_b[0] = '\0';
    if(!src || !src[0]) return;

    size_t len = strlen(src);
    if(len < 20) {
        snprintf(line_a, a_len, "%s", src);
        return;
    }

    size_t mid = len / 2;
    size_t break_at = 0;
    for(size_t i = mid; i > 6; i--) {
        if(src[i] == ' ' || src[i] == '-' || src[i] == '/' || src[i] == '|') {
            break_at = i;
            break;
        }
    }
    if(break_at == 0) {
        for(size_t i = mid; i + 3 < len; i++) {
            if(src[i] == ' ' || src[i] == '-' || src[i] == '/' || src[i] == '|') {
                break_at = i;
                break;
            }
        }
    }

    if(break_at == 0 || break_at + 1 >= len) {
        snprintf(line_a, a_len, "%s", src);
        return;
    }

    size_t n = break_at;
    if(n >= a_len) n = a_len - 1;
    memcpy(line_a, src, n);
    line_a[n] = '\0';

    size_t start = break_at;
    while(src[start] == ' ' || src[start] == '-' || src[start] == '|' || src[start] == '/')
        start++;
    snprintf(line_b, b_len, "%s", src + start);
}

static void draw_section_pills(Canvas* canvas, uint8_t n, uint8_t active) {
    if(n == 0) return;

    const int y = 52;
    const int spacing = 9;
    int total_w = (int)(n - 1) * spacing;
    int start_x = 64 - total_w / 2;

    for(uint8_t i = 0; i < n; i++) {
        int x = start_x + (int)i * spacing;
        if(i == active) {
            canvas_draw_disc(canvas, x, y, 2);
        } else {
            canvas_draw_circle(canvas, x, y, 2);
        }
    }
}

void draw_specs_view(Canvas* canvas, JayxApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Top bar */
    canvas_draw_box(canvas, 0, 0, 128, JAYX_SPECS_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 8, "SYSTEM INFO");

    if(!app->specs_valid) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, "Collecting");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, "Run pc_agent on PC");
        jayx_draw_page_dots(canvas, JayxPageSpecs);
        return;
    }

    jayx_specs_clamp_scroll(app);
    uint8_t idx = app->specs_scroll;
    uint8_t nsec = app->specs_count;
    if(nsec > JAYX_SPEC_SECTIONS_MAX) nsec = JAYX_SPEC_SECTIONS_MAX;
    if(idx >= nsec) idx = 0;

    JayxSpecSection* sec = &app->specs[idx];

    char pos[10];
    snprintf(pos, sizeof(pos), "%u/%u", (unsigned)(idx + 1), (unsigned)nsec);
    canvas_draw_str_aligned(canvas, 124, 8, AlignRight, AlignBottom, pos);

    canvas_set_color(canvas, ColorBlack);

    /* Section title in a framed band */
    canvas_draw_rframe(canvas, 4, 14, 120, 13, 2);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, sec->title);

    /* Build display lines from section data */
    char lines[4][40];
    uint8_t nlines = 0;

    if(sec->line[0][0]) {
        char a[40], b[40];
        split_value(sec->line[0], a, sizeof(a), b, sizeof(b));
        snprintf(lines[nlines++], sizeof(lines[0]), "%s", a);
        if(b[0] && nlines < 4) {
            snprintf(lines[nlines++], sizeof(lines[0]), "%s", b);
        }
    }
    for(uint8_t L = 1; L < JAYX_SPEC_LINES && nlines < 3; L++) {
        if(sec->line[L][0]) {
            snprintf(lines[nlines++], sizeof(lines[0]), "%s", sec->line[L]);
        }
    }

    canvas_set_font(canvas, FontSecondary);
    if(nlines == 0) {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, "—");
    } else if(nlines == 1) {
        draw_centered_fit(canvas, 64, 40, lines[0], 118);
    } else if(nlines == 2) {
        draw_centered_fit(canvas, 64, 36, lines[0], 118);
        draw_centered_fit(canvas, 64, 46, lines[1], 118);
    } else {
        draw_centered_fit(canvas, 64, 34, lines[0], 118);
        draw_centered_fit(canvas, 64, 43, lines[1], 118);
        draw_centered_fit(canvas, 64, 52, lines[2], 118);
        /* skip pills when 3 lines — tight; still show page dots */
        jayx_draw_page_dots(canvas, JayxPageSpecs);
        return;
    }

    draw_section_pills(canvas, nsec, idx);
    jayx_draw_page_dots(canvas, JayxPageSpecs);
}
