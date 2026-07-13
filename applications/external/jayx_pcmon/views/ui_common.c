#include "../jayx.h"

void jayx_draw_page_dots(Canvas* canvas, JayxPage page) {
    const int spacing = 10;
    const int start_x = 64 - ((JAYX_PAGE_COUNT - 1) * spacing) / 2;

    for(int i = 0; i < JAYX_PAGE_COUNT; i++) {
        int x = start_x + i * spacing;
        if((JayxPage)i == page) {
            canvas_draw_disc(canvas, x, JAYX_DOT_Y, 2);
        } else {
            canvas_draw_circle(canvas, x, JAYX_DOT_Y, 2);
        }
    }
}
