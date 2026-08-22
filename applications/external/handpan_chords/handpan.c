#include "handpan.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */

#define HP_SCREEN_W 128
#define HP_SCREEN_H 64

#define HP_LIST_ROWS   4
#define HP_LIST_ROW_H  13
#define HP_LIST_TOP    12
#define HP_SCROLLBAR_X 125

#define HP_PAN_CX    33
#define HP_PAN_CY    38
#define HP_DING_R    6
#define HP_TONE_R    5
#define HP_DIVIDER_X 61

/* Tone fields alternate side to side and climb toward the top of the shell --
 * the standard handpan layout, lowest pair nearest the player. */
static const int8_t hp_pad_dx[HP_MAX_TONE_FIELDS] = {-12, 12, -22, 22, -21, 21, -11, 11, 0};
static const int8_t hp_pad_dy[HP_MAX_TONE_FIELDS] = {20, 20, 8, 8, -8, -8, -19, -19, -23};

/* Practice mode. Three fixed speeds rather than a BPM dial -- when you're
 * learning a phrase you want "slower", not a number. Fast is the tempo the
 * play-along has always run at; the two below it are for getting the shape
 * under your hands first. */
#define HP_MAX_PATTERNS 24

static const uint16_t hp_speed_bpm[] = {40, 60, 80};
static const char* const hp_speed_names[] = {"Slow", "Med", "Fast"};
#define HP_SPEED_COUNT   COUNT_OF(hp_speed_bpm)
#define HP_SPEED_DEFAULT (HP_SPEED_COUNT - 1)

/* Splash. It never advances on its own -- it holds until a key is pressed, so
 * the prompt means what it says. Unhurried on purpose: one pad every 220ms
 * reads as a slow arpeggio rather than a flicker. */
#define HP_SPLASH_MS    220 /* per animation frame */
#define HP_SPLASH_SLIDE 4 /* frames for the title to slide in */
#define HP_SPLASH_BLINK 6 /* frames per on/off phase of the prompt */
#define HP_SPLASH_CX    33
#define HP_SPLASH_CY    33
#define HP_SPLASH_RIM   29
#define HP_SPLASH_PADS  8 /* every shipped drum has eight tone fields */

/* Chord depth filter: how many tones a formula may have to be shown. */
static const uint8_t hp_depths[] = {3, 4, 6};
static const char* const hp_depth_names[] = {"Triads", "+7ths", "All"};
#define HP_DEPTH_COUNT COUNT_OF(hp_depths)
#define HP_DEPTH_ALL   (HP_DEPTH_COUNT - 1)

/* ------------------------------------------------------------------ */
/* App state                                                           */
/* ------------------------------------------------------------------ */

typedef enum {
    HpScreenSplash,
    HpScreenMenu,
    HpScreenScales,
    HpScreenChords,
    HpScreenFavorites,
    HpScreenPractice,
    HpScreenPlay,
    HpScreenAbout,
} HpScreen;

typedef enum {
    HpMenuScales,
    HpMenuFavorites,
    HpMenuPractice,
    HpMenuDepth,
    HpMenuAbout,
    HpMenuCount,
} HpMenuItem;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    bool running;

    HpScreen screen;
    HpScreen chord_return; /* where Back from the chord view goes */
    uint32_t frame; /* splash animation tick, one per idle timeout */

    size_t menu_sel;
    size_t menu_top;
    size_t depth_idx;

    size_t scale_sel;
    size_t scale_top;

    size_t scale_idx; /* drum shown in the chord view */
    HpChord chords[HP_MAX_CHORDS];
    size_t chord_count;
    size_t chord_sel;

    HpFavorite favs[HP_MAX_FAVORITES];
    size_t fav_count;
    size_t fav_sel;
    size_t fav_top;

    /* practice: patterns the current drum can actually play */
    uint8_t pat_list[HP_MAX_PATTERNS];
    size_t pat_count;
    size_t pat_sel;
    size_t pat_top;

    size_t play_pat; /* index into hp_patterns[] */
    uint16_t play_masks[HP_MAX_STEPS];
    size_t play_len;
    size_t play_step;
    bool playing;
    uint32_t next_step_tick;
    size_t speed_idx;
    uint32_t splash_start;
} HpApp;

/* Fills a row label for the shared list widget. */
typedef void (*HpLabelFn)(HpApp* app, size_t index, char* out, size_t out_size);

static void hp_list_scroll(size_t count, size_t sel, size_t* top);

/* ------------------------------------------------------------------ */
/* Chord / favorite helpers                                            */
/* ------------------------------------------------------------------ */

static void hp_rebuild_chords(HpApp* app) {
    app->chord_count = hp_build_chords(
        &hp_scales[app->scale_idx], hp_depths[app->depth_idx], app->chords, HP_MAX_CHORDS);
    if(app->chord_count == 0) {
        app->chord_sel = 0;
    } else if(app->chord_sel >= app->chord_count) {
        app->chord_sel = app->chord_count - 1;
    }
}

