#include "about_view.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <string.h>

#include "../helpers/pocketlab_fonts.h"
#include "../helpers/pocketlab_i18n.h"
#include "../helpers/pocketlab_sound.h"

#define ABOUT_PERIOD_MS  40
#define ABOUT_MATRIX_END 38 // ~1.5 s of matrix rain before the terminal boots
#define ABOUT_TYPE_SPEED 2 // characters revealed per frame

#define ABOUT_BODY_TOP 23 // ~5px below the "About" title
#define ABOUT_LINE_H   11 // 1px more air between lines
#define ABOUT_VISIBLE  4 // terminal lines fitting under the title
#define ABOUT_LEFT     3

#define ABOUT_MATRIX_COLS 16
#define ABOUT_MATRIX_STEP 8

// Coffee page Pac-Man conveyor + knock-out animation (in timer frames).
#define COFFEE_PAC_CX      82 // resting x, aligned with the left edge of the QR
#define COFFEE_PAC_CY      5 // resting y (raised 1px off the QR)
#define COFFEE_EAT_X       84 // x where a mug is swallowed
#define COFFEE_GAP         30 // frames between successive mugs
#define COFFEE_LIFESPAN    60 // frames from entering (x=128) to eaten at 3/4 px/frame
#define COFFEE_STRIKE      20 // frames of the grow + head-butt before knock-out
#define COFFEE_FALL        10 // frames of the knock-out fall (gravity parabola)
#define COFFEE_KO_MIN      150 // ~6s at 40ms/frame
#define COFFEE_KO_RANGE    225 // up to +9s more (6-15s total)
#define COFFEE_SPLAT_MIN   75 // ~3s lying splatted with flies
#define COFFEE_SPLAT_RANGE 76 // up to +3s (3-6s total)
#define COFFEE_GHOST_DUR   64 // frames for the ghost to float up and vanish
#define COFFEE_CUP_STYLES  5 // mug fill patterns

// "Scan to buy me a coffee" glitch effect.
#define ABOUT_GLITCH_SLOTS 4 // letters that can be scrambled at once
static const char about_coffee_l0[] = "Scan to buy";
static const char about_coffee_l1[] = "me a coffee";
static const char about_glitch_glyphs[] = "#@%&$*?<>+=/\\~";

// Letter index 0..10 = line 0, 11..21 = line 1. False for spaces.
static bool about_coffee_letter(uint8_t pos) {
    const char c = pos < 11 ? about_coffee_l0[pos] : about_coffee_l1[pos - 11];
    return c != ' ' && c != '\0';
}

// --- Small monochrome icons (bit c of each row = pixel x=c) -----------------

// about_icon_copyright: 9x9, a "c" inside a circle (matches version).
static const uint16_t about_icon_copyright[9] =
    {0x07C, 0x082, 0x101, 0x139, 0x109, 0x139, 0x101, 0x082, 0x07C};

// about_icon_version: 9x9, a "v" inside the same circle.
static const uint16_t about_icon_version[9] =
    {0x07C, 0x082, 0x101, 0x129, 0x129, 0x111, 0x101, 0x082, 0x07C};

// about_icon_github: 9x9 Octocat silhouette (ears, face with eyes, feet).
static const uint16_t about_icon_github[9] =
    {0x044, 0x0EE, 0x0FE, 0x1FF, 0x1B6, 0x1FF, 0x0FE, 0x07C, 0x054};

// QR code for https://perfecto-web.com/d/ (version 2-L, bit c of each row = module x=c).
#define ABOUT_QR_SIZE  25
#define ABOUT_QR_SCALE 2
static const uint32_t about_qr[ABOUT_QR_SIZE] = {
    0x1FC2F7F, 0x1056941, 0x174A25D, 0x174E85D, 0x175795D, 0x1054741, 0x1FD557F,
    0x001C300, 0x19F1B67, 0x1AD8104, 0x17DCF59, 0x0231C22, 0x104D67F, 0x18FAD36,
    0x1669BF7, 0x03C940C, 0x09F9FE3, 0x1111F00, 0x1154E7F, 0x191DF41, 0x09F965D,
    0x0D2AC5D, 0x1BB095D, 0x01B5741, 0x123177F,
};

// --- Terminal content -------------------------------------------------------

typedef struct {
    const uint16_t* icon; // optional glyph drawn left of the text
    uint8_t icon_h;
    const char* text;
    uint8_t bullet; // draw a bullet dot after this many chars (0 = none)
} AboutLine;

// The description wraps differently per language, so each has its own line set.
// The URL/repo/author handle stay literal; only the labels are translated.
static const AboutLine about_lines_en[] = {
    {NULL, 0, "PocketLab is an app that", 0},
    {NULL, 0, "teaches the Flipper Zero", 0},
    {NULL, 0, "features.", 0},
    {NULL, 0, "", 0},
    {about_icon_version, 9, "Version: " FAP_VERSION, 0},
    {about_icon_copyright, 9, "Author: PerfectoWeb", 0},
    {about_icon_github, 9, "github.com/PerfectoWeb/", 0},
    {NULL, 0, "flipper-pocketlab", 0},
};

