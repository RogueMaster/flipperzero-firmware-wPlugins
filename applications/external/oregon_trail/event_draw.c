#include "event_draw.h"
#include "sprites.h"
#include <stdio.h>
#include <string.h>

#define VISIBLE_LINES 4
#define LINE_H        8
#define BODY_TOP      20
#define CHOICE_TOP    54

void event_draw(Canvas* c, const ActiveEvent* ev, const GameState* gs) {
    if(!ev->active || !ev->def) return;

    const EventDef* def = ev->def;
    const char* player_name = gs->players[ev->affected_player].name;

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    // ── Border ───────────────────────────────────────────────
    canvas_draw_frame(c, 0, 0, 128, 64);

    // ── Inverted header ──────────────────────────────────────
    canvas_draw_box(c, 1, 1, 126, 10);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, def->header);
    canvas_set_color(c, ColorBlack);

    // ── Scrollable body ──────────────────────────────────────
    // Count total body lines
    int total_lines = 0;
    while(total_lines < MAX_BODY_LINES && def->body[total_lines]) total_lines++;

    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < VISIBLE_LINES; i++) {
        int li = ev->scroll_y + i;
        if(li >= total_lines) break;

        const char* raw = def->body[li];
        char rendered[32];

        // Substitute %s with player name if present
        if(strstr(raw, "%s")) {
            snprintf(rendered, sizeof(rendered), raw, player_name);
        } else {
            strncpy(rendered, raw, sizeof(rendered) - 1);
            rendered[sizeof(rendered) - 1] = '\0';
        }

        canvas_draw_str(c, 3, BODY_TOP + i * LINE_H, rendered);
    }

    // ── Scroll thumb ─────────────────────────────────────────
    if(total_lines > VISIBLE_LINES) {
        int track_top = BODY_TOP;
        int track_h   = VISIBLE_LINES * LINE_H;        // 40px
        int thumb_h   = (track_h * VISIBLE_LINES) / total_lines;
        if(thumb_h < 4) thumb_h = 4;
        int thumb_y   = track_top + (track_h - thumb_h) * ev->scroll_y
                        / (total_lines - VISIBLE_LINES);

        // Track
        canvas_draw_line(c, 125, track_top, 125, track_top + track_h);
        // Thumb
        canvas_draw_box(c, 124, thumb_y, 2, thumb_h);
        // Down arrow if more below
        if(ev->scroll_y + VISIBLE_LINES < total_lines)
            canvas_draw_str(c, 121, track_top + track_h + 1, "v");
    }

    // ── Divider before choices ───────────────────────────────
    canvas_draw_line(c, 1, 52, 126, 52);

    // ── Choice: arrow + label left, page indicator right ─────
    canvas_set_font(c, FontSecondary);
    const EventChoice* ch = &def->choices[ev->choice_cursor];
    char choice_buf[32];
    snprintf(choice_buf, sizeof(choice_buf), "\x15 %s", ch->label);
    canvas_draw_str_aligned(c, 3,   CHOICE_TOP, AlignLeft,  AlignTop, choice_buf);

    char page_buf[16];
    snprintf(page_buf, sizeof(page_buf), "%d/%d",
             ev->choice_cursor + 1, (int)ev->def->num_choices);
    canvas_draw_str_aligned(c, 125, CHOICE_TOP, AlignRight, AlignTop, page_buf);
}