/* Collect the patterns this drum has every note for. */
static void hp_rebuild_patterns(HpApp* app) {
    const HpScale* s = &hp_scales[app->scale_idx];

    app->pat_count = 0;
    for(size_t i = 0; i < hp_pattern_count && app->pat_count < HP_MAX_PATTERNS; i++) {
        if(hp_pattern_available(s, &hp_patterns[i])) {
            app->pat_list[app->pat_count++] = (uint8_t)i;
        }
    }

    if(app->pat_count == 0) {
        app->pat_sel = 0;
    } else if(app->pat_sel >= app->pat_count) {
        app->pat_sel = app->pat_count - 1;
    }
    hp_list_scroll(app->pat_count, app->pat_sel, &app->pat_top);
}

/* Left/Right changes drum from the chord and practice screens alike. */
static void hp_select_scale(HpApp* app, size_t idx) {
    app->scale_idx = idx;
    app->scale_sel = idx;
    hp_list_scroll(hp_scale_count, app->scale_sel, &app->scale_top);
    app->chord_sel = 0;
    hp_rebuild_chords(app);
    hp_rebuild_patterns(app);
}

static int hp_fav_find(HpApp* app, uint8_t scale, uint8_t root_pc, uint8_t formula) {
    for(size_t i = 0; i < app->fav_count; i++) {
        if(app->favs[i].scale == scale && app->favs[i].root_pc == root_pc &&
           app->favs[i].formula == formula) {
            return (int)i;
        }
    }
    return -1;
}

static bool hp_current_is_fav(HpApp* app) {
    if(app->chord_count == 0) return false;
    const HpChord* c = &app->chords[app->chord_sel];
    return hp_fav_find(app, (uint8_t)app->scale_idx, c->root_pc, c->formula) >= 0;
}

static void hp_fav_toggle(HpApp* app) {
    if(app->chord_count == 0) return;
    const HpChord* c = &app->chords[app->chord_sel];
    int idx = hp_fav_find(app, (uint8_t)app->scale_idx, c->root_pc, c->formula);

    if(idx >= 0) {
        for(size_t i = (size_t)idx; i + 1 < app->fav_count; i++) {
            app->favs[i] = app->favs[i + 1];
        }
        app->fav_count--;
        if(app->fav_sel >= app->fav_count && app->fav_count > 0) {
            app->fav_sel = app->fav_count - 1;
        } else if(app->fav_count == 0) {
            app->fav_sel = 0;
        }
    } else if(app->fav_count < HP_MAX_FAVORITES) {
        app->favs[app->fav_count].scale = (uint8_t)app->scale_idx;
        app->favs[app->fav_count].root_pc = c->root_pc;
        app->favs[app->fav_count].formula = c->formula;
        app->fav_count++;
    } else {
        return; /* list full, nothing changed */
    }

    hp_fav_save(app->storage, app->favs, app->fav_count);
}

/* Chord name for a favorite, which may belong to a drum we aren't showing. */
static void hp_fav_chord_name(const HpFavorite* fav, char* out, size_t out_size) {
    const HpScale* s = &hp_scales[fav->scale];
    snprintf(
        out,
        out_size,
        "%s%s",
        hp_pc_name(fav->root_pc, s->flats),
        hp_formulas[fav->formula].suffix);
}

/* Open the chord view on a specific chord, widening the depth filter if the
 * saved chord has more tones than the current filter allows. */
static void hp_open_favorite(HpApp* app, size_t index) {
    if(index >= app->fav_count) return;
    HpFavorite fav = app->favs[index];

    app->scale_idx = fav.scale;
    if(hp_formulas[fav.formula].count > hp_depths[app->depth_idx]) {
        app->depth_idx = HP_DEPTH_ALL;
    }
    app->chord_sel = 0;
    hp_rebuild_chords(app);

    for(size_t i = 0; i < app->chord_count; i++) {
        if(app->chords[i].root_pc == fav.root_pc && app->chords[i].formula == fav.formula) {
            app->chord_sel = i;
            break;
        }
    }

    app->chord_return = HpScreenFavorites;
    app->screen = HpScreenChords;
}

/* ------------------------------------------------------------------ */
/* Shared list widget                                                  */
/* ------------------------------------------------------------------ */

/* Keeps the cursor one row in from the top or bottom edge while scrolling. */
static void hp_list_scroll(size_t count, size_t sel, size_t* top) {
    if(count <= HP_LIST_ROWS) {
        *top = 0;
        return;
    }
    if(sel < *top + 1) {
        *top = (sel > 0) ? sel - 1 : 0;
    } else if(sel + 2 > *top + HP_LIST_ROWS) {
        *top = sel + 2 - HP_LIST_ROWS;
    }
    if(*top + HP_LIST_ROWS > count) {
        *top = count - HP_LIST_ROWS;
    }
}