// The URL is split so it fits the wider Cyrillic/Spanish font, and a trailing
// blank line keeps the last URL line clear of the bottom-right "coffee" hint.
static const AboutLine about_lines_ru[] = {
    {NULL, 0, "PocketLab - твой", 0},
    {NULL, 0, "карманный гайд по", 0},
    {NULL, 0, "радиочастотам и", 0},
    {NULL, 0, "Флипперу.", 0},
    {NULL, 0, "", 0},
    {about_icon_version, 9, "Версия: " FAP_VERSION, 0},
    {about_icon_copyright, 9, "Автор: PerfectoWeb", 0},
    {about_icon_github, 9, "github.com/", 0},
    {NULL, 0, "PerfectoWeb/", 0},
    {NULL, 0, "flipper-pocketlab", 0},
    {NULL, 0, "", 0},
};

static const AboutLine* about_lines(void) {
    return pocketlab_i18n_get_lang() == PocketLabLangRu ? about_lines_ru : about_lines_en;
}

// Line count varies by language (the URL wraps to more lines in RU).
static size_t about_line_count(void) {
    return pocketlab_i18n_get_lang() == PocketLabLangRu ? COUNT_OF(about_lines_ru) :
                                                          COUNT_OF(about_lines_en);
}

struct AboutView {
    View* view;
    FuriTimer* timer;
    NotificationApp* notifications;
    bool sound_enabled;
};

typedef struct {
    uint8_t pos; // scrambled letter index
    char ch; // glyph shown in its place
    uint8_t ttl; // frames remaining (0 = inactive)
} AboutGlitch;

typedef struct {
    uint32_t frame; // animation tick
    uint16_t typed; // characters revealed on the terminal screen
    uint8_t offset; // first visible terminal line
    uint8_t screen; // 0 = terminal, 1 = coffee break / QR
    bool matrix_done;
    uint16_t coffee_ko; // frame at which the Pac-Man gets knocked out
    uint16_t coffee_ghost; // frame at which the splatted Pac-Man turns to a ghost
    uint32_t coffee_seed; // randomises the decomposition pattern each visit
    uint8_t glitch_burst; // frames left in the current glitch burst
    AboutGlitch glitch[ABOUT_GLITCH_SLOTS];
} AboutModel;

static uint16_t about_total_chars(void) {
    const AboutLine* lines = about_lines();
    const size_t count = about_line_count();
    uint16_t total = 0;
    for(size_t i = 0; i < count; i++) {
        total += (uint16_t)strlen(lines[i].text);
    }
    return total;
}

static uint8_t about_max_offset(void) {
    const size_t count = about_line_count();
    return count > ABOUT_VISIBLE ? (uint8_t)(count - ABOUT_VISIBLE) : 0;
}

// Index of the line currently being typed, given how many chars are revealed.
static uint8_t about_typing_line(uint16_t typed) {
    const AboutLine* lines = about_lines();
    const size_t count = about_line_count();
    uint16_t acc = 0;
    for(size_t i = 0; i < count; i++) {
        acc += (uint16_t)strlen(lines[i].text);
        if(typed <= acc) return (uint8_t)i;
    }
    return (uint8_t)(count - 1);
}

static void
    about_draw_bitmap(Canvas* canvas, const uint16_t* rows, uint8_t height, uint8_t x, uint8_t y) {
    for(uint8_t r = 0; r < height; r++) {
        for(uint8_t c = 0; c < 12; c++) {
            if(rows[r] & (1u << c)) {
                canvas_draw_dot(canvas, x + c, y + r);
            }
        }
    }
}

// --- Matrix rain ------------------------------------------------------------

static const char about_matrix_glyphs[] = "01<>#*+=/\\%$&@?ABXY";

