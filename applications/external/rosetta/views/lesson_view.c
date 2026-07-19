#include "lesson_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ------------------------------------------------------------- geometry */
#define TITLE_H 13
#define DIAG_Y0 15
#define DIAG_Y1 44 // diagram band: 15..43
#define CAP_Y   46 // caption band: 46..63

struct LessonView {
    View* view;
};

typedef struct {
    RosettaProtocol protocol;
    uint8_t step;
    uint8_t nsteps;
    uint32_t anim;
} LessonModel;

/* triangle wave 0..1..0 over `period` frames, for smooth back-and-forth */
static float tri(uint32_t t, uint32_t period) {
    uint32_t p = t % period;
    float h = period / 2.0f;
    float x = (p < h) ? (p / h) : (2.0f - p / h);
    return x;
}
/* sawtooth 0..1 over `period` frames, for sweeps that snap back */
static float saw(uint32_t t, uint32_t period) {
    return (t % period) / (float)period;
}

/* ============================================================ MIFARE ==== */

static void draw_actor(Canvas* c, int x, int y, int w, int h, const char* label) {
    canvas_draw_rframe(c, x, y, w, h, 2);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, x + w / 2, y + h / 2, AlignCenter, AlignCenter, label);
}

/* A message travelling along the bus between two x positions. */
static void draw_packet(Canvas* c, int lx, int rx, int y, bool going_right, float frac, const char* label) {
    canvas_draw_line(c, lx, y, rx, y);
    int px = going_right ? (lx + (int)((rx - lx) * frac)) : (rx - (int)((rx - lx) * frac));
    canvas_draw_box(c, px - 2, y - 2, 5, 5);
    /* arrow tip */
    if(going_right) {
        canvas_draw_line(c, rx, y, rx - 3, y - 2);
        canvas_draw_line(c, rx, y, rx - 3, y + 2);
    } else {
        canvas_draw_line(c, lx, y, lx + 3, y - 2);
        canvas_draw_line(c, lx, y, lx + 3, y + 2);
    }
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, (lx + rx) / 2, y - 6, AlignCenter, AlignBottom, label);
}

static void draw_mifare(Canvas* c, LessonModel* m) {
    const int rdr_x = 2, card_x = 96, box_w = 30, box_h = 16;
    const int by = 20;
    const int lx = rdr_x + box_w + 2; // left of the wire
    const int rx = card_x - 2; // right of the wire
    const int wy = by + box_h / 2;

    draw_actor(c, rdr_x, by, box_w, box_h, "RDR");
    draw_actor(c, card_x, by, box_w, box_h, "TAG");

    switch(m->step) {
    case 0: { // RF field powers the card, ATQA comes back
        for(int k = 0; k < 3; k++) {
            int r = 3 + (int)((m->anim / 2 + k * 6) % 18);
            canvas_draw_circle(c, lx, wy, r);
        }
        draw_packet(c, lx, rx, wy, false, saw(m->anim, 50), "ATQA");
        break;
    }
    case 1: { // anticollision: UID + SAK back to reader
        draw_packet(c, lx, rx, wy, false, saw(m->anim, 50), "UID+SAK");
        break;
    }
    case 2: { // auth request out to the card
        draw_packet(c, lx, rx, wy, true, saw(m->anim, 50), "AUTH 60");
        break;
    }
    case 3: { // 3-pass Crypto1 handshake, cycling through the three messages
        uint32_t leg = (m->anim / 45) % 3;
        float f = saw(m->anim, 45);
        if(leg == 0)
            draw_packet(c, lx, rx, wy, false, f, "nT");
        else if(leg == 1)
            draw_packet(c, lx, rx, wy, true, f, "{nR,aR}");
        else
            draw_packet(c, lx, rx, wy, false, f, "aT");
        /* leg pips */
        for(int i = 0; i < 3; i++) {
            if((int)leg == i)
                canvas_draw_disc(c, 60 + i * 6, DIAG_Y1 - 2, 2);
            else
                canvas_draw_circle(c, 60 + i * 6, DIAG_Y1 - 2, 2);
        }
        break;
    }
    default: { // broken: keys flood in, the lock pops open
        /* rapid guesses reader -> card */
        for(int i = 0; i < 3; i++) {
            float f = saw(m->anim + i * 12, 24);
            int px = lx + (int)((rx - lx) * f);
            canvas_draw_line(c, px, wy - 3, px, wy + 3);
        }
        /* padlock in the middle, shackle lifts on the second half of the loop */
        int cx = (lx + rx) / 2;
        bool open = (m->anim % 60) > 30;
        canvas_draw_rbox(c, cx - 6, wy, 12, 9, 2);
        int sh = open ? 4 : 0;
        int top = wy - 4 - sh;
        canvas_draw_line(c, cx - 4, wy, cx - 4, top); // left post
        canvas_draw_line(c, cx - 4, top, cx + 4, top); // top bar
        if(open)
            canvas_draw_line(c, cx + 4, top, cx + 4, wy - 2); // right post sprung open
        else
            canvas_draw_line(c, cx + 4, top, cx + 4, wy); // right post seated
        break;
    }
    }
}

