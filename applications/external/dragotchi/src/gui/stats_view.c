#include <furi.h>
#include <stdio.h>
#include <gui/view.h>
#include "stats_view.h"
#include "../economy.h"
#include "../constants.h"

typedef struct {
    uint8_t stage, alignment;
    uint32_t age_days, hunger, happiness, health, discipline, hoard;
    int32_t care;
    uint16_t eggs_common, eggs_rare;
} StatsModel;

static const uint8_t HEART[6] = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08};
static bool hon(int r, int c) { return (r>=0&&r<6&&c>=0&&c<7) && ((HEART[r]>>(6-c))&1); }
static void heart(Canvas *cv, int x, int y, bool f) {
    for(int r=0;r<6;r++) for(int c=0;c<7;c++){
        if(!hon(r,c)) continue;
        if(f) canvas_draw_dot(cv,x+c,y+r);
        else if(!hon(r-1,c)||!hon(r+1,c)||!hon(r,c-1)||!hon(r,c+1)) canvas_draw_dot(cv,x+c,y+r);
    }
}
static void row(Canvas *cv, int x, int y, const char *lab, uint32_t v) {
    canvas_draw_str(cv, x, y+6, lab);
    int fh=(int)((v*4+50)/100); if(fh<0)fh=0; if(fh>4)fh=4;
    for(int i=0;i<4;i++) heart(cv, x+14+i*8, y, i<fh);
}
static const char *care_word(int32_t c) {
    if(c>=80) return "Thriving";
    if(c>=60) return "Content";
    if(c>=40) return "OK";
    if(c>=20) return "Poor";
    return "Neglected";
}

static void stats_draw(Canvas *cv, void *model) {
    StatsModel *m = model;
    canvas_clear(cv);
    canvas_set_font(cv, FontSecondary);
    char buf[48];

    // title: alignment + stage, age at right
    if(m->stage == ADULT && m->alignment != ALIGN_NONE)
        snprintf(buf, sizeof(buf), "%s %s", ALIGNMENT_STRING[m->alignment], LIFE_STAGE_STRING[m->stage]);
    else
        snprintf(buf, sizeof(buf), "%s", LIFE_STAGE_STRING[m->stage]);
    canvas_set_font(cv, FontPrimary);
    canvas_draw_str(cv, 2, 9, buf);
    canvas_set_font(cv, FontSecondary);
    snprintf(buf, sizeof(buf), "%lud", (unsigned long)m->age_days);
    canvas_draw_str(cv, 108, 8, buf);
    canvas_draw_line(cv, 0, 12, 127, 12);

    row(cv, 2, 15, "Hu", m->hunger);
    row(cv, 66, 15, "Jo", m->happiness);
    row(cv, 2, 25, "Hp", m->health);
    canvas_draw_str(cv, 66, 31, care_word(m->care));

    snprintf(buf, sizeof(buf), "Rank: %s", hoard_rank(m->hoard));
    canvas_draw_str(cv, 2, 42, buf);
    snprintf(buf, sizeof(buf), "Hoard %lu   Disc %lu",
             (unsigned long)m->hoard, (unsigned long)m->discipline);
    canvas_draw_str(cv, 2, 52, buf);
    snprintf(buf, sizeof(buf), "Eggs %u common  %u rare",
             (unsigned)m->eggs_common, (unsigned)m->eggs_rare);
    canvas_draw_str(cv, 2, 62, buf);
}

View *stats_view_alloc(void *context) {
    View *v = view_alloc();
    view_set_context(v, context);
    view_set_draw_callback(v, stats_draw);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(StatsModel));
    return v;
}
void stats_view_free(View *v) { view_free(v); }

void stats_view_update(View *v, const struct GameState *gs, uint32_t age_days) {
    const struct PersistentGameState *p = &gs->persistent;
    with_view_model(v, StatsModel * m, {
        m->stage=p->stage; m->alignment=p->alignment; m->age_days=age_days;
        m->hunger=p->hunger; m->happiness=p->happiness; m->health=p->health;
        m->care=p->care_score; m->discipline=p->discipline; m->hoard=p->hoard;
        m->eggs_common=p->eggs_common; m->eggs_rare=p->eggs_rare;
    }, true);
}