static void about_draw_matrix(Canvas* canvas, uint32_t frame) {
    const uint8_t pool = (uint8_t)(sizeof(about_matrix_glyphs) - 1);
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t col = 0; col < ABOUT_MATRIX_COLS; col++) {
        const uint8_t x = col * ABOUT_MATRIX_STEP;
        const uint8_t speed = 1 + (col % 3);
        const int head = (int)((frame * speed) / 2 + col * 3) % 20 - 3;
        for(int t = 0; t < 6; t++) {
            const int row = head - t;
            if(row < 0 || row > 7) continue;
            const uint8_t idx = (uint8_t)((col * 7 + row * 13 + frame / 3) % pool);
            const char ch[2] = {about_matrix_glyphs[idx], '\0'};
            const uint8_t y = (uint8_t)(row * 8);
            if(t == 0) {
                // Bright leading glyph: inverted on a filled cell.
                canvas_draw_box(canvas, x, y, 7, 8);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, x, y + 7, ch);
                canvas_set_color(canvas, ColorBlack);
            } else if((row + col + (int)(frame / 4)) % 2 == 0) {
                // Sparse trailing glyphs, to suggest a fading tail.
                canvas_draw_str(canvas, x, y + 7, ch);
            }
        }
    }

    // Centred logo plate the rain appears to flow around, like a boot splash.
    const uint8_t bw = 92;
    const uint8_t bh = 22;
    const uint8_t bx = (128 - bw) / 2;
    const uint8_t by = (64 - bh) / 2;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, bx, by, bw, bh, 4);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rframe(canvas, bx, by, bw, bh, 4);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, by + bh / 2, AlignCenter, AlignCenter, "PocketLab");
    canvas_set_color(canvas, ColorBlack);
}

// --- Screens ----------------------------------------------------------------

static void about_draw_terminal(Canvas* canvas, AboutModel* model) {
    // Plain, bold title instead of a terminal status bar.
    pocketlab_font_apply(canvas, true);
    canvas_draw_str(canvas, ABOUT_LEFT, 11, pocketlab_text(PocketLabTextMenuAbout));

    const uint16_t total = about_total_chars();
    const uint16_t typed = model->typed < total ? model->typed : total;
    const bool typing = model->typed < total;

    // Reveal text line by line; count characters up to the visible window.
    const AboutLine* lines = about_lines();
    // The Universal (Cyrillic) font is a touch taller, so it needs more leading.
    const uint8_t line_h = pocketlab_font_is_universal() ? 12 : ABOUT_LINE_H;
    uint16_t remaining = typed;
    uint8_t cursor_x = ABOUT_LEFT;
    uint8_t cursor_y = ABOUT_BODY_TOP;
    bool cursor_set = false;

    pocketlab_font_apply_small(canvas);
    const size_t line_count = about_line_count();
    for(size_t i = 0; i < line_count; i++) {
        const AboutLine* line = &lines[i];
        const uint8_t length = (uint8_t)strlen(line->text);
        uint8_t shown = remaining >= length ? length : (uint8_t)remaining;
        if(remaining < length) {
            // This line is mid-typing; nothing after it is revealed yet.
            remaining = 0;
        } else {
            remaining -= shown;
        }

        const bool visible = i >= (size_t)model->offset &&
                             i < (size_t)model->offset + ABOUT_VISIBLE;
        if(visible && shown > 0) {
            const uint8_t y = (uint8_t)(ABOUT_BODY_TOP + (i - model->offset) * line_h);
            uint8_t tx = ABOUT_LEFT;
            if(line->icon) {
                // A 9px icon centred on the ~7px text: 1px above and 1px below.
                // The Universal font sits a touch lower, so lift the icon 1px there.
                const int8_t icon_dy = pocketlab_font_is_universal() ? -1 : 0;
                about_draw_bitmap(
                    canvas,
                    line->icon,
                    line->icon_h,
                    tx,
                    (uint8_t)(y - line->icon_h + 2 + icon_dy));
                tx += 12;
            }
            char buffer[64];
            uint8_t n = shown < sizeof(buffer) - 1 ? shown : (uint8_t)(sizeof(buffer) - 1);
            memcpy(buffer, line->text, n);
            // Don't leave a dangling UTF-8 lead byte mid-type (Cyrillic is 2 bytes).
            if(n > 0 && (unsigned char)buffer[n - 1] >= 0xC0) n--;
            buffer[n] = '\0';
            canvas_draw_str(canvas, tx, y, buffer);

            // Separator dot between the version and the author, drawn by hand
            // since the font has no reliable bullet glyph.
            if(line->bullet && shown > line->bullet) {
                char pre[8];
                const uint8_t pn = line->bullet < sizeof(pre) - 1 ? line->bullet : sizeof(pre) - 1;
                memcpy(pre, line->text, pn);
                pre[pn] = '\0';
                canvas_draw_box(canvas, tx + canvas_string_width(canvas, pre) + 4, y - 5, 2, 2);
            }

            if(shown < length && !cursor_set) {
                cursor_x = tx + canvas_string_width(canvas, buffer);
                cursor_y = y;
                cursor_set = true;
            }
        }

        if(shown < length) break; // stop at the line still being typed
    }

    // Blinking block cursor while typing.
    if(typing && cursor_set) {
        canvas_draw_box(canvas, cursor_x + 1, cursor_y - 7, 4, 8);
    }

    // Once the log is written, hint that the coffee page is one press away.
    if(!typing && (model->frame / 15) % 2 == 0) {
        pocketlab_font_apply_small(canvas);
        canvas_draw_str_aligned(
            canvas, 125, 61, AlignRight, AlignBottom, pocketlab_text(PocketLabTextCoffeeHint));
    }
}