static void hp_list_move(size_t count, size_t* sel, size_t* top, int delta) {
    if(count == 0) return;
    if(delta > 0) {
        *sel = (*sel + 1 >= count) ? 0 : *sel + 1;
    } else {
        *sel = (*sel == 0) ? count - 1 : *sel - 1;
    }
    hp_list_scroll(count, *sel, top);
}

static void
    hp_draw_list(Canvas* canvas, HpApp* app, size_t count, size_t sel, size_t top, HpLabelFn label) {
    canvas_set_font(canvas, FontSecondary);

    if(count == 0) {
        const char* msg = "(nothing here yet)";
        uint16_t w = canvas_string_width(canvas, msg);
        canvas_draw_str(canvas, (HP_SCREEN_W - (int32_t)w) / 2, 40, msg);
        return;
    }

    bool scrollbar = count > HP_LIST_ROWS;
    size_t row_w = scrollbar ? 123 : HP_SCREEN_W;

    for(size_t row = 0; row < HP_LIST_ROWS; row++) {
        size_t index = top + row;
        if(index >= count) break;

        int32_t y = HP_LIST_TOP + (int32_t)(row * HP_LIST_ROW_H);
        char text[40];
        text[0] = '\0';
        label(app, index, text, sizeof(text));

        if(index == sel) {
            canvas_draw_box(canvas, 0, y, row_w, HP_LIST_ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y + 10, text);
        if(index == sel) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    if(scrollbar) {
        int32_t track_y = HP_LIST_TOP;
        int32_t track_h = HP_SCREEN_H - HP_LIST_TOP;
        canvas_draw_line(canvas, HP_SCROLLBAR_X, track_y, HP_SCROLLBAR_X, track_y + track_h - 1);

        int32_t knob_h = (int32_t)((size_t)track_h * HP_LIST_ROWS / count);
        if(knob_h < 4) knob_h = 4;
        size_t span = count - HP_LIST_ROWS;
        int32_t knob_y = track_y + (int32_t)((size_t)(track_h - knob_h) * top / span);
        canvas_draw_box(canvas, HP_SCROLLBAR_X - 1, knob_y, 3, (size_t)knob_h);
    }
}

/* ------------------------------------------------------------------ */
/* Screen: splash                                                      */
/* ------------------------------------------------------------------ */

/* The shell fills in one pad at a time, low to high, like a run up the
 * scale; once the run finishes a wave keeps rippling out from the ding.
 * Any key skips straight to the menu. */
static void hp_draw_splash(Canvas* canvas, HpApp* app) {
    uint32_t f = app->frame;

    canvas_draw_circle(canvas, HP_SPLASH_CX, HP_SPLASH_CY, HP_SPLASH_RIM);

    if(f > HP_SPLASH_PADS) {
        int32_t r = 8 + (int32_t)(((f - HP_SPLASH_PADS - 1) * 2) % 24);
        canvas_draw_circle(canvas, HP_SPLASH_CX, HP_SPLASH_CY, (size_t)r);
    }

    canvas_draw_disc(canvas, HP_SPLASH_CX, HP_SPLASH_CY, HP_DING_R);

    for(uint32_t i = 0; i < HP_SPLASH_PADS; i++) {
        int32_t x = HP_SPLASH_CX + hp_pad_dx[i];
        int32_t y = HP_SPLASH_CY + hp_pad_dy[i];
        if(f > i) {
            canvas_draw_disc(canvas, x, y, HP_TONE_R);
        } else {
            canvas_draw_circle(canvas, x, y, HP_TONE_R);
        }
    }

    /* Title slides in from the right edge. */
    int32_t tx = 68;
    if(f < HP_SPLASH_SLIDE) tx += (int32_t)((HP_SPLASH_SLIDE - f) * 18);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, tx, 26, "HANDPAN");
    canvas_draw_str(canvas, tx, 39, "CHORDS");

    if(f >= HP_SPLASH_SLIDE) {
        canvas_draw_line(canvas, 68, 45, 124, 45);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 68, 54, "by KingBoa");
        /* Phase from when the prompt appears, so its first showing is a full
         * beat rather than a clipped one. */
        if(((f - HP_SPLASH_SLIDE) / HP_SPLASH_BLINK) % 2 == 0) {
            canvas_draw_str(canvas, 68, 63, "OK to start");
        }
    }
}

/* ------------------------------------------------------------------ */
/* Screen: main menu                                                   */
/* ------------------------------------------------------------------ */

static void hp_menu_label(HpApp* app, size_t index, char* out, size_t out_size) {
    switch(index) {
    case HpMenuScales:
        snprintf(out, out_size, "Scales (%u)", (unsigned)hp_scale_count);
        break;
    case HpMenuFavorites:
        snprintf(out, out_size, "Favorites (%u)", (unsigned)app->fav_count);
        break;
    case HpMenuPractice:
        snprintf(out, out_size, "Practice (%u)", (unsigned)app->pat_count);
        break;
    case HpMenuDepth:
        snprintf(out, out_size, "Chord depth: %s", hp_depth_names[app->depth_idx]);
        break;
    default:
        snprintf(out, out_size, "About");
        break;
    }
}