/* ======================================================== MODULATION ==== */

static const uint8_t k_bits[] = {1, 0, 1, 1, 0, 1, 0, 0};
#define NBITS (int)(sizeof(k_bits))
#define BITW  16

typedef enum { WaveCarrier, WaveOok, WavePsk } WaveMode;

static void draw_wave(Canvas* c, int mid, int amp, uint32_t scroll, WaveMode mode) {
    int prevy = mid;
    for(int x = 0; x < 128; x++) {
        int sx = x + (int)scroll;
        int bit = k_bits[(sx / BITW) % NBITS];
        float arg = sx * 0.6f; // ~carrier phase
        int a = amp;
        if(mode == WaveOok && !bit) a = 0; // gate amplitude
        if(mode == WavePsk && bit) arg += (float)M_PI; // flip phase
        int y = mid - (int)(a * sinf(arg));
        if(x == 0) prevy = y;
        canvas_draw_line(c, x - 1 < 0 ? 0 : x - 1, prevy, x, y);
        prevy = y;
    }
}

/* thin data track along the top of the band: high block for 1, low for 0 */
static void draw_data_track(Canvas* c, int y_hi, int y_lo, uint32_t scroll) {
    int prevy = y_lo;
    for(int x = 0; x < 128; x++) {
        int sx = x + (int)scroll;
        int bit = k_bits[(sx / BITW) % NBITS];
        int y = bit ? y_hi : y_lo;
        canvas_draw_line(c, x - 1 < 0 ? 0 : x - 1, prevy, x, y);
        prevy = y;
    }
}

static void draw_modulation(Canvas* c, LessonModel* m) {
    uint32_t scroll = m->anim; // pixels/frame scroll

    switch(m->step) {
    case 0: // pure carrier
        draw_wave(c, 30, 12, scroll, WaveCarrier);
        break;
    case 1: // OOK
        draw_data_track(c, 16, 21, scroll);
        draw_wave(c, 33, 9, scroll, WaveOok);
        break;
    case 2: // PSK
        draw_data_track(c, 16, 21, scroll);
        draw_wave(c, 33, 9, scroll, WavePsk);
        break;
    default: { // recovering bits from the OOK envelope
        draw_wave(c, 30, 9, scroll, WaveOok);
        /* sample at each bit centre and print the recovered bit */
        canvas_set_font(c, FontSecondary);
        for(int x = BITW / 2; x < 128; x += BITW) {
            int sx = x + (int)scroll;
            int bit = k_bits[(sx / BITW) % NBITS];
            canvas_draw_line(c, x, DIAG_Y0, x, DIAG_Y1 - 8);
            char ch[2] = {(char)('0' + bit), 0};
            canvas_draw_str_aligned(c, x, DIAG_Y1 - 1, AlignCenter, AlignBottom, ch);
        }
        break;
    }
    }
}

/* ============================================================ 1-WIRE ==== */

typedef struct {
    uint8_t len; // width in px
    uint8_t level; // 1 = high (idle), 0 = low (pulled down)
} Seg;

/* Draw a static stepped bus trace from a segment list, left-aligned at x0. */
static int bus_level_at(const Seg* segs, int nseg, int rel) {
    int acc = 0;
    for(int i = 0; i < nseg; i++) {
        acc += segs[i].len;
        if(rel < acc) return segs[i].level;
    }
    return 1;
}