static void about_draw_qr(Canvas* canvas, uint8_t x0, uint8_t y0) {
    for(uint8_t r = 0; r < ABOUT_QR_SIZE; r++) {
        for(uint8_t c = 0; c < ABOUT_QR_SIZE; c++) {
            if(about_qr[r] & (1u << c)) {
                canvas_draw_box(
                    canvas,
                    x0 + c * ABOUT_QR_SCALE,
                    y0 + r * ABOUT_QR_SCALE,
                    ABOUT_QR_SCALE,
                    ABOUT_QR_SCALE);
            }
        }
    }
}

// Inverted pill button in the top-left corner, doubling as the page title.
static void about_draw_back_button(Canvas* canvas) {
    canvas_draw_rbox(canvas, 2, 1, 40, 13, 3);
    canvas_set_color(canvas, ColorWhite);
    pocketlab_font_apply_small(canvas);
    const char* back = pocketlab_text(PocketLabTextBack); // "< back" / "< назад" / "< atrás"

    if(!pocketlab_font_is_universal()) {
        // English: unchanged, single centred string.
        canvas_draw_str_aligned(canvas, 21, 7, AlignCenter, AlignCenter, back);
    } else {
        // RU/ES: split the leading arrow off so it can be nudged 1px right and
        // 1px down to sit better on the pill, keeping the word centred.
        const char* sp = strchr(back, ' ');
        const char* word = sp ? sp + 1 : back;
        const int full_w = (int)canvas_string_width(canvas, back);
        const int arrow_w = (int)canvas_string_width(canvas, "<");
        const int word_w = (int)canvas_string_width(canvas, word);
        const int gap = full_w - arrow_w - word_w; // the space advance
        const int left = 21 - full_w / 2;
        canvas_draw_str_aligned(canvas, (uint8_t)(left + 1), 8, AlignLeft, AlignCenter, "<");
        canvas_draw_str_aligned(
            canvas, (uint8_t)(left + arrow_w + gap), 7, AlignLeft, AlignCenter, word);
    }
    canvas_set_color(canvas, ColorBlack);
}

// A steaming coffee mug with a handle; ry is the y of the rim. `style` picks a
// fill pattern for variety. The steam drifts right, trailing the moving mug.
static void about_draw_cup(Canvas* canvas, uint8_t x, uint8_t ry, uint32_t frame, uint8_t style) {
    canvas_draw_line(canvas, x, ry, x + 6, ry); // rim
    canvas_draw_line(canvas, x, ry + 1, x, ry + 5); // left wall
    canvas_draw_line(canvas, x + 6, ry + 1, x + 6, ry + 5); // right wall
    canvas_draw_line(canvas, x + 1, ry + 6, x + 5, ry + 6); // base
    canvas_draw_line(canvas, x + 7, ry + 1, x + 8, ry + 2); // handle
    canvas_draw_line(canvas, x + 8, ry + 2, x + 8, ry + 3);
    canvas_draw_line(canvas, x + 7, ry + 4, x + 8, ry + 3);

    // Fill pattern inside the body (x+1..x+5, ry+1..ry+5).
    switch(style) {
    case 1: // solid
        canvas_draw_box(canvas, x + 1, ry + 1, 5, 5);
        break;
    case 2: // horizontal stripes
        canvas_draw_line(canvas, x + 1, ry + 2, x + 5, ry + 2);
        canvas_draw_line(canvas, x + 1, ry + 4, x + 5, ry + 4);
        break;
    case 3: // dotted, checkerboard
        for(uint8_t dy = 0; dy < 5; dy++) {
            for(uint8_t dx = (dy & 1); dx < 5; dx += 2) {
                canvas_draw_dot(canvas, x + 1 + dx, ry + 1 + dy);
            }
        }
        break;
    case 4: // vertical stripes
        canvas_draw_line(canvas, x + 2, ry + 1, x + 2, ry + 5);
        canvas_draw_line(canvas, x + 4, ry + 1, x + 4, ry + 5);
        break;
    default: // 0: plain outline (the classic mug)
        break;
    }

    for(uint8_t s = 0; s < 3; s++) {
        const uint8_t h = (uint8_t)((frame / 3 + s * 2) % 6);
        const int sy = (int)ry - 1 - (int)h;
        if(sy >= 0) canvas_draw_dot(canvas, x + 1 + s * 2 + h, (uint8_t)sy); // wind trail
    }
}