static void hp_draw_header(Canvas* canvas, const char* title) {
    canvas_set_font(canvas, FontSecondary);
    uint16_t w = canvas_string_width(canvas, title);
    canvas_draw_str(canvas, (HP_SCREEN_W - (int32_t)w) / 2, 9, title);
    canvas_draw_line(canvas, 0, 11, HP_SCREEN_W - 1, 11);
}

static void hp_draw_menu(Canvas* canvas, HpApp* app) {
    hp_draw_header(canvas, "Handpan Chords");
    hp_draw_list(canvas, app, HpMenuCount, app->menu_sel, app->menu_top, hp_menu_label);
}

/* ------------------------------------------------------------------ */
/* Screen: scale list                                                  */
/* ------------------------------------------------------------------ */

static void hp_scale_label(HpApp* app, size_t index, char* out, size_t out_size) {
    UNUSED(app);
    snprintf(out, out_size, "%s", hp_scales[index].name);
}

static void hp_draw_scales(Canvas* canvas, HpApp* app) {
    hp_draw_header(canvas, "Scales");
    hp_draw_list(canvas, app, hp_scale_count, app->scale_sel, app->scale_top, hp_scale_label);
}

/* ------------------------------------------------------------------ */
/* Screen: favorites                                                   */
/* ------------------------------------------------------------------ */

static void hp_fav_label(HpApp* app, size_t index, char* out, size_t out_size) {
    const HpFavorite* fav = &app->favs[index];
    char chord[HP_NAME_LEN];
    hp_fav_chord_name(fav, chord, sizeof(chord));
    snprintf(out, out_size, "%s - %s", chord, hp_scales[fav->scale].name);
}

static void hp_draw_favorites(Canvas* canvas, HpApp* app) {
    /* The delete gesture is undiscoverable, so advertise it in the header. */
    hp_draw_header(canvas, app->fav_count > 0 ? "Favorites - Left deletes" : "Favorites");
    hp_draw_list(canvas, app, app->fav_count, app->fav_sel, app->fav_top, hp_fav_label);
}

/* ------------------------------------------------------------------ */
/* Screen: chord view                                                  */
/* ------------------------------------------------------------------ */

/* Note names of every lit pad, ding first then pads ascending, greedily
 * wrapped into three lines of at most 11 characters. */
static void hp_chord_note_lines(const HpScale* s, const HpChord* c, char lines[3][16]) {
    for(size_t i = 0; i < 3; i++) {
        lines[i][0] = '\0';
    }

    uint8_t pad_count = hp_scale_pad_count(s);
    size_t line = 0;
    size_t len = 0;

    for(uint8_t pad = 0; pad < pad_count && line < 3; pad++) {
        if(!(c->pad_mask & (uint16_t)(1u << pad))) continue;

        char note[8];
        hp_note_name(hp_scale_pad_midi(s, pad), s->flats, note, sizeof(note));

        size_t note_len = strlen(note);
        size_t needed = (len == 0) ? note_len : note_len + 1;
        if(len + needed > 11) {
            line++;
            if(line >= 3) break;
            len = 0;
        }

        int written = snprintf(
            lines[line] + len, sizeof(lines[line]) - len, "%s%s", (len > 0) ? " " : "", note);
        if(written < 0) break;
        len += (size_t)written;
        if(len >= sizeof(lines[line])) {
            len = sizeof(lines[line]) - 1;
        }
    }
}

/* Eight-point asterisk, four strokes through a common centre. */
static void hp_draw_star(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_line(canvas, x - 3, y, x + 3, y);
    canvas_draw_line(canvas, x, y - 3, x, y + 3);
    canvas_draw_line(canvas, x - 2, y - 2, x + 2, y + 2);
    canvas_draw_line(canvas, x - 2, y + 2, x + 2, y - 2);
}

static void hp_draw_pan(Canvas* canvas, const HpScale* s, const HpChord* c) {
    if(c->pad_mask & 1u) {
        canvas_draw_disc(canvas, HP_PAN_CX, HP_PAN_CY, HP_DING_R);
    } else {
        canvas_draw_circle(canvas, HP_PAN_CX, HP_PAN_CY, HP_DING_R);
    }

    uint8_t tone_count = (uint8_t)(hp_scale_pad_count(s) - 1);
    for(uint8_t i = 0; i < tone_count && i < HP_MAX_TONE_FIELDS; i++) {
        int32_t x = HP_PAN_CX + hp_pad_dx[i];
        int32_t y = HP_PAN_CY + hp_pad_dy[i];
        if(c->pad_mask & (uint16_t)(1u << (i + 1))) {
            canvas_draw_disc(canvas, x, y, HP_TONE_R);
        } else {
            canvas_draw_circle(canvas, x, y, HP_TONE_R);
        }
    }
}

