#include <furi.h>
#include <stdio.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "pet_view.h"
#include "game_decoder.h"
#include "../flipper_structs.h"

typedef struct {
    uint8_t stage, alignment, display_state;
    uint32_t hunger, happiness, health;
    uint8_t poop, sick, night, call;
    uint32_t frame;
    uint8_t reveal_ticks;
    char reveal_text[24];
    uint8_t on_expedition;
    uint32_t remaining_sec;
    uint8_t board_present;
    uint8_t last_wifi;
} PetModel;

static const uint8_t HEART[6] = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08};
static bool heart_on(int r, int c) { return (r >= 0 && r < 6 && c >= 0 && c < 7) && ((HEART[r] >> (6 - c)) & 1); }
static void draw_meter_heart(Canvas *c, int x, int y, bool filled) {
    for(int r = 0; r < 6; r++)
        for(int col = 0; col < 7; col++) {
            if(!heart_on(r, col)) continue;
            if(filled) canvas_draw_dot(c, x + col, y + r);
            else if(!heart_on(r-1,col)||!heart_on(r+1,col)||!heart_on(r,col-1)||!heart_on(r,col+1))
                canvas_draw_dot(c, x + col, y + r);
        }
}
static void draw_meter_row(Canvas *c, int y, const char *label, uint32_t value) {
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 66, y + 6, label);
    int fh = (int)((value * 4 + 50) / 100);
    if(fh < 0) fh = 0;
    if(fh > 4) fh = 4;
    for(int i = 0; i < 4; i++) draw_meter_heart(c, 82 + i * 8, y, i < fh);
}
static void draw_mini_heart(Canvas *c, int x, int y) {
    canvas_draw_dot(c,x,y);canvas_draw_dot(c,x+2,y);
    canvas_draw_dot(c,x-1,y+1);canvas_draw_dot(c,x+1,y+1);canvas_draw_dot(c,x+3,y+1);
    canvas_draw_dot(c,x,y+2);canvas_draw_dot(c,x+1,y+2);canvas_draw_dot(c,x+2,y+2);
    canvas_draw_dot(c,x+1,y+3);
}
static void draw_state_overlay(Canvas *c, uint8_t ds, uint32_t frame) {
    int p = (int)(frame & 1u);
    switch(ds) {
        case DISP_SLEEPING:
            canvas_set_font(c, FontSecondary);
            canvas_draw_str(c, 40, 12 - p*2, "z"); canvas_draw_str(c, 46, 8 - p*2, "Z");
            break;
        case DISP_SICK: canvas_draw_disc(c, 46, 14 + p*2, 1); break;
        case DISP_PLAYING: draw_mini_heart(c, 42, 8 - p*2); break;
        case DISP_EATING:
            canvas_draw_dot(c,8+p,34);canvas_draw_dot(c,12,36-p);canvas_draw_dot(c,6,37); break;
        case DISP_EVOLVING: {
            const int sx[4]={6,58,10,54}, sy[4]={6,8,50,46};
            for(int i=0;i<4;i++){ int x=sx[i], y=sy[i]+(p?0:1);
                canvas_draw_line(c,x-2,y,x+2,y); canvas_draw_line(c,x,y-2,x,y+2);} break;
        }
        default: break;
    }
}
static void draw_poop(Canvas *c, int x, int y) {
    canvas_draw_disc(c,x,y,2); canvas_draw_dot(c,x-1,y-2); canvas_draw_dot(c,x+1,y-3); canvas_draw_dot(c,x,y-4);
}

static void pet_draw_callback(Canvas *canvas, void *model) {
    PetModel *m = model;
    canvas_clear(canvas);

    if(m->on_expedition) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 16, 20, "On Expedition");
        canvas_set_font(canvas, FontSecondary);
        char rb[32];
        uint32_t s = m->remaining_sec;
        snprintf(rb, sizeof(rb), "Returns in %luh %02lum",
                 (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
        canvas_draw_str(canvas, 18, 36, rb);
        canvas_draw_str(canvas, 22, 52, m->frame & 1u ? "adventuring . ." : "adventuring . . .");
        canvas_draw_line(canvas, 0, 0, 127, 0);
        return;
    }

    int dx = 0, dy = 0, p = (int)(m->frame & 1u);
    switch(m->display_state) {
        case DISP_IDLE: dy = p ? 0 : 1; break;
        case DISP_SICK: dx = p ? 1 : -1; break;
        case DISP_PLAYING: dy = p ? -2 : 0; break;
        case DISP_EATING: dx = p; break;
        default: break;
    }
    canvas_draw_icon(canvas, 2 + dx, 2 + dy,
                     decode_image_for(m->stage, m->alignment, m->display_state, m->frame));
    draw_state_overlay(canvas, m->display_state, m->frame);
    if(m->poop) draw_poop(canvas, 52, 50);
    if(m->call) { canvas_set_font(canvas, FontPrimary); canvas_draw_str(canvas, 34, 12, "!"); }

    draw_meter_row(canvas, 4, "Hu", m->hunger);
    draw_meter_row(canvas, 17, "Jo", m->happiness);
    draw_meter_row(canvas, 30, "Hp", m->health);

    // bottom strip: reveal banner (if active) else the control hint
    canvas_set_font(canvas, FontSecondary);
    if(m->reveal_ticks) {
        /* Plain sub-GHz catch: single-line banner tagged "RF". (Board-assisted
         * catches get their own animated Signal Storm screen instead.) */
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 52, 128, 12);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 3, 61, m->reveal_text);
        canvas_draw_str(canvas, 128 - canvas_string_width(canvas, "RF") - 3, 61, "RF");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_line(canvas, 0, 52, 127, 52);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 3, 62, "OK");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 20, 61, "Menu");
        canvas_draw_str(canvas, 95, 61, "Back:exit");
    }
}

static bool pet_input_callback(InputEvent *event, void *context) {
    struct ApplicationContext *app = (struct ApplicationContext *)context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PET_EVT_MENU);
        return true;
    }
    return false; // Back handled by navigation callback
}

View *pet_view_alloc(void *context) {
    View *view = view_alloc();
    view_set_context(view, context);
    view_set_draw_callback(view, pet_draw_callback);
    view_set_input_callback(view, pet_input_callback);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(PetModel));
    with_view_model(view, PetModel * m, { m->stage = EGG; }, true);
    return view;
}
void pet_view_free(View *view) { view_free(view); }

void pet_view_update(View *view, const struct GameState *gs, bool night, uint32_t remaining_sec) {
    const struct PersistentGameState *p = &gs->persistent;
    with_view_model(
        view, PetModel * m,
        {
            m->stage=p->stage; m->alignment=p->alignment; m->display_state=gs->display_state;
            m->hunger=p->hunger; m->happiness=p->happiness; m->health=p->health;
            m->poop=p->poop; m->sick=p->sick; m->night=night?1:0;
            m->call=p->attention_call; m->frame=gs->next_animation_index;
            m->reveal_ticks=gs->reveal_ticks;
            for(size_t i=0;i<sizeof(m->reveal_text);i++){ m->reveal_text[i]=gs->reveal_text[i]; if(!gs->reveal_text[i]) break; }
            m->reveal_text[sizeof(m->reveal_text)-1]='\0';
            m->on_expedition=p->on_expedition; m->remaining_sec=remaining_sec;
            m->board_present=gs->board_present; m->last_wifi=gs->last_wifi;
        },
        true);
}