// The attacking mug drawn as a slanted parallelogram; `tilt` shears the top
// (negative = leaning left toward the Pac-Man for the head-butt).
static void about_draw_hitcup(Canvas* canvas, int bx, int by, int w, int h, int tilt) {
    const int tlx = bx + tilt, tly = by - h; // top-left
    const int trx = bx + w + tilt; // top-right x
    canvas_draw_line(canvas, tlx, tly, trx, tly); // rim
    canvas_draw_line(canvas, bx, by, bx + w, by); // base
    canvas_draw_line(canvas, tlx, tly, bx, by); // left wall
    canvas_draw_line(canvas, trx, tly, bx + w, by); // right wall
    canvas_draw_line(canvas, trx, tly + 1, trx + 2, tly + 2); // handle
    canvas_draw_line(canvas, trx + 2, tly + 2, trx + 1, tly + 4);
}

// Pac-Man chewing to the right, mouth width given by the caller.
static void about_draw_pacman(Canvas* canvas, uint8_t cx, uint8_t cy, uint8_t mouth) {
    canvas_draw_disc(canvas, cx, cy, 5);
    if(mouth > 0) {
        canvas_set_color(canvas, ColorWhite);
        for(uint8_t i = 0; i <= mouth; i++) {
            canvas_draw_line(canvas, cx, cy, cx + 6, cy - i);
            canvas_draw_line(canvas, cx, cy, cx + 6, cy + i);
        }
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, cx - 2, cy - 2); // eye
    canvas_set_color(canvas, ColorBlack);
}