static void hp_draw_chords(Canvas* canvas, HpApp* app) {
    const HpScale* s = &hp_scales[app->scale_idx];

    hp_draw_header(canvas, s->name);
    canvas_draw_str(canvas, 1, 9, "<");
    canvas_draw_str(canvas, 122, 9, ">");

    if(app->chord_count == 0) {
        const char* msg = "No chords at this depth";
        uint16_t w = canvas_string_width(canvas, msg);
        canvas_draw_str(canvas, (HP_SCREEN_W - (int32_t)w) / 2, 40, msg);
        return;
    }

    const HpChord* c = &app->chords[app->chord_sel];

    hp_draw_pan(canvas, s, c);
    canvas_draw_line(canvas, HP_DIVIDER_X, 12, HP_DIVIDER_X, HP_SCREEN_H - 1);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 65, 24, c->name);

    canvas_set_font(canvas, FontSecondary);
    char lines[3][16];
    hp_chord_note_lines(s, c, lines);
    const int32_t line_y[3] = {35, 44, 53};
    for(size_t i = 0; i < 3; i++) {
        if(lines[i][0] != '\0') {
            canvas_draw_str(canvas, 65, line_y[i], lines[i]);
        }
    }

    char index[16];
    snprintf(
        index, sizeof(index), "%u/%u", (unsigned)(app->chord_sel + 1), (unsigned)app->chord_count);
    canvas_draw_str(canvas, 65, 62, index);

    if(hp_current_is_fav(app)) {
        hp_draw_star(canvas, 120, 58);
    }
}

/* ------------------------------------------------------------------ */
/* Screen: practice pattern list                                       */
/* ------------------------------------------------------------------ */

static void hp_pattern_label(HpApp* app, size_t index, char* out, size_t out_size) {
    const HpPattern* p = &hp_patterns[app->pat_list[index]];
    snprintf(out, out_size, "%s (%u)", p->name, (unsigned)p->step_count);
}

static void hp_draw_practice(Canvas* canvas, HpApp* app) {
    hp_draw_header(canvas, hp_scales[app->scale_idx].name);
    canvas_draw_str(canvas, 1, 9, "<");
    canvas_draw_str(canvas, 122, 9, ">");
    hp_draw_list(canvas, app, app->pat_count, app->pat_sel, app->pat_top, hp_pattern_label);
}

/* ------------------------------------------------------------------ */
/* Screen: play-along                                                  */
/* ------------------------------------------------------------------ */

/* Note names for one strike -- "D4" alone, or "D3+D4" when two pads are struck
 * together. Ding first, then tone fields ascending. */
static void hp_step_note_text(const HpScale* s, uint16_t mask, char* out, size_t out_size) {
    if(!out || out_size == 0) return;
    out[0] = '\0';

    size_t len = 0;
    uint8_t pad_count = hp_scale_pad_count(s);

    for(uint8_t pad = 0; pad < pad_count; pad++) {
        if(!(mask & (uint16_t)(1u << pad))) continue;

        char note[8];
        hp_note_name(hp_scale_pad_midi(s, pad), s->flats, note, sizeof(note));

        int written = snprintf(out + len, out_size - len, "%s%s", (len > 0) ? "+" : "", note);
        if(written < 0) break;
        len += (size_t)written;
        if(len >= out_size) {
            len = out_size - 1;
            break;
        }
    }
}

/* The pads for one strike are lit, the next strike's pads marked with dots so
 * you can see where the phrase is heading -- follow it round the shell. */
