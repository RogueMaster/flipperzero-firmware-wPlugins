#include "cw_markdown_widget.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct Canvas {
    Font font;
    uint16_t draw_calls;
    uint16_t xbm_calls;
    uint16_t dot_calls;
    uint16_t box_calls;
    uint16_t left_buttons;
    uint16_t right_buttons;
    char drawn[512];
};

static uint16_t char_width(Font font, char ch) {
    if(ch == '\0') return 0U;
    if(ch == ' ') return 3U;
    if(font == FontKeyboard) return 4U;
    if(font == FontPrimary) return ch == 'W' ? 7U : 6U;
    return ch == 'W' ? 6U : 5U;
}

void canvas_set_font(Canvas* canvas, Font font) {
    canvas->font = font;
}

uint16_t canvas_string_width(Canvas* canvas, const char* str) {
    uint16_t w = 0U;
    while(*str) {
        w += char_width(canvas->font, *str);
        str++;
    }
    return w;
}

void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str) {
    size_t len;
    (void)x;
    (void)y;
    canvas->draw_calls++;
    len = strlen(canvas->drawn);
    snprintf(canvas->drawn + len, sizeof(canvas->drawn) - len, "%s|", str);
}

void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align h,
    Align v,
    const char* str) {
    (void)x;
    (void)y;
    (void)h;
    (void)v;
    canvas_draw_str(canvas, 0, 0, str);
}

void canvas_draw_xbm(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    uint8_t width,
    uint8_t height,
    const uint8_t* bitmap) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)bitmap;
    canvas->xbm_calls++;
}

void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y) {
    (void)x;
    (void)y;
    canvas->dot_calls++;
}

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, uint8_t width, uint8_t height) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    canvas->box_calls++;
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)canvas;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
}

void canvas_set_color(Canvas* canvas, Color color) {
    (void)canvas;
    (void)color;
}

void elements_button_left(Canvas* canvas, const char* str) {
    (void)str;
    canvas->left_buttons++;
}

void elements_button_right(Canvas* canvas, const char* str) {
    (void)str;
    canvas->right_buttons++;
}

static Canvas canvas_new(void) {
    Canvas canvas = {0};
    canvas.font = FontSecondary;
    return canvas;
}

static CwmdConfig cfg_narrow(void) {
    CwmdConfig cfg;
    cwmd_config_default(&cfg, false);
    cfg.width = 40U;
    cfg.height = 24U;
    cfg.scrollbar = false;
    return cfg;
}

static void test_default_config(void) {
    CwmdConfig cfg;
    cwmd_config_default(&cfg, true);
    assert(cfg.x == 1U);
    assert(cfg.y == 1U);
    assert(cfg.width == 126U);
    assert(cfg.height == 50U);
    assert(cfg.line_height == 9U);
    assert(cfg.scrollbar);
    assert((cfg.chrome & CwmdChromeCenter) != 0U);

    cwmd_config_default(&cfg, false);
    assert(cfg.height == 62U);
    assert(cfg.chrome == CwmdChromeNone);
}

static void test_wrap_height_and_scroll(void) {
    Canvas canvas = canvas_new();
    CwmdConfig cfg = cfg_narrow();
    CwmdState state = {0};

    assert(cwmd_content_height(&canvas, &cfg, "one two three four five") > cfg.line_height);
    assert(cwmd_max_scroll_px(&canvas, &cfg, "one two three four five six seven") > 0);
    cwmd_scroll_step(&state, 1, 40, cfg.line_height);
    assert(state.target_scroll_px == 9);
    assert(state.scroll_px == 0);
    assert(cwmd_scroll_tick(&state));
    assert(state.scroll_px == 1);
    assert(cwmd_scroll_tick(&state));
    assert(state.scroll_px == 2);
    cwmd_scroll_step(&state, -1, 40, cfg.line_height);
    assert(state.target_scroll_px == 0);

    state = (CwmdState){0};
    cwmd_scroll_step(&state, 1, 100, 36U);
    assert(state.target_scroll_px == 36);
    for(uint8_t i = 0U; i < 36U; i++) {
        assert(cwmd_scroll_tick(&state));
    }
    assert(state.scroll_px == 36);
}

static void test_escapes_ascii_and_malformed_input(void) {
    Canvas canvas = canvas_new();
    CwmdConfig cfg = cfg_narrow();
    CwmdTestStats stats;
    const char bad[] = {'A', '\t', (char)0x01, (char)0x80, '\0'};

    cwmd_test_stats(
        &canvas,
        &cfg,
        "\x1b"
        "ccenter\nleft",
        &stats);
    assert(stats.centered == 1U);
    assert(stats.lines == 2U);
    cwmd_test_stats(
        &canvas,
        &cfg,
        "\x1b"
        "#bold\nplain",
        &stats);
    assert(stats.bold_atoms >= 1U);
    assert(stats.lines == 2U);
    cwmd_test_stats(
        &canvas,
        &cfg,
        "\x1b"
        "*mono\nplain",
        &stats);
    assert(stats.mono_atoms >= 1U);
    assert(stats.lines == 2U);
    cwmd_test_stats(&canvas, &cfg, bad, &stats);
    assert(stats.sanitized == 2U);
    cwmd_test_stats(
        &canvas,
        &cfg,
        "\x1b"
        "x",
        &stats);
    assert(stats.unknown_icons == 0U);
}