// The coffee break scene: a conveyor of mugs feeding the Pac-Man, until a
// random moment when a mug knocks it out and it splats at the bottom with flies.
static void about_draw_pacman_snack(
    Canvas* canvas,
    uint32_t frame,
    uint16_t ko,
    uint16_t ghost,
    uint32_t seed) {
    if(frame < (uint32_t)ko - COFFEE_STRIKE) {
        // Find the nearest mug so the mouth can open just before it touches.
        int nearest = 999;
        const int slot = (int)(frame / COFFEE_GAP);
        for(int n = slot - 2; n <= slot; n++) {
            if(n < 0) continue;
            const int age = (int)frame - n * (int)COFFEE_GAP;
            if(age < 0 || age > COFFEE_LIFESPAN) continue;
            const int cx = 128 - (age * 3) / 4; // 3/4 px/frame
            if(cx > COFFEE_EAT_X && cx < nearest) nearest = cx;
        }
        // Open ~2px before the mug reaches the mouth, hold while it slides in.
        const uint8_t mouth = (nearest >= COFFEE_EAT_X && nearest <= COFFEE_PAC_CX + 8) ? 5 : 1;
        about_draw_pacman(canvas, COFFEE_PAC_CX, COFFEE_PAC_CY, mouth);

        // Conveyor: mugs streaming in from the right edge, drawn over the mouth.
        for(int n = slot - 2; n <= slot; n++) {
            if(n < 0) continue;
            const int age = (int)frame - n * (int)COFFEE_GAP;
            if(age < 0 || age > COFFEE_LIFESPAN) continue;
            const int cx = 128 - (age * 3) / 4;
            if(cx <= COFFEE_EAT_X) continue; // swallowed
            // Cycle through every fill style (including solid black) as mugs pass.
            const uint8_t style = (uint8_t)(((n % COFFEE_CUP_STYLES) + 5) % COFFEE_CUP_STYLES);
            about_draw_cup(canvas, (uint8_t)cx, 3, frame, style);
        }
    } else if(frame < (uint32_t)ko) {
        // The dangerous mug stops, grows while blinking (Mario-style), leans
        // back, then swings its whole body forward to head-butt the Pac-Man.
        const int t = COFFEE_STRIKE - ((int)ko - (int)frame); // 0..STRIKE-1
        about_draw_pacman(canvas, COFFEE_PAC_CX, COFFEE_PAC_CY, 5);

        const int by = 10; // mug base y
        int w, h, tilt;
        if(t < 12) {
            // Grow slowly, blinking big/small like a Mario power-up.
            const bool big = (t % 4) < 2;
            w = big ? 10 : 7;
            h = big ? 10 : 7;
            tilt = 0;
        } else if(t < 16) {
            // Lean back (top shears right, away from the Pac-Man).
            w = 10;
            h = 10;
            tilt = (t - 11) * 2; // +2..+8
        } else {
            // Swing the whole body forward into the Pac-Man.
            w = 10;
            h = 10;
            tilt = 8 - (t - 15) * 6; // +8 -> -16
        }
        about_draw_hitcup(canvas, 94, by, w, h, tilt);
    } else if(frame < (uint32_t)ko + COFFEE_FALL) {
        // Knocked off: a quick pop up, then an accelerating free fall.
        const int t = (int)frame - (int)ko;
        const int fx = COFFEE_PAC_CX - (COFFEE_PAC_CX - 68) * t / COFFEE_FALL;
        int fy = COFFEE_PAC_CY - 3 * t + t * t; // short rise, gravity takes over
        if(fy < 0) fy = 0;
        if(t <= 1) {
            // Stretched upward at the moment of impact.
            canvas_draw_disc(canvas, (uint8_t)fx, (uint8_t)fy, 4);
            canvas_draw_disc(canvas, (uint8_t)fx, (uint8_t)(fy - 3), 4);
        } else {
            canvas_draw_disc(canvas, (uint8_t)fx, (uint8_t)fy, 5);
        }
        canvas_set_color(canvas, ColorWhite); // dizzy X-eyes
        canvas_draw_line(canvas, fx - 3, fy - 3, fx - 1, fy - 1);
        canvas_draw_line(canvas, fx - 1, fy - 3, fx - 3, fy - 1);
        canvas_draw_line(canvas, fx + 1, fy - 3, fx + 3, fy - 1);
        canvas_draw_line(canvas, fx + 3, fy - 3, fx + 1, fy - 1);
        canvas_set_color(canvas, ColorBlack);
    } else if(frame < (uint32_t)ghost) {
        // Splatted, slowly decomposing: the puddle gets eaten away by holes and
        // gives off rising specks, with flies circling, until the ghost leaves.
        const uint8_t sx = 66;
        const uint8_t sy = 60;
        const int start = (int)ko + COFFEE_FALL;
        const int total = (int)ghost - start;
        const int p = (int)frame - start;

        canvas_draw_rbox(canvas, sx - 8, sy - 3, 17, 5, 2);
        // Random decay: holes accrue over time, some 1px, some big chunks. The
        // seed makes every visit rot differently, but stable within a visit.
        const int holes = total > 0 ? p * 14 / total : 0;
        canvas_set_color(canvas, ColorWhite);
        for(int i = 0; i < holes; i++) {
            const uint32_t hsh = seed * 2654435761u + (uint32_t)i * 2246822519u;
            const int hx = sx - 7 + (int)(hsh % 15);
            const int hy = sy - 3 + (int)((hsh >> 8) % 3); // eat from the top, where the flies are
            const uint32_t sz = (hsh >> 16) % 5; // mostly small, sometimes chunks
            if(sz < 3) {
                canvas_draw_dot(canvas, hx, hy);
            } else if(sz == 3) {
                canvas_draw_box(canvas, hx, hy, 2, 2);
            } else {
                canvas_draw_box(canvas, hx, hy, 3, 2);
            }
        }
        canvas_set_color(canvas, ColorBlack);
        // Flies circling above.
        for(uint8_t i = 0; i < 3; i++) {
            const int fx = sx - 5 + i * 5 + (int)((frame / 2 + i * 4) % 7) - 3;
            const int fy = sy - 10 + (int)((frame / 3 + i * 3) % 6) - 3;
            canvas_draw_dot(canvas, fx, fy);
            canvas_draw_dot(canvas, fx + 1, fy);
        }
        // Gas specks rising from the rot.
        for(uint8_t s = 0; s < 2; s++) {
            const int spy = sy - 5 - (int)((frame / 2 + s * 6) % 10);
            const int spx = sx - 3 + s * 5 + (int)((frame / 5 + s) % 3) - 1;
            canvas_draw_dot(canvas, spx, spy);
        }
    } else if(frame < (uint32_t)ghost + COFFEE_GHOST_DUR) {
        // Its ghost drifts up airily, swaying gently, fading out near the top.
        static const int8_t ghost_sway[12] = {0, 1, 2, 2, 2, 1, 0, -1, -2, -2, -2, -1};
        const int gt = (int)frame - (int)ghost;
        const int gx = 66 + ghost_sway[gt % 12]; // smooth sway
        const int gy = 55 - gt; // slow rise, 1px/frame
        const bool fade = gt > COFFEE_GHOST_DUR - 10 && (gt & 1); // soft flicker at the very end
        if(!fade && gy > -8) {
            canvas_draw_disc(canvas, (uint8_t)gx, (uint8_t)gy, 5); // round head
            canvas_draw_box(canvas, gx - 5, gy, 11, 6); // body
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, gx - 4, gy + 5, 2, 1); // skirt notches
            canvas_draw_box(canvas, gx + 1, gy + 5, 2, 1);
            canvas_draw_box(canvas, gx - 3, gy - 1, 2, 2); // eyes
            canvas_draw_box(canvas, gx + 1, gy - 1, 2, 2);
            canvas_set_color(canvas, ColorBlack);
        }
    }
    // else: gone for good.
}

