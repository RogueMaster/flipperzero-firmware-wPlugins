/* Canvas emulator: records the primitives, prints the picture and catches
 * text that ran off the screen or landed on top of something else.
 *
 * Character widths are taken with headroom (the real Flipper fonts are
 * narrower), so the test is pickier than the actual screen — as it
 * should be. */

#include "canvas_stub.h"

#include <gui/elements.h>

#define TPMS_MAX_ITEMS 96

#define TPMS_FONT_PRIMARY_WIDTH   7
#define TPMS_FONT_PRIMARY_ASCENT  8
#define TPMS_FONT_SECONDARY_WIDTH 6
#define TPMS_FONT_SECONDARY_ASCENT 7

static TpmsDrawItem g_items[TPMS_MAX_ITEMS];
static size_t g_count;
static Font g_font = FontSecondary;

static int tpms_font_width(void) {
    return g_font == FontPrimary ? TPMS_FONT_PRIMARY_WIDTH : TPMS_FONT_SECONDARY_WIDTH;
}

static int tpms_font_ascent(void) {
    return g_font == FontPrimary ? TPMS_FONT_PRIMARY_ASCENT : TPMS_FONT_SECONDARY_ASCENT;
}

static TpmsDrawItem* tpms_item_add(TpmsItemKind kind, int32_t x, int32_t y, int32_t w, int32_t h) {
    assert(g_count < TPMS_MAX_ITEMS);
    TpmsDrawItem* item = &g_items[g_count++];
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    item->x = x;
    item->y = y;
    item->w = w;
    item->h = h;
    return item;
}

void canvas_clear(Canvas* canvas) {
    UNUSED(canvas);
    g_count = 0;
}

void canvas_set_color(Canvas* canvas, Color color) {
    UNUSED(canvas);
    UNUSED(color);
}

void canvas_invert_color(Canvas* canvas) {
    UNUSED(canvas);
}

void canvas_set_font(Canvas* canvas, Font font) {
    UNUSED(canvas);
    g_font = font;
}

uint16_t canvas_string_width(Canvas* canvas, const char* str) {
    UNUSED(canvas);
    return (uint16_t)(strlen(str) * tpms_font_width());
}

void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str) {
    UNUSED(canvas);
    const int32_t width = (int32_t)(strlen(str) * tpms_font_width());
    const int32_t ascent = tpms_font_ascent();
    TpmsDrawItem* item = tpms_item_add(TpmsItemText, x, y - ascent, width, ascent);
    snprintf(item->text, sizeof(item->text), "%s", str);
}

void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* str) {
    const int32_t width = (int32_t)(strlen(str) * tpms_font_width());
    const int32_t ascent = tpms_font_ascent();

    if(horizontal == AlignRight) {
        x -= width;
    } else if(horizontal == AlignCenter) {
        x -= width / 2;
    }

    if(vertical == AlignTop) {
        y += ascent;
    } else if(vertical == AlignCenter) {
        y += ascent / 2;
    }

    canvas_draw_str(canvas, x, y, str);
}

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height) {
    UNUSED(canvas);
    tpms_item_add(TpmsItemShape, x, y, (int32_t)width, (int32_t)height);
}

void canvas_draw_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height) {
    canvas_draw_box(canvas, x, y, width, height);
}

void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y) {
    UNUSED(canvas);
    tpms_item_add(TpmsItemShape, x, y, 1, 1);
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    UNUSED(canvas);
    const int32_t x = x1 < x2 ? x1 : x2;
    const int32_t y = y1 < y2 ? y1 : y2;
    tpms_item_add(TpmsItemShape, x, y, (x1 > x2 ? x1 - x2 : x2 - x1) + 1, (y1 > y2 ? y1 - y2 : y2 - y1) + 1);
}