static void test_icons(void) {
    static const uint8_t xbm[] = {0x0f};
    static const CwmdIcon icons[] = {
        {.id = 1U, .width = 4U, .height = 4U, .left_bearing = 1U, .right_bearing = 2U, .xbm = xbm},
        {.id = 10U, .width = 8U, .height = 5U, .left_bearing = 0U, .right_bearing = 1U, .xbm = xbm},
        {.id = 99U, .width = 30U, .height = 7U, .left_bearing = 2U, .right_bearing = 2U, .xbm = xbm},
    };
    Canvas canvas = canvas_new();
    CwmdConfig cfg = cfg_narrow();
    CwmdTestStats stats;
    cfg.icons = icons;
    cfg.icon_count = 3U;

    cwmd_test_stats(
        &canvas,
        &cfg,
        "a \x1b"
        "x1 b \x1b"
        "x01 c \x1b"
        "x10 d \x1b"
        "x99",
        &stats);
    assert(stats.icons == 4U);
    assert(stats.unknown_icons == 0U);
    cwmd_test_stats(
        &canvas,
        &cfg,
        "\x1b"
        "x1 starts a line",
        &stats);
    assert(stats.icons == 1U);
    cwmd_test_stats(
        &canvas,
        &cfg,
        "\x1b"
        "x9 \x1b"
        "x09 \x1b"
        "x11",
        &stats);
    assert(stats.icons == 0U);
    assert(stats.unknown_icons == 3U);
    cwmd_draw(
        &canvas,
        &cfg,
        NULL,
        "a \x1b"
        "x1 b");
    assert(canvas.xbm_calls == 1U);
}

static void test_inline_markdown(void) {
    Canvas canvas = canvas_new();
    CwmdConfig cfg = cfg_narrow();
    CwmdTestStats stats;
    cwmd_test_stats(&canvas, &cfg, "plain __bold__ `mono` \\_ \\`", &stats);
    assert(stats.bold_atoms >= 1U);
    assert(stats.mono_atoms >= 1U);
    cwmd_test_stats(&canvas, &cfg, "__literal\nnext", &stats);
    assert(stats.bold_atoms == 0U);
    assert(stats.lines == 2U);
}

static void test_bullets_and_justification(void) {
    Canvas canvas = canvas_new();
    CwmdConfig cfg = cfg_narrow();
    CwmdTestStats stats;
    cwmd_test_stats(&canvas, &cfg, "- one two three four five", &stats);
    assert(stats.bullets == 1U);
    cwmd_test_stats(&canvas, &cfg, "not - a bullet", &stats);
    assert(stats.bullets == 0U);

    cfg.width = 70U;
    cwmd_test_stats(&canvas, &cfg, "aa bb cc dd ee ff gg", &stats);
    assert(stats.justified >= 1U);
    assert(stats.last_extra_gap_px <= 2);
    cfg.width = 123U;
    cwmd_test_stats(&canvas, &cfg, "something more organised tail", &stats);
    assert(stats.justified >= 1U);
    assert(stats.last_extra_gap_px == 3);
}

static void test_microfit(void) {
    Canvas canvas = canvas_new();
    CwmdConfig cfg = cfg_narrow();
    CwmdTestStats stats;

    cfg.width = 35U;
    cwmd_test_stats(&canvas, &cfg, "aa. b cc", &stats);
    assert(stats.lines == 1U);
    assert(stats.microfit_lines == 1U);
    assert(stats.last_microfit_shrink_px == 1);
    assert(stats.last_line_width == 35);

    cfg.width = 38U;
    cwmd_test_stats(&canvas, &cfg, "aa. b ccc", &stats);
    assert(stats.lines == 2U);
    assert(stats.microfit_lines == 0U);
}

static void test_soft_hyphen_markers(void) {
    Canvas canvas = canvas_new();
    CwmdConfig cfg = cfg_narrow();

    cfg.width = 50U;
    cwmd_draw(&canvas, &cfg, NULL, "fil~ter~ing");
    assert(strcmp(canvas.drawn, "filtering|") == 0);
    canvas = canvas_new();
    cfg.height = 40U;
    cfg.width = 40U;
    cwmd_draw(&canvas, &cfg, NULL, "fil~ter~ing");
    assert(strcmp(canvas.drawn, "filter-|ing|") == 0);
    canvas = canvas_new();
    cfg.width = 25U;
    cwmd_draw(&canvas, &cfg, NULL, "fil~ter~ing");
    assert(strcmp(canvas.drawn, "fil-|ter-|ing|") == 0);
    canvas = canvas_new();
    cfg.width = 80U;
    cwmd_draw(&canvas, &cfg, NULL, "foo~~bar");
    assert(strcmp(canvas.drawn, "foo~bar|") == 0);
}

static void test_chrome_and_scrollbar_draw(void) {
    Canvas canvas = canvas_new();
    CwmdConfig cfg;
    CwmdState state = {0};
    cwmd_config_default(&cfg, true);
    cfg.height = 16U;
    cfg.center_label = "1/2";
    cwmd_draw(&canvas, &cfg, &state, "one two three four five six seven eight");
    assert(state.max_scroll_px > 0);
    assert(canvas.left_buttons == 1U);
    assert(canvas.right_buttons == 1U);
    assert(canvas.box_calls > 0U);
}

int main(void) {
    test_default_config();
    test_wrap_height_and_scroll();
    test_escapes_ascii_and_malformed_input();
    test_icons();
    test_inline_markdown();
    test_bullets_and_justification();
    test_microfit();
    test_soft_hyphen_markers();
    test_chrome_and_scrollbar_draw();
    return 0;
}