static void draw_bus(Canvas* c, int x0, int y_hi, int y_lo, const Seg* segs, int nseg) {
    int total = 0;
    for(int i = 0; i < nseg; i++) total += segs[i].len;
    int prevy = y_hi;
    for(int i = 0; i <= total; i++) {
        int lvl = bus_level_at(segs, nseg, i >= total ? total - 1 : i);
        int y = lvl ? y_hi : y_lo;
        int x = x0 + i;
        if(i == 0) prevy = y;
        canvas_draw_line(c, x - 1, prevy, x, y); // vertical edge + advance
        prevy = y;
    }
}

/* A scope-style playhead sweeping across the static trace. */
static void draw_playhead(Canvas* c, int x0, int total, int y_hi, int y_lo, const Seg* segs, int nseg, float frac) {
    int rel = (int)(total * frac);
    if(rel >= total) rel = total - 1;
    int x = x0 + rel;
    for(int y = DIAG_Y0; y < DIAG_Y1; y += 3) canvas_draw_dot(c, x, y);
    int lvl = bus_level_at(segs, nseg, rel);
    canvas_draw_disc(c, x, lvl ? y_hi : y_lo, 2);
}

static void draw_onewire(Canvas* c, LessonModel* m) {
    const int x0 = 4;
    const int y_hi = 20, y_lo = 38;

    if(m->step == 0) { // one idle-high line + pull-up + endpoints
        draw_actor(c, 2, 26, 26, 14, "MCU");
        draw_actor(c, 100, 26, 26, 14, "KEY");
        /* the single data line */
        canvas_draw_line(c, 28, 22, 100, 22);
        /* pull-up resistor zigzag rising to Vcc */
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 52, 15, "+V");
        int zx = 64;
        canvas_draw_line(c, zx, 16, zx, 18);
        for(int i = 0; i < 4; i++) {
            int yy = 18 + i * 2;
            canvas_draw_line(c, zx, yy, zx + (i % 2 ? 3 : -3), yy + 1);
        }
        canvas_draw_line(c, zx, 22, zx, 24);
        /* a charge dot drifting toward the key (parasite power) */
        int px = 30 + (int)((m->anim) % 68);
        canvas_draw_disc(c, px, 22, 1);
        return;
    }
    if(m->step == 4) { // the 64-bit ROM clocking out
        const int bx = 4, bw = 120, byy = 24, bh = 12;
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, bx, 21, "64-bit ROM");
        canvas_draw_frame(c, bx, byy, bw, bh);
        /* region dividers: family(8) | serial(48) | crc(8) */
        int d1 = bx + bw * 8 / 64;
        int d2 = bx + bw * 56 / 64;
        canvas_draw_line(c, d1, byy, d1, byy + bh);
        canvas_draw_line(c, d2, byy, d2, byy + bh);
        /* data clocking in, left to right */
        int fillw = (int)((bw - 2) * saw(m->anim, 90));
        canvas_draw_box(c, bx + 1, byy + 1, fillw, bh - 2);
        canvas_draw_str(c, bx + 1, byy + bh + 9, "FAM");
        canvas_draw_str_aligned(c, (d1 + d2) / 2, byy + bh + 9, AlignCenter, AlignBottom, "SERIAL");
        canvas_draw_str_aligned(c, d2 + 8, byy + bh + 9, AlignCenter, AlignBottom, "CRC");
        return;
    }

    /* steps 1-3: static timing diagram + labels + sweeping playhead */
    static const Seg reset_segs[] = {{10, 1}, {40, 0}, {8, 1}, {24, 0}, {38, 1}};
    static const Seg write_segs[] = {{44, 0}, {16, 1}, {12, 0}, {48, 1}};
    static const Seg read_segs[] = {{40, 0}, {20, 1}, {8, 0}, {52, 1}};

    const Seg* segs;
    int nseg;
    if(m->step == 1) {
        segs = reset_segs;
        nseg = (int)(sizeof(reset_segs) / sizeof(Seg));
    } else if(m->step == 2) {
        segs = write_segs;
        nseg = (int)(sizeof(write_segs) / sizeof(Seg));
    } else {
        segs = read_segs;
        nseg = (int)(sizeof(read_segs) / sizeof(Seg));
    }
    int total = 0;
    for(int i = 0; i < nseg; i++) total += segs[i].len;

    draw_bus(c, x0, y_hi, y_lo, segs, nseg);
    draw_playhead(c, x0, total, y_hi, y_lo, segs, nseg, saw(m->anim, 80));

    canvas_set_font(c, FontSecondary);
    if(m->step == 1) {
        canvas_draw_str(c, x0 + 14, DIAG_Y1 - 1, "reset");
        canvas_draw_str(c, x0 + 60, DIAG_Y1 - 1, "presence");
    } else if(m->step == 2) {
        canvas_draw_str_aligned(c, x0 + 30, DIAG_Y1 - 1, AlignCenter, AlignBottom, "write 0");
        canvas_draw_str_aligned(c, x0 + 90, DIAG_Y1 - 1, AlignCenter, AlignBottom, "write 1");
    } else {
        canvas_draw_str_aligned(c, x0 + 30, DIAG_Y1 - 1, AlignCenter, AlignBottom, "read 0");
        canvas_draw_str_aligned(c, x0 + 90, DIAG_Y1 - 1, AlignCenter, AlignBottom, "read 1");
        /* sample markers just after each slot start */
        canvas_draw_circle(c, x0 + 12, y_lo, 2);
        canvas_draw_circle(c, x0 + 72, y_hi, 2);
    }
    (void)tri; // reserved for future eased motion
}