void elements_scrollbar_pos(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t height,
    size_t pos,
    size_t total) {
    UNUSED(canvas);
    UNUSED(pos);
    UNUSED(total);
    /* In the firmware the bar is 3 px wide and is drawn left of the
     * given edge. */
    tpms_item_add(TpmsItemShape, x - 3, y, 3, (int32_t)height);
}

size_t tpms_canvas_items(void) {
    return g_count;
}

const TpmsDrawItem* tpms_canvas_item(size_t index) {
    assert(index < g_count);
    return &g_items[index];
}

void tpms_canvas_print(const char* title) {
    static char grid[TPMS_CANVAS_HEIGHT][TPMS_CANVAS_WIDTH + 1];

    for(int y = 0; y < TPMS_CANVAS_HEIGHT; y++) {
        memset(grid[y], ' ', TPMS_CANVAS_WIDTH);
        grid[y][TPMS_CANVAS_WIDTH] = '\0';
    }

    for(size_t i = 0; i < g_count; i++) {
        const TpmsDrawItem* item = &g_items[i];

        if(item->kind == TpmsItemShape) {
            for(int y = item->y; y < item->y + item->h; y++) {
                for(int x = item->x; x < item->x + item->w; x++) {
                    if(x < 0 || x >= TPMS_CANVAS_WIDTH) continue;
                    if(y < 0 || y >= TPMS_CANVAS_HEIGHT) continue;
                    if(grid[y][x] == ' ') grid[y][x] = '#';
                }
            }
            continue;
        }

        /* Text goes into the row under the baseline, every character in
         * its own real pixel column. */
        const int row = item->y + item->h - 1;
        const int step = item->w / (int)strlen(item->text);
        for(size_t c = 0; item->text[c]; c++) {
            const int x = item->x + (int)c * step;
            if(x < 0 || x >= TPMS_CANVAS_WIDTH) continue;
            if(row < 0 || row >= TPMS_CANVAS_HEIGHT) continue;
            grid[row][x] = item->text[c];
        }
    }

    printf("\n== %s ==\n", title);
    printf("    +");
    for(int x = 0; x < TPMS_CANVAS_WIDTH; x++) putchar('-');
    printf("+\n");
    for(int y = 0; y < TPMS_CANVAS_HEIGHT; y++) {
        printf("%3d |%s|\n", y, grid[y]);
    }
    printf("    +");
    for(int x = 0; x < TPMS_CANVAS_WIDTH; x++) putchar('-');
    printf("+\n");
}

static bool tpms_rect_overlap(const TpmsDrawItem* a, const TpmsDrawItem* b) {
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

int tpms_canvas_check(const char* title) {
    int problems = 0;

    for(size_t i = 0; i < g_count; i++) {
        const TpmsDrawItem* item = &g_items[i];

        if(item->x < 0 || item->y < 0 || item->x + item->w > TPMS_CANVAS_WIDTH ||
           item->y + item->h > TPMS_CANVAS_HEIGHT) {
            printf(
                "FAIL [%s] off screen: %s x=%d y=%d w=%d h=%d\n",
                title,
                item->kind == TpmsItemText ? item->text : "shape",
                item->x,
                item->y,
                item->w,
                item->h);
            problems++;
        }

        if(item->kind != TpmsItemText) continue;

        for(size_t j = 0; j < g_count; j++) {
            if(j == i) continue;
            const TpmsDrawItem* other = &g_items[j];

            /* Text must not run into other text or into small graphics
             * (level bars, the scrollbar). Large shapes are the backdrop
             * of the selected row, which is exactly what it is for. */
            const bool interesting =
                other->kind == TpmsItemText || (other->kind == TpmsItemShape && other->w <= 8);
            if(!interesting) continue;
            if(other->kind == TpmsItemText && j < i) continue;

            if(tpms_rect_overlap(item, other)) {
                printf(
                    "FAIL [%s] overlap: '%s' and '%s'\n",
                    title,
                    item->text,
                    other->kind == TpmsItemText ? other->text : "graphics");
                problems++;
            }
        }
    }

    return problems;
}