static void hp_draw_play(Canvas* canvas, HpApp* app) {
    const HpScale* s = &hp_scales[app->scale_idx];
    const HpPattern* p = &hp_patterns[app->play_pat];

    hp_draw_header(canvas, p->name);

    if(app->play_len == 0) {
        const char* msg = "Not playable here";
        uint16_t w = canvas_string_width(canvas, msg);
        canvas_draw_str(canvas, (HP_SCREEN_W - (int32_t)w) / 2, 40, msg);
        return;
    }

    uint16_t cur = app->play_masks[app->play_step];
    uint16_t next = app->play_masks[(app->play_step + 1) % app->play_len];

    if(cur & 1u) {
        canvas_draw_disc(canvas, HP_PAN_CX, HP_PAN_CY, HP_DING_R);
    } else {
        canvas_draw_circle(canvas, HP_PAN_CX, HP_PAN_CY, HP_DING_R);
        if(next & 1u) canvas_draw_dot(canvas, HP_PAN_CX, HP_PAN_CY);
    }

    uint8_t tone_count = (uint8_t)(hp_scale_pad_count(s) - 1);
    for(uint8_t i = 0; i < tone_count && i < HP_MAX_TONE_FIELDS; i++) {
        int32_t x = HP_PAN_CX + hp_pad_dx[i];
        int32_t y = HP_PAN_CY + hp_pad_dy[i];
        uint16_t bit = (uint16_t)(1u << (i + 1));

        if(cur & bit) {
            canvas_draw_disc(canvas, x, y, HP_TONE_R);
        } else {
            canvas_draw_circle(canvas, x, y, HP_TONE_R);
            if(next & bit) canvas_draw_dot(canvas, x, y);
        }
    }

    canvas_draw_line(canvas, HP_DIVIDER_X, 12, HP_DIVIDER_X, HP_SCREEN_H - 1);

    char note[16];
    hp_step_note_text(s, cur, note, sizeof(note));
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 65, 25, note);

    canvas_set_font(canvas, FontSecondary);
    char buf[24];
    snprintf(
        buf, sizeof(buf), "Step %u/%u", (unsigned)(app->play_step + 1), (unsigned)app->play_len);
    canvas_draw_str(canvas, 65, 36, buf);

    snprintf(buf, sizeof(buf), "%s %s", app->playing ? ">" : "||", hp_speed_names[app->speed_idx]);
    canvas_draw_str(canvas, 65, 46, buf);

    /* Progress strip: played, current, still to come. */
    int32_t spacing = (app->play_len > 12) ? 3 : 4;
    for(size_t i = 0; i < app->play_len; i++) {
        int32_t x = 65 + (int32_t)i * spacing;
        if(i == app->play_step) {
            canvas_draw_box(canvas, x, 52, 2, 7);
        } else if(i < app->play_step) {
            canvas_draw_box(canvas, x, 55, 2, 3);
        } else {
            canvas_draw_dot(canvas, x, 57);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Screen: about                                                       */
/* ------------------------------------------------------------------ */

static void hp_draw_about(Canvas* canvas, HpApp* app) {
    UNUSED(app);
    hp_draw_header(canvas, "About");

    static const char* const lines[] = {
        "Up/Dn: chords  L/R: drums",
        "OK: toggle favorite",
        "Practice: OK plays along",
        "Filled pad = strike it",
    };

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < COUNT_OF(lines); i++) {
        canvas_draw_str(canvas, 2, 20 + (int32_t)(i * 8), lines[i]);
    }

    canvas_draw_line(canvas, 0, 47, HP_SCREEN_W - 1, 47);

    static const char* const credit[] = {"by KingBoa", "NoodleNugget.com"};
    for(size_t i = 0; i < COUNT_OF(credit); i++) {
        uint16_t w = canvas_string_width(canvas, credit[i]);
        canvas_draw_str(canvas, (HP_SCREEN_W - (int32_t)w) / 2, 55 + (int32_t)(i * 8), credit[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Draw / input dispatch                                               */
/* ------------------------------------------------------------------ */

static void hp_draw_callback(Canvas* canvas, void* ctx) {
    HpApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(app->screen) {
    case HpScreenSplash:
        hp_draw_splash(canvas, app);
        break;
    case HpScreenMenu:
        hp_draw_menu(canvas, app);
        break;
    case HpScreenScales:
        hp_draw_scales(canvas, app);
        break;
    case HpScreenChords:
        hp_draw_chords(canvas, app);
        break;
    case HpScreenFavorites:
        hp_draw_favorites(canvas, app);
        break;
    case HpScreenPractice:
        hp_draw_practice(canvas, app);
        break;
    case HpScreenPlay:
        hp_draw_play(canvas, app);
        break;
    case HpScreenAbout:
        hp_draw_about(canvas, app);
        break;
    }

    furi_mutex_release(app->mutex);
}

static void hp_input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

static void hp_input_menu(HpApp* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        hp_list_move(HpMenuCount, &app->menu_sel, &app->menu_top, -1);
        break;
    case InputKeyDown:
        hp_list_move(HpMenuCount, &app->menu_sel, &app->menu_top, 1);
        break;
    case InputKeyLeft:
        if(app->menu_sel == HpMenuDepth) {
            app->depth_idx = (app->depth_idx == 0) ? HP_DEPTH_COUNT - 1 : app->depth_idx - 1;
            hp_rebuild_chords(app);
        }
        break;
    case InputKeyRight:
        if(app->menu_sel == HpMenuDepth) {
            app->depth_idx = (app->depth_idx + 1) % HP_DEPTH_COUNT;
            hp_rebuild_chords(app);
        }
        break;
    case InputKeyOk:
        switch(app->menu_sel) {
        case HpMenuScales:
            app->screen = HpScreenScales;
            break;
        case HpMenuFavorites:
            hp_list_scroll(app->fav_count, app->fav_sel, &app->fav_top);
            app->screen = HpScreenFavorites;
            break;
        case HpMenuPractice:
            hp_rebuild_patterns(app);
            app->screen = HpScreenPractice;
            break;
        case HpMenuDepth:
            app->depth_idx = (app->depth_idx + 1) % HP_DEPTH_COUNT;
            hp_rebuild_chords(app);
            break;
        default:
            app->screen = HpScreenAbout;
            break;
        }
        break;
    case InputKeyBack:
        app->running = false;
        break;
    default:
        break;
    }
}

static void hp_input_scales(HpApp* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        hp_list_move(hp_scale_count, &app->scale_sel, &app->scale_top, -1);
        break;
    case InputKeyDown:
        hp_list_move(hp_scale_count, &app->scale_sel, &app->scale_top, 1);
        break;
    case InputKeyOk:
        hp_select_scale(app, app->scale_sel);
        app->chord_return = HpScreenScales;
        app->screen = HpScreenChords;
        break;
    case InputKeyBack:
        app->screen = HpScreenMenu;
        break;
    default:
        break;
    }
}

static void hp_input_chords(HpApp* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        if(app->chord_count > 0) {
            app->chord_sel = (app->chord_sel == 0) ? app->chord_count - 1 : app->chord_sel - 1;
        }
        break;
    case InputKeyDown:
        if(app->chord_count > 0) {
            app->chord_sel = (app->chord_sel + 1 >= app->chord_count) ? 0 : app->chord_sel + 1;
        }
        break;
    case InputKeyLeft:
        hp_select_scale(app, (app->scale_idx == 0) ? hp_scale_count - 1 : app->scale_idx - 1);
        break;
    case InputKeyRight:
        hp_select_scale(app, (app->scale_idx + 1 >= hp_scale_count) ? 0 : app->scale_idx + 1);
        break;
    case InputKeyOk:
        hp_fav_toggle(app);
        break;
    case InputKeyBack:
        if(app->chord_return == HpScreenFavorites) {
            hp_list_scroll(app->fav_count, app->fav_sel, &app->fav_top);
        }
        app->screen = app->chord_return;
        break;
    default:
        break;
    }
}

static void hp_input_favorites(HpApp* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        hp_list_move(app->fav_count, &app->fav_sel, &app->fav_top, -1);
        break;
    case InputKeyDown:
        hp_list_move(app->fav_count, &app->fav_sel, &app->fav_top, 1);
        break;
    case InputKeyLeft:
        if(app->fav_count > 0) {
            for(size_t i = app->fav_sel; i + 1 < app->fav_count; i++) {
                app->favs[i] = app->favs[i + 1];
            }
            app->fav_count--;
            if(app->fav_count == 0) {
                app->fav_sel = 0;
            } else if(app->fav_sel >= app->fav_count) {
                app->fav_sel = app->fav_count - 1;
            }
            hp_list_scroll(app->fav_count, app->fav_sel, &app->fav_top);
            hp_fav_save(app->storage, app->favs, app->fav_count);
        }
        break;
    case InputKeyOk:
        hp_open_favorite(app, app->fav_sel);
        break;
    case InputKeyBack:
        app->screen = HpScreenMenu;
        break;
    default:
        break;
    }
}

static void hp_input_practice(HpApp* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        hp_list_move(app->pat_count, &app->pat_sel, &app->pat_top, -1);
        break;
    case InputKeyDown:
        hp_list_move(app->pat_count, &app->pat_sel, &app->pat_top, 1);
        break;
    case InputKeyLeft:
        hp_select_scale(app, (app->scale_idx == 0) ? hp_scale_count - 1 : app->scale_idx - 1);
        break;
    case InputKeyRight:
        hp_select_scale(app, (app->scale_idx + 1 >= hp_scale_count) ? 0 : app->scale_idx + 1);
        break;
    case InputKeyOk:
        if(app->pat_count > 0) {
            app->play_pat = app->pat_list[app->pat_sel];
            app->play_len = hp_pattern_steps(
                &hp_scales[app->scale_idx],
                &hp_patterns[app->play_pat],
                app->play_masks,
                HP_MAX_STEPS);
            app->play_step = 0;
            app->playing = false;
            app->screen = HpScreenPlay;
        }
        break;
    case InputKeyBack:
        app->screen = HpScreenMenu;
        break;
    default:
        break;
    }
}

