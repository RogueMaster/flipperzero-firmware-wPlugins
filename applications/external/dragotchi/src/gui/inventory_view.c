#include <furi.h>
#include <stdio.h>
#include <gui/view.h>
#include "inventory_view.h"

typedef struct {
    uint16_t ts, tm, tl;   // treasure small/med/large
    uint32_t hoard;
    uint16_t eggs_common, eggs_rare, eggs_storm;
    uint16_t prey_caught, eggs_caught;
} InvModel;

static void inv_draw(Canvas *cv, void *model) {
    InvModel *m = model;
    canvas_clear(cv);
    canvas_set_font(cv, FontPrimary);
    canvas_draw_str(cv, 2, 9, "Inventory");
    canvas_draw_line(cv, 0, 12, 127, 12);
    canvas_set_font(cv, FontSecondary);
    char buf[48];
    snprintf(buf, sizeof(buf), "Treasure  S:%u M:%u L:%u", (unsigned)m->ts, (unsigned)m->tm, (unsigned)m->tl);
    canvas_draw_str(cv, 2, 23, buf);
    snprintf(buf, sizeof(buf), "Hoard value: %lu", (unsigned long)m->hoard);
    canvas_draw_str(cv, 2, 33, buf);
    snprintf(buf, sizeof(buf), "Eggs C:%u R:%u Storm:%u", (unsigned)m->eggs_common, (unsigned)m->eggs_rare, (unsigned)m->eggs_storm);
    canvas_draw_str(cv, 2, 43, buf);
    unsigned total = m->prey_caught + m->ts + m->tm + m->tl + m->eggs_caught;
    snprintf(buf, sizeof(buf), "Caught  prey:%u  eggs:%u", (unsigned)m->prey_caught, (unsigned)m->eggs_caught);
    canvas_draw_str(cv, 2, 53, buf);
    snprintf(buf, sizeof(buf), "Total catches: %u", total);
    canvas_draw_str(cv, 2, 63, buf);
}

View *inventory_view_alloc(void *context) {
    View *v = view_alloc();
    view_set_context(v, context);
    view_set_draw_callback(v, inv_draw);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(InvModel));
    return v;
}
void inventory_view_free(View *v) { view_free(v); }

void inventory_view_update(View *v, const struct GameState *gs) {
    const struct PersistentGameState *p = &gs->persistent;
    with_view_model(v, InvModel * m, {
        m->ts=p->treasure_small; m->tm=p->treasure_med; m->tl=p->treasure_large;
        m->hoard=p->hoard; m->eggs_common=p->eggs_common; m->eggs_rare=p->eggs_rare; m->eggs_storm=p->eggs_storm;
        m->prey_caught=p->prey_caught; m->eggs_caught=p->eggs_caught;
    }, true);
}