static void about_draw_coffee(Canvas* canvas, AboutModel* model) {
    about_draw_back_button(canvas);

    pocketlab_font_apply_small(canvas);
    canvas_draw_str(canvas, 5, 27, pocketlab_text(PocketLabTextEnjoying));
    canvas_draw_str(canvas, 5, 37, "PocketLab?");

    // "Scan to buy me a coffee" always on, but with letters briefly scrambled
    // into matrix-like glyphs and back.
    char b0[16];
    char b1[16];
    strncpy(b0, about_coffee_l0, sizeof(b0) - 1);
    b0[sizeof(b0) - 1] = '\0';
    strncpy(b1, about_coffee_l1, sizeof(b1) - 1);
    b1[sizeof(b1) - 1] = '\0';
    for(uint8_t i = 0; i < ABOUT_GLITCH_SLOTS; i++) {
        if(model->glitch[i].ttl == 0) continue;
        const uint8_t p = model->glitch[i].pos;
        if(p < 11) {
            b0[p] = model->glitch[i].ch;
        } else {
            b1[p - 11] = model->glitch[i].ch;
        }
    }
    canvas_draw_str(canvas, 5, 50, b0);
    canvas_draw_str(canvas, 5, 59, b1);

    about_draw_pacman_snack(
        canvas, model->frame, model->coffee_ko, model->coffee_ghost, model->coffee_seed);

    // QR on the right, raised 1px so it is not glued to the bottom edge.
    about_draw_qr(canvas, 76, 12);
}

static void about_view_draw_callback(Canvas* canvas, void* context) {
    AboutModel* model = context;
    canvas_clear(canvas);

    if(model->screen == 1) {
        about_draw_coffee(canvas, model);
    } else if(!model->matrix_done) {
        about_draw_matrix(canvas, model->frame);
    } else {
        about_draw_terminal(canvas, model);
    }
}

// --- Timer / input ----------------------------------------------------------

static void about_view_timer_callback(void* context) {
    AboutView* instance = context;
    const uint16_t total = about_total_chars();
    bool play_tick = false;
    bool play_geiger = false;
    bool play_sip = false;
    bool play_thud = false;
    bool play_splat = false;
    bool play_grow = false;
    bool play_glitch = false;
    uint8_t glitch_variant = 0;

    with_view_model(
        instance->view,
        AboutModel * model,
        {
            model->frame++;
            if(model->screen == 0) {
                if(!model->matrix_done) {
                    if(model->frame >= ABOUT_MATRIX_END) {
                        model->matrix_done = true;
                        model->frame = 0;
                    } else {
                        // Irregular, sparse ticks, like radiation over the rain.
                        play_geiger = (furi_hal_random_get() % 6) == 0;
                    }
                } else if(model->typed < total) {
                    model->typed += ABOUT_TYPE_SPEED;
                    if(model->typed > total) model->typed = total;
                    if(model->typed >= total) {
                        // Once fully typed, rest at the very bottom so the trailing
                        // blank line sits under the "coffee" hint (no overlap).
                        model->offset = about_max_offset();
                    } else {
                        // Keep the line being typed at the bottom of the viewport.
                        const uint8_t line = about_typing_line(model->typed);
                        model->offset = line >= ABOUT_VISIBLE ? line - ABOUT_VISIBLE + 1 : 0;
                    }
                    play_tick = (model->frame % 2) == 0;
                }
            } else if(model->screen == 1) {
                const uint32_t ko = model->coffee_ko;
                const uint32_t f = model->frame;
                const bool dead = f >= ko + COFFEE_FALL;
                if(f >= COFFEE_LIFESPAN && f < ko - COFFEE_STRIKE &&
                   (f % COFFEE_GAP) == (COFFEE_LIFESPAN % COFFEE_GAP)) {
                    play_sip = true; // gulp exactly when a mug reaches the mouth
                } else if(f == ko - COFFEE_STRIKE) {
                    play_grow = true; // the mug powers up, Mario-style
                } else if(f == ko) {
                    play_thud = true; // final mug knocks it out
                } else if(f == ko + COFFEE_FALL) {
                    play_splat = true; // hits the bottom
                }

                // Glitches only after death, and in bursts with quiet pauses.
                for(uint8_t i = 0; i < ABOUT_GLITCH_SLOTS; i++) {
                    if(model->glitch[i].ttl > 0) model->glitch[i].ttl--;
                }
                bool spawn = false;
                if(dead) {
                    if(model->glitch_burst > 0) {
                        model->glitch_burst--;
                        spawn = (furi_hal_random_get() % 2) == 0; // dense during a burst
                    } else if((furi_hal_random_get() % 25) == 0) {
                        model->glitch_burst = (uint8_t)(6 + furi_hal_random_get() % 18);
                    }
                }
                if(spawn) {
                    for(uint8_t i = 0; i < ABOUT_GLITCH_SLOTS; i++) {
                        if(model->glitch[i].ttl != 0) continue;
                        const uint8_t p = (uint8_t)(furi_hal_random_get() % 22);
                        if(about_coffee_letter(p)) {
                            model->glitch[i].pos = p;
                            model->glitch[i].ch = about_glitch_glyphs
                                [furi_hal_random_get() % (sizeof(about_glitch_glyphs) - 1)];
                            model->glitch[i].ttl = (uint8_t)(2 + furi_hal_random_get() % 5);
                            play_glitch = true;
                            glitch_variant = (uint8_t)(furi_hal_random_get() % 3);
                        }
                        break;
                    }
                }
            }
        },
        true);

    if(play_tick) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, PocketLabSoundType);
    }
    if(play_geiger) {
        pocketlab_sound_play(
            instance->notifications, instance->sound_enabled, PocketLabSoundGeiger);
    }
    if(play_sip) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, PocketLabSoundSip);
    }
    if(play_thud) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, PocketLabSoundThud);
    }
    if(play_splat) {
        pocketlab_sound_play(
            instance->notifications, instance->sound_enabled, PocketLabSoundSplat);
    }
    if(play_grow) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, PocketLabSoundGrow);
    }
    if(play_glitch) {
        // Vary the click so the glitch never sounds the same twice.
        static const PocketLabSoundId glitch_snd[3] = {
            PocketLabSoundGlitch, PocketLabSoundType, PocketLabSoundGeiger};
        pocketlab_sound_play(
            instance->notifications, instance->sound_enabled, glitch_snd[glitch_variant]);
    }
}