static uint32_t hp_step_ticks(HpApp* app) {
    return furi_ms_to_ticks(60000u / hp_speed_bpm[app->speed_idx]);
}

static void hp_input_play(HpApp* app, InputKey key) {
    switch(key) {
    /* Stepping by hand takes over from playback: stop where you are so you can
     * walk the phrase at your own pace. OK picks it up again from here. */
    case InputKeyRight:
        app->playing = false;
        if(app->play_len > 0) app->play_step = (app->play_step + 1) % app->play_len;
        break;
    case InputKeyLeft:
        app->playing = false;
        if(app->play_len > 0) {
            app->play_step = (app->play_step == 0) ? app->play_len - 1 : app->play_step - 1;
        }
        break;
    case InputKeyUp:
        if(app->speed_idx + 1 < HP_SPEED_COUNT) {
            app->speed_idx++;
            /* Re-time the pending step so the change is heard immediately. */
            if(app->playing) app->next_step_tick = furi_get_tick() + hp_step_ticks(app);
        }
        break;
    case InputKeyDown:
        if(app->speed_idx > 0) {
            app->speed_idx--;
            if(app->playing) app->next_step_tick = furi_get_tick() + hp_step_ticks(app);
        }
        break;
    case InputKeyOk:
        app->playing = !app->playing;
        if(app->playing) app->next_step_tick = furi_get_tick() + hp_step_ticks(app);
        break;
    case InputKeyBack:
        app->playing = false;
        app->screen = HpScreenPractice;
        break;
    default:
        break;
    }
}

