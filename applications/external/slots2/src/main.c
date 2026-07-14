#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "include/icons.h"
#include "include/konami.h"
#include "include/pcg32.h"
#include "include/slots.h"

static pcg32_t rng = {0};

static const int start_x = 2;
static const int start_y = 2;
static const int sidebar_x = 88;
static const int icon_size = 20;

KONAMI(money, {UP, OK, UP, DOWN, DOWN, BACK});
KONAMI(sprites, {LEFT, RIGHT, BACK, DOWN, OK, UP});
KONAMI(rigged, {UP, UP, DOWN, DOWN, LEFT, RIGHT, OK});
KONAMI(free_play, {UP, DOWN, UP, OK, OK, OK});
KONAMI(nuke, {UP, DOWN, LEFT, RIGHT, BACK, UP});
KONAMI(rtp, {LEFT, RIGHT, OK, BACK, LEFT, RIGHT});
KONAMI(high_roller, {UP, UP, OK, UP, UP, OK});

static inline void draw_sprites(Canvas* canvas) {
    int x = 0, y = 0;
    for(int sym = 0; sym < SYM_MAX; sym++) {
        const uint8_t* bitmap = get_icon(get_icon_from_symbol(sym));
        canvas_draw_xbm(canvas, x, y, icon_size, icon_size, bitmap);

        if((sym + 1) % 5 == 0) {
            y += icon_size;
            x = 0;
        } else
            x += icon_size;
    }
}

static inline void draw_rtp(Canvas* canvas, double rtp) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%g%%", rtp);
    canvas_draw_str_aligned(canvas, 2, 2, AlignLeft, AlignTop, "RTP over 500 spins:");
    canvas_draw_str_aligned(canvas, 2, 12, AlignLeft, AlignTop, buffer);
}

static inline void draw_reels(Canvas* canvas, slot_machine_t* sm) {
    for(int x = 0; x < 4; x++) {
        int x_pos = start_x + x + (x * icon_size);
        float offset = sm->reel_offsets[x];

        for(int y = -1; y < 3; y++) {
            int y_pos = start_y + offset + (y * icon_size);

            symbol_t sym;
            if(sm->state == STATE_IDLE) {
                if(y < 0) continue;
                sym = sm->reels[x][y];
            } else
                sym = (y < 0) ? sm->next_reels[x][0] : sm->reels[x][y];

            const uint8_t* bitmap = get_icon(get_icon_from_symbol(sym));

            if(y_pos > -icon_size && y_pos < 62)
                canvas_draw_xbm(canvas, x_pos, y_pos, icon_size, icon_size, bitmap);
        }
    }

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_frame(canvas, 0, 0, 7 + (4 * icon_size), 4 + (3 * icon_size));
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rframe(
        canvas, start_x - 1, start_y - 1, 5 + (4 * icon_size), 2 + (3 * icon_size), 2);

    for(int i = 1; i < 4; i++) {
        int line_x = start_x + i - 1 + (i * icon_size);
        canvas_draw_line(canvas, line_x, start_y, line_x, start_y - 1 + (3 * icon_size));
    }

    canvas_draw_line(canvas, sidebar_x, 0, sidebar_x, 63);
}

static inline void draw_sidebar(Canvas* canvas, slot_machine_t* sm) {
    char buffer[32];

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_box(canvas, sidebar_x + 2, 0, 125 - sidebar_x, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 108, 1, AlignCenter, AlignTop, "CASH");
    canvas_set_color(canvas, ColorBlack);

    snprintf(buffer, sizeof(buffer), "$%lu", sm->credits);
    canvas_draw_str_aligned(canvas, 108, 11, AlignCenter, AlignTop, buffer);

    canvas_draw_line(canvas, sidebar_x + 4, 20, 124, 20);
    canvas_draw_str_aligned(canvas, 105, 23, AlignCenter, AlignTop, "BET");

    snprintf(buffer, sizeof(buffer), "$%lu", sm->bet);
    canvas_draw_str_aligned(canvas, 108, 33, AlignCenter, AlignTop, buffer);

    if(sm->bet_index < 7) canvas_draw_xbm(canvas, 115, 23, 5, 3, get_icon(ICON_ARROW_UP));
    if(sm->bet_index > 0) canvas_draw_xbm(canvas, 115, 27, 5, 3, get_icon(ICON_ARROW_DOWN));

    if(sm->last_win > 0) {
        canvas_draw_rbox(canvas, sidebar_x + 2, 43, 124 - sidebar_x, 20, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 108, 45, AlignCenter, AlignTop, "WIN!");

        snprintf(buffer, sizeof(buffer), "$%lu", sm->last_win);
        canvas_draw_str_aligned(canvas, 108, 54, AlignCenter, AlignTop, buffer);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    slot_machine_t* sm = ctx;
    canvas_clear(canvas);

    if(sm->state == STATE_SPRITES)
        draw_sprites(canvas);

    else if(sm->state == STATE_RTP)
        draw_rtp(canvas, (double)sm->rtp);

    else {
        draw_reels(canvas, sm);
        draw_sidebar(canvas, sm);
    }
}

static void input_callback(InputEvent* ev, void* ctx) {
    FuriMessageQueue* ev_queue = ctx;
    furi_message_queue_put(ev_queue, ev, FuriWaitForever);
}

int slots_app(void* p) {
    (void)p;

    pcg32_seed(&rng);
    slot_machine_t* sm = slot_machine_create();
    spin_reels(&rng, sm);

    FuriMessageQueue* ev_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);

    view_port_draw_callback_set(view_port, render_callback, sm);
    view_port_input_callback_set(view_port, input_callback, ev_queue);

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        uint32_t timeout = sm->state == STATE_SPINNING ? 33 : FuriWaitForever;

        InputEvent event;
        FuriStatus ev_status = furi_message_queue_get(ev_queue, &event, timeout);

        if(ev_status == FuriStatusOk) {
            if(event.type != InputTypeShort) continue;

            bool matched = false;
            CHECK_KONAMI(money, &event, &matched, sm);
            CHECK_KONAMI(sprites, &event, &matched, sm);
            CHECK_KONAMI(rigged, &event, &matched, sm);
            CHECK_KONAMI(free_play, &event, &matched, sm);
            CHECK_KONAMI(nuke, &event, &matched, sm);
            CHECK_KONAMI(rtp, &event, &matched, sm);
            CHECK_KONAMI(high_roller, &event, &matched, sm);

            if(event.key == BACK) {
                if(sm->state == STATE_SPRITES || sm->state == STATE_RTP)
                    sm->state = STATE_IDLE;
                else if(!matched)
                    running = false;
            } else if(sm->state == STATE_IDLE) {
                if(event.key == OK && !matched)
                    start_game(&rng, sm);
                else if(event.key == UP)
                    inc_bet(sm);
                else if(event.key == DOWN)
                    dec_bet(sm);
            }
        }

        else if(ev_status == FuriStatusErrorTimeout) {
            if(sm->state == STATE_SPINNING) animate_reels(&rng, sm);
        }

        view_port_update(view_port);
    }

    gui_remove_view_port(gui, view_port);

    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(ev_queue);

    slot_machine_destroy(sm);

    return 0;
}