static bool about_view_input_callback(InputEvent* event, void* context) {
    AboutView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    const uint16_t total = about_total_chars();
    bool consumed = false;

    with_view_model(
        instance->view,
        AboutModel * model,
        {
            if(model->screen == 0 && !model->matrix_done) {
                // Any key skips straight to the terminal.
                model->matrix_done = true;
                model->frame = 0;
                consumed = true;
            } else if(model->screen == 0 && model->typed < total) {
                // Reveal the whole log at once.
                model->typed = total;
                model->offset = about_max_offset();
                consumed = true;
            } else if(model->screen == 0) {
                if(event->key == InputKeyDown) {
                    if(model->offset < about_max_offset()) model->offset++;
                    consumed = true;
                } else if(event->key == InputKeyUp) {
                    if(model->offset > 0) model->offset--;
                    consumed = true;
                } else if(event->key == InputKeyRight || event->key == InputKeyOk) {
                    model->screen = 1;
                    model->frame = 0;
                    model->glitch_burst = 0;
                    for(uint8_t i = 0; i < ABOUT_GLITCH_SLOTS; i++) {
                        model->glitch[i].ttl = 0;
                    }
                    // Random knock-out (~6-15s), then a random 3-6s splat before
                    // the ghost floats away.
                    model->coffee_ko =
                        (uint16_t)(COFFEE_KO_MIN + furi_hal_random_get() % COFFEE_KO_RANGE);
                    model->coffee_ghost =
                        (uint16_t)(model->coffee_ko + COFFEE_FALL + COFFEE_SPLAT_MIN +
                                   furi_hal_random_get() % COFFEE_SPLAT_RANGE);
                    model->coffee_seed = furi_hal_random_get();
                    consumed = true;
                }
            } else { // coffee / QR screen
                if(event->key == InputKeyLeft || event->key == InputKeyOk ||
                   event->key == InputKeyBack) {
                    model->screen = 0;
                    model->frame = 0;
                    consumed = true;
                }
            }
        },
        true);

    return consumed;
}

static void about_view_enter_callback(void* context) {
    AboutView* instance = context;
    with_view_model(
        instance->view,
        AboutModel * model,
        {
            model->frame = 0;
            model->typed = 0;
            model->offset = 0;
            model->screen = 0;
            model->matrix_done = false;
            model->glitch_burst = 0;
            for(uint8_t i = 0; i < ABOUT_GLITCH_SLOTS; i++) {
                model->glitch[i].ttl = 0;
            }
        },
        true);
    furi_timer_start(instance->timer, furi_ms_to_ticks(ABOUT_PERIOD_MS));
}

static void about_view_exit_callback(void* context) {
    AboutView* instance = context;
    furi_timer_stop(instance->timer);
}

AboutView* about_view_alloc(void) {
    AboutView* instance = malloc(sizeof(AboutView));
    instance->view = view_alloc();
    instance->notifications = NULL;
    instance->sound_enabled = false;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(AboutModel));
    view_set_draw_callback(instance->view, about_view_draw_callback);
    view_set_input_callback(instance->view, about_view_input_callback);
    view_set_enter_callback(instance->view, about_view_enter_callback);
    view_set_exit_callback(instance->view, about_view_exit_callback);

    instance->timer = furi_timer_alloc(about_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void about_view_free(AboutView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* about_view_get_view(AboutView* instance) {
    furi_assert(instance);
    return instance->view;
}

void about_view_configure(AboutView* instance, NotificationApp* notifications, bool sound_enabled) {
    furi_assert(instance);
    instance->notifications = notifications;
    instance->sound_enabled = sound_enabled;
}