static void hp_handle_input(HpApp* app, InputKey key) {
    switch(app->screen) {
    case HpScreenSplash:
        UNUSED(key); /* any key skips the intro */
        app->screen = HpScreenMenu;
        break;
    case HpScreenMenu:
        hp_input_menu(app, key);
        break;
    case HpScreenScales:
        hp_input_scales(app, key);
        break;
    case HpScreenChords:
        hp_input_chords(app, key);
        break;
    case HpScreenFavorites:
        hp_input_favorites(app, key);
        break;
    case HpScreenPractice:
        hp_input_practice(app, key);
        break;
    case HpScreenPlay:
        hp_input_play(app, key);
        break;
    case HpScreenAbout:
        if(key == InputKeyBack || key == InputKeyOk) {
            app->screen = HpScreenMenu;
        }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Timed updates                                                       */
/* ------------------------------------------------------------------ */

/* Both clocks work off elapsed ticks rather than counting wakeups, so the
 * splash runs at a fixed speed and the tempo stays honest regardless of how
 * often the loop happens to wake. Returns true when the screen changed. */
static bool hp_tick(HpApp* app) {
    uint32_t now = furi_get_tick();

    if(app->screen == HpScreenSplash) {
        /* No timeout: the splash holds until a key is pressed. It keeps
         * breathing while it waits, so the frame counter just runs on. */
        uint32_t frame = (now - app->splash_start) / furi_ms_to_ticks(HP_SPLASH_MS);
        if(frame == app->frame) return false;
        app->frame = frame;
        return true;
    }

    if(app->screen == HpScreenPlay && app->playing && app->play_len > 0) {
        /* Signed compare so the tick counter wrapping doesn't stall playback. */
        if((int32_t)(now - app->next_step_tick) >= 0) {
            app->play_step = (app->play_step + 1) % app->play_len;
            app->next_step_tick = now + hp_step_ticks(app);
            return true;
        }
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

static HpApp* hp_app_alloc(void) {
    HpApp* app = malloc(sizeof(HpApp));
    memset(app, 0, sizeof(HpApp));

    app->running = true;
    app->screen = HpScreenSplash;
    app->chord_return = HpScreenScales;
    app->depth_idx = HP_DEPTH_ALL;

    app->speed_idx = HP_SPEED_DEFAULT;
    app->splash_start = furi_get_tick();

    app->storage = furi_record_open(RECORD_STORAGE);
    app->fav_count = hp_fav_load(app->storage, app->favs, HP_MAX_FAVORITES);

    hp_rebuild_chords(app);
    hp_rebuild_patterns(app);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, hp_draw_callback, app);
    view_port_input_callback_set(app->view_port, hp_input_callback, app->queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void hp_app_free(HpApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int32_t handpan_app(void* p) {
    UNUSED(p);

    HpApp* app = hp_app_alloc();
    InputEvent event;

    while(app->running) {
        /* Only the splash and the play-along metronome need a clock; every
         * other screen is static, so idle wakeups stay cheap. The splash can
         * sit waiting for a key indefinitely, so it ticks lazily -- the
         * metronome is the one that wants precision. */
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool splashing = app->screen == HpScreenSplash;
        bool metronome = app->screen == HpScreenPlay && app->playing;
        furi_mutex_release(app->mutex);

        bool animating = splashing || metronome;
        uint32_t timeout = metronome ? 25 : (splashing ? 50 : 200);

        if(furi_message_queue_get(app->queue, &event, timeout) == FuriStatusOk) {
            bool actionable = event.type == InputTypeShort ||
                              (event.type == InputTypeRepeat && event.key != InputKeyBack);
            if(!actionable) continue;

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            hp_handle_input(app, event.key);
            furi_mutex_release(app->mutex);

            view_port_update(app->view_port);
            continue;
        }

        if(!animating) continue;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool redraw = hp_tick(app);
        furi_mutex_release(app->mutex);

        if(redraw) view_port_update(app->view_port);
    }

    hp_app_free(app);
    return 0;
}