/* ============================================================== chrome === */

static void draw_titlebar(Canvas* c, LessonModel* m) {
    canvas_draw_box(c, 0, 0, 128, TITLE_H);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 3, 10, protocol_step_title(m->protocol, m->step));

    /* step pips on the right */
    int n = m->nsteps;
    int pr = 128 - 4;
    for(int i = n - 1; i >= 0; i--) {
        int cx = pr - (n - 1 - i) * 7;
        if(i == m->step)
            canvas_draw_disc(c, cx, 6, 2);
        else
            canvas_draw_circle(c, cx, 6, 2);
    }
    canvas_set_color(c, ColorBlack);
}

static void lesson_view_draw(Canvas* canvas, void* model) {
    LessonModel* m = model;
    canvas_clear(canvas);

    switch(m->protocol) {
    case ProtocolMifare:
        draw_mifare(canvas, m);
        break;
    case ProtocolModulation:
        draw_modulation(canvas, m);
        break;
    case ProtocolOneWire:
    default:
        draw_onewire(canvas, m);
        break;
    }

    draw_titlebar(canvas, m);

    /* caption */
    canvas_set_font(canvas, FontSecondary);
    elements_text_box(
        canvas,
        2,
        CAP_Y,
        124,
        18,
        AlignLeft,
        AlignTop,
        protocol_step_caption(m->protocol, m->step),
        false);

    /* subtle navigation chevrons */
    if(m->step > 0) canvas_draw_str(canvas, 0, 30, "<");
    if(m->step + 1 < m->nsteps) canvas_draw_str(canvas, 124, 30, ">");
}

static bool lesson_view_input(InputEvent* event, void* context) {
    LessonView* v = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyRight || event->key == InputKeyOk) {
            with_view_model(
                v->view,
                LessonModel * m,
                {
                    if(m->step + 1 < m->nsteps) {
                        m->step++;
                        m->anim = 0;
                    }
                },
                true);
            consumed = true;
        } else if(event->key == InputKeyLeft) {
            with_view_model(
                v->view,
                LessonModel * m,
                {
                    if(m->step > 0) {
                        m->step--;
                        m->anim = 0;
                    }
                },
                true);
            consumed = true;
        }
    }
    return consumed; // Back propagates to the scene manager
}

/* -------------------------------------------------------------- lifecycle */

LessonView* lesson_view_alloc(void) {
    LessonView* v = malloc(sizeof(LessonView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(LessonModel));
    view_set_draw_callback(v->view, lesson_view_draw);
    view_set_input_callback(v->view, lesson_view_input);
    return v;
}

void lesson_view_free(LessonView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* lesson_view_get_view(LessonView* v) {
    furi_assert(v);
    return v->view;
}

void lesson_view_set_protocol(LessonView* v, RosettaProtocol p) {
    furi_assert(v);
    with_view_model(
        v->view,
        LessonModel * m,
        {
            m->protocol = p;
            m->step = 0;
            m->nsteps = protocol_step_count(p);
            m->anim = 0;
        },
        true);
}

void lesson_view_tick(LessonView* v) {
    furi_assert(v);
    with_view_model(v->view, LessonModel * m, { m->anim++; }, true);
}
