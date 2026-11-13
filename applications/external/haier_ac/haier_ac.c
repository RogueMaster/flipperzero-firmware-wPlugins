/*
 * Haier AC Remote - telecomando grafico per Flipper Zero (Momentum)
 * Climatizzatore: Haier AS35RBAHRA-3, telecomando YR-HE2
 *
 * Interfaccia a pulsanti-icona (stile telecomando): croce direzionale per
 * spostare la selezione, OK per "premere" il tasto. Ogni pressione invia lo
 * STATO COMPLETO, come il telecomando originale.
 *
 * Schermata principale: Temp+/Temp-, Power, Mode, Fan, SwingV, SwingH,
 *                       Turbo/Quiet, Eco, Sleep, More.
 * Schermata More:       Health, Self clean, Back (Back del Flipper = indietro).
 * Con il condizionatore su OFF solo Power invia il segnale; gli altri tasti
 * modificano lo stato senza trasmettere.
 *
 * Protocollo: frame da 176 bit (22 byte), 38 kHz, 2 checksum a somma
 * (byte 13 = somma byte 0..12, byte 21 = somma byte 14..20).
 *
 *   byte 1  : (temp-16)<<4 | posizione alette verticali (0 se spento):
 *             pos1=A, pos2=8, pos3=6, pos4=4, pos5=2, auto=C  (= 2*(6-pos))
 *   byte 2  : swing orizzontale: 1=60 2=80 3=00(centro) 4=A0 5=C0 6=E0(auto)
 *   byte 3  : 0x02 Health
 *   byte 4  : 0x40 acceso, 0x00 spento
 *   byte 5  : ventola auto A0 / low 60 / med 40 / high 20, |0x08 se Sleep
 *   byte 6  : 0x40 Turbo, 0x80 Quiet
 *   byte 7  : modo auto 00 / cool 20 / dry 40 / heat 80 / fan C0
 *   byte 8  : 0x80 se Sleep
 *   byte 10 : 0x10 Self clean
 *   byte 12 : codice dell'ULTIMO TASTO premuto:
 *             temp+ 00, temp- 01, swing 02, swingH 03, fan 04, power 05,
 *             mode 06, health 07, turbo/quiet 08, sleep 0B, eco 17, self clean 19
 *   byte 14 : 0xB7
 *   byte 15 : 0x40 Self clean | Eco: 0x10 | (livello<<2)  (L1=14 L2=18 L3=1C)
 *   byte 16 : ventola low C0 / med 80 / high 40, auto = 00
 *
 * Regole del telecomando: pos 1 alette solo in Heat; Sleep non in Fan;
 * Turbo/Quiet solo in Cool e Heat (il primo clic porta SwingV a 3) e escludono
 * la ventola manuale (premere FAN li disattiva e riporta la ventola ad Auto);
 * in Fan niente ventola Auto; Eco in tutti i modi tranne Fan; Self clean solo
 * in Cool, Dry e Fan (qui e' un'azione: OK la invia).
 *
 * Non ancora verificato: ordine reale dei modi col tasto Mode (qui
 * Auto > Cool > Dry > Heat > Fan), Turbo/Quiet con ventola non auto, ventola
 * low/med/high in Cool/Heat/Dry, timer.
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <infrared_transmit.h>

#define FRAME_BYTES   22
#define FRAME_BITS    176
#define TIMINGS_COUNT (4 + FRAME_BITS * 2 + 1) /* 357 */

#define T_HDR_1 3050
#define T_HDR_2 3059
#define T_HDR_3 4418
#define T_MARK  555
#define T_ONE   1670
#define T_ZERO  565

/* codici byte 12 (ultimo tasto) */
#define KEY_TEMP_UP   0x00
#define KEY_TEMP_DOWN 0x01
#define KEY_SWING     0x02
#define KEY_SWING_H   0x03
#define KEY_FAN       0x04
#define KEY_POWER     0x05
#define KEY_MODE      0x06
#define KEY_HEALTH    0x07
#define KEY_TURBO     0x08
#define KEY_SLEEP     0x0B
#define KEY_ECO       0x17
#define KEY_CLEAN     0x19

#define ARR_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

typedef enum { ModeAuto, ModeCool, ModeHeat, ModeDry, ModeFan, ModeCount } AcMode;
typedef enum { FanAuto, FanLow, FanMed, FanHigh, FanCount } AcFan;

#define SWH_COUNT  6 /* indici 0..5 = posizioni 1..6 (1 sx, 3 centro, 5 dx, 6 auto) */
#define SWH_CENTER 2
#define SWV_AUTO   6 /* swingV: 1..5 = posizioni, 6 = auto */
#define TQ_OFF     0
#define TQ_TURBO   1
#define TQ_QUIET   2

static const uint8_t mode_code[ModeCount] = {0x00, 0x20, 0x80, 0x40, 0xC0};
static const uint8_t fan_code[FanCount] = {0xA0, 0x60, 0x40, 0x20};
static const uint8_t fan_b16_code[FanCount] = {0x00, 0xC0, 0x80, 0x40};
static const uint8_t swh_code[SWH_COUNT] = {0x60, 0x80, 0x00, 0xA0, 0xC0, 0xE0};
static const char* mode_name[ModeCount] = {"Auto", "Cool", "Heat", "Dry", "Fan"};
static const char* fan_name[FanCount] = {"Auto", "Low", "Med", "High"};
/* ordine del tasto Mode: Auto > Cool > Dry > Heat > Fan > Auto */
static const AcMode mode_next[ModeCount] = {ModeCool, ModeDry, ModeFan, ModeHeat, ModeAuto};

typedef struct {
    bool power;
    AcMode mode;
    uint8_t temp; /* 16..30 */
    AcFan fan;
    uint8_t swing_v; /* 1..5, SWV_AUTO */
    uint8_t swing_h; /* 0..5 */
    bool health;
    bool sleep;
    uint8_t tq; /* TQ_* */
    uint8_t eco; /* 0 = off, 1..3 = L1..L3 */
    uint8_t last_key; /* byte 12 */
} AcState;

static bool temp_allowed(const AcState* s) {
    return s->mode != ModeFan; /* in Fan il telecomando non mostra ne' cambia la temperatura */
}
static bool sleep_allowed(const AcState* s) {
    return s->mode != ModeFan;
}
static bool tq_allowed(const AcState* s) {
    return s->mode == ModeCool || s->mode == ModeHeat;
}
static bool eco_allowed(const AcState* s) {
    return s->mode != ModeFan;
}
static bool clean_allowed(const AcState* s) {
    return s->mode == ModeCool || s->mode == ModeDry || s->mode == ModeFan;
}

/* ------------------------------------------------------------------ */
/* Protocollo                                                         */
/* ------------------------------------------------------------------ */

static void build_frame(const AcState* s, bool clean, uint8_t f[FRAME_BYTES]) {
    memset(f, 0, FRAME_BYTES);
    uint8_t swc = (s->swing_v == SWV_AUTO) ? 0x0C : (uint8_t)(2 * (6 - s->swing_v));
    /* con Turbo/Quiet attivi la ventola manuale e' esclusa: il telecomando manda Auto */
    AcFan fan = s->tq ? FanAuto : s->fan;

    f[0] = 0xA6;
    f[1] = ((s->temp - 16) << 4) | (s->power ? swc : 0);
    f[2] = swh_code[s->swing_h];
    f[3] = s->health ? 0x02 : 0x00;
    f[4] = s->power ? 0x40 : 0x00;
    f[5] = fan_code[fan] | (s->sleep ? 0x08 : 0x00);
    f[6] = (s->tq == TQ_TURBO) ? 0x40 : (s->tq == TQ_QUIET) ? 0x80 : 0x00;
    f[7] = mode_code[s->mode];
    f[8] = s->sleep ? 0x80 : 0x00;
    f[10] = clean ? 0x10 : 0x00;
    f[12] = clean ? KEY_CLEAN : s->last_key;
    uint8_t sum = 0;
    for(int i = 0; i < 13; i++) sum += f[i];
    f[13] = sum;
    f[14] = 0xB7;
    f[15] = (clean ? 0x40 : 0x00) | (s->eco ? (0x10 | (s->eco << 2)) : 0x00);
    f[16] = fan_b16_code[fan];
    sum = 0;
    for(int i = 14; i < 21; i++) sum += f[i];
    f[21] = sum;
}

static void send_state(const AcState* s, bool clean) {
    static uint32_t t[TIMINGS_COUNT]; /* statico: non pesa sullo stack */
    uint8_t f[FRAME_BYTES];
    build_frame(s, clean, f);

    uint32_t n = 0;
    t[n++] = T_HDR_1;
    t[n++] = T_HDR_1;
    t[n++] = T_HDR_2;
    t[n++] = T_HDR_3;
    for(int i = 0; i < FRAME_BYTES; i++) {
        for(int b = 7; b >= 0; b--) { /* MSB first */
            t[n++] = T_MARK;
            t[n++] = (f[i] >> b) & 1 ? T_ONE : T_ZERO;
        }
    }
    t[n++] = T_MARK;

    infrared_send_raw_ext(t, n, true, 38000, 0.33f);
}

/* ------------------------------------------------------------------ */
/* Pulsanti                                                           */
/* ------------------------------------------------------------------ */

typedef enum {
    BtnTempUp,
    BtnTempDown,
    BtnPower,
    BtnMode,
    BtnFan,
    BtnSwingV,
    BtnSwingH,
    BtnTurbo,
    BtnEco,
    BtnSleep,
    BtnMore,
    BtnHealth,
    BtnClean,
    BtnBack
} BtnId;

typedef struct {
    int16_t x, y, w, h;
    BtnId id;
} Btn;

typedef enum { ScrMain, ScrMore } Screen;

#define GX(c) (49 + (c) * 26)
#define GY(r) ((r) * 22)

static const Btn main_btns[] = {
    {0, 0, 46, 14, BtnTempUp},
    {0, 50, 46, 14, BtnTempDown},
    {GX(0), GY(0), 25, 20, BtnPower},
    {GX(1), GY(0), 25, 20, BtnMode},
    {GX(2), GY(0), 25, 20, BtnFan},
    {GX(0), GY(1), 25, 20, BtnSwingV},
    {GX(1), GY(1), 25, 20, BtnSwingH},
    {GX(2), GY(1), 25, 20, BtnTurbo},
    {GX(0), GY(2), 25, 20, BtnEco},
    {GX(1), GY(2), 25, 20, BtnSleep},
    {GX(2), GY(2), 25, 20, BtnMore},
};

static const Btn more_btns[] = {
    {0, 0, 40, 20, BtnHealth},
    {44, 0, 40, 20, BtnClean},
    {88, 0, 40, 20, BtnBack},
};

typedef struct {
    AcState st;
    Screen screen;
    int focus; /* indice nell'array dei pulsanti della schermata */
    FuriMutex* mutex;
    FuriMessageQueue* queue;
} App;

static const Btn* screen_btns(Screen sc, int* n) {
    if(sc == ScrMore) {
        *n = ARR_LEN(more_btns);
        return more_btns;
    }
    *n = ARR_LEN(main_btns);
    return main_btns;
}

static int find_btn(const Btn* bt, int n, BtnId id) {
    for(int i = 0; i < n; i++) {
        if(bt[i].id == id) return i;
    }
    return 0;
}

static bool btn_enabled(const AcState* s, BtnId id) {
    switch(id) {
    case BtnTempUp:
    case BtnTempDown: return temp_allowed(s);
    case BtnTurbo: return tq_allowed(s);
    case BtnEco: return eco_allowed(s);
    case BtnSleep: return sleep_allowed(s);
    case BtnClean: return clean_allowed(s);
    default: return true;
    }
}

static void btn_text(const AcState* s, BtnId id, char* out, size_t len) {
    if(!btn_enabled(s, id)) {
        snprintf(out, len, "--");
        return;
    }
    switch(id) {
    case BtnPower: snprintf(out, len, "%s", s->power ? "ON" : "OFF"); break;
    case BtnMode: snprintf(out, len, "%s", mode_name[s->mode]); break;
    case BtnFan: snprintf(out, len, "%s", fan_name[s->tq ? FanAuto : s->fan]); break;
    case BtnSwingV:
        if(s->swing_v == SWV_AUTO) snprintf(out, len, "Auto");
        else snprintf(out, len, "%u", s->swing_v);
        break;
    case BtnSwingH:
        if(s->swing_h == SWH_COUNT - 1) snprintf(out, len, "Auto");
        else snprintf(out, len, "%u", s->swing_h + 1);
        break;
    case BtnTurbo:
        snprintf(
            out, len, "%s", s->tq == TQ_TURBO ? "Turbo" : s->tq == TQ_QUIET ? "Quiet" : "Off");
        break;
    case BtnEco:
        if(s->eco) snprintf(out, len, "L%u", s->eco);
        else snprintf(out, len, "Off");
        break;
    case BtnSleep: snprintf(out, len, "%s", s->sleep ? "ON" : "OFF"); break;
    case BtnHealth: snprintf(out, len, "%s", s->health ? "ON" : "OFF"); break;
    case BtnMore: snprintf(out, len, "More"); break;
    case BtnClean: snprintf(out, len, "Send"); break;
    case BtnBack: snprintf(out, len, "Back"); break;
    default: out[0] = 0; break;
    }
}

/* ------------------------------------------------------------------ */
/* Icone (disegnate in codice, area ~ 11x9 centrata in cx,cy)          */
/* ------------------------------------------------------------------ */

static void draw_icon(Canvas* c, BtnId id, const AcState* s, int cx, int cy, Color fg, Color bg) {
    switch(id) {
    case BtnTempUp:
        for(int i = 0; i < 5; i++) canvas_draw_line(c, cx - i, cy - 2 + i, cx + i, cy - 2 + i);
        break;
    case BtnTempDown:
        for(int i = 0; i < 5; i++) canvas_draw_line(c, cx - i, cy + 2 - i, cx + i, cy + 2 - i);
        break;
    case BtnPower:
        canvas_draw_circle(c, cx, cy + 1, 3);
        canvas_set_color(c, bg);
        canvas_draw_box(c, cx - 1, cy - 2, 3, 2);
        canvas_set_color(c, fg);
        canvas_draw_line(c, cx, cy - 4, cx, cy);
        break;
    case BtnMode:
        switch(s->mode) {
        case ModeAuto:
            canvas_set_font(c, FontPrimary);
            canvas_draw_str_aligned(c, cx, cy + 4, AlignCenter, AlignBottom, "A");
            canvas_set_font(c, FontSecondary);
            break;
        case ModeCool: /* fiocco di neve */
            canvas_draw_line(c, cx - 5, cy, cx + 5, cy);
            canvas_draw_line(c, cx - 3, cy - 4, cx + 3, cy + 4);
            canvas_draw_line(c, cx - 3, cy + 4, cx + 3, cy - 4);
            break;
        case ModeHeat: /* sole */
            canvas_draw_disc(c, cx, cy, 2);
            canvas_draw_line(c, cx, cy - 4, cx, cy - 3);
            canvas_draw_line(c, cx, cy + 3, cx, cy + 4);
            canvas_draw_line(c, cx - 4, cy, cx - 3, cy);
            canvas_draw_line(c, cx + 3, cy, cx + 4, cy);
            canvas_draw_dot(c, cx - 3, cy - 3);
            canvas_draw_dot(c, cx + 3, cy - 3);
            canvas_draw_dot(c, cx - 3, cy + 3);
            canvas_draw_dot(c, cx + 3, cy + 3);
            break;
        case ModeDry: /* goccia */
            canvas_draw_line(c, cx, cy - 4, cx - 3, cy);
            canvas_draw_line(c, cx, cy - 4, cx + 3, cy);
            canvas_draw_circle(c, cx, cy + 1, 3);
            break;
        default: /* ventola */
            canvas_draw_circle(c, cx, cy, 4);
            canvas_draw_line(c, cx, cy, cx, cy - 4);
            canvas_draw_line(c, cx, cy, cx - 4, cy + 2);
            canvas_draw_line(c, cx, cy, cx + 4, cy + 2);
            break;
        }
        break;
    case BtnFan: { /* barre di velocita' */
        int level = s->tq ? 0 : (int)s->fan;
        for(int i = 0; i < 3; i++) {
            int h = 3 + 3 * i;
            int x = cx - 5 + i * 4;
            if(level > i) canvas_draw_box(c, x, cy + 4 - h + 1, 3, h);
            else canvas_draw_frame(c, x, cy + 4 - h + 1, 3, h);
        }
        break;
    }
    case BtnSwingV: /* doppia freccia verticale */
        canvas_draw_line(c, cx, cy - 4, cx, cy + 4);
        canvas_draw_line(c, cx - 2, cy - 2, cx, cy - 4);
        canvas_draw_line(c, cx + 2, cy - 2, cx, cy - 4);
        canvas_draw_line(c, cx - 2, cy + 2, cx, cy + 4);
        canvas_draw_line(c, cx + 2, cy + 2, cx, cy + 4);
        break;
    case BtnSwingH: /* doppia freccia orizzontale + indicatore posizione */
        canvas_draw_line(c, cx - 6, cy, cx + 6, cy);
        canvas_draw_line(c, cx - 4, cy - 2, cx - 6, cy);
        canvas_draw_line(c, cx - 4, cy + 2, cx - 6, cy);
        canvas_draw_line(c, cx + 4, cy - 2, cx + 6, cy);
        canvas_draw_line(c, cx + 4, cy + 2, cx + 6, cy);
        if(s->swing_h < SWH_COUNT - 1) canvas_draw_box(c, cx - 4 + 2 * s->swing_h, cy - 3, 2, 7);
        break;
    case BtnTurbo:
        if(s->tq == TQ_QUIET) { /* altoparlante barrato */
            canvas_draw_box(c, cx - 5, cy - 1, 2, 3);
            canvas_draw_line(c, cx - 3, cy - 1, cx, cy - 4);
            canvas_draw_line(c, cx - 3, cy + 1, cx, cy + 4);
            canvas_draw_line(c, cx, cy - 4, cx, cy + 4);
            canvas_draw_line(c, cx + 2, cy - 3, cx + 6, cy + 3);
        } else { /* fulmine */
            canvas_draw_line(c, cx + 2, cy - 4, cx - 3, cy + 1);
            canvas_draw_line(c, cx - 3, cy + 1, cx, cy + 1);
            canvas_draw_line(c, cx, cy + 1, cx - 2, cy + 4);
            canvas_draw_line(c, cx - 2, cy + 4, cx + 3, cy - 1);
            canvas_draw_line(c, cx + 3, cy - 1, cx, cy - 1);
            canvas_draw_line(c, cx, cy - 1, cx + 2, cy - 4);
        }
        break;
    case BtnEco: /* foglia con nervatura */
        canvas_draw_line(c, cx - 4, cy + 4, cx - 4, cy);
        canvas_draw_line(c, cx - 4, cy, cx - 1, cy - 3);
        canvas_draw_line(c, cx - 1, cy - 3, cx + 4, cy - 4);
        canvas_draw_line(c, cx + 4, cy - 4, cx + 3, cy + 1);
        canvas_draw_line(c, cx + 3, cy + 1, cx, cy + 4);
        canvas_draw_line(c, cx, cy + 4, cx - 4, cy + 4);
        canvas_draw_line(c, cx - 4, cy + 4, cx + 1, cy - 1);
        break;
    case BtnSleep: /* luna */
        canvas_draw_disc(c, cx, cy, 4);
        canvas_set_color(c, bg);
        canvas_draw_disc(c, cx + 2, cy - 2, 3);
        canvas_set_color(c, fg);
        break;
    case BtnHealth: /* croce: sottile = off, spessa = on */
        if(s->health) {
            canvas_draw_box(c, cx - 1, cy - 4, 3, 9);
            canvas_draw_box(c, cx - 4, cy - 1, 9, 3);
        } else {
            canvas_draw_line(c, cx, cy - 4, cx, cy + 4);
            canvas_draw_line(c, cx - 4, cy, cx + 4, cy);
        }
        break;
    case BtnClean: /* scopa */
        canvas_draw_line(c, cx + 4, cy - 4, cx - 1, cy + 1);
        canvas_draw_line(c, cx - 1, cy + 1, cx - 5, cy + 4);
        canvas_draw_line(c, cx - 1, cy + 1, cx + 1, cy + 4);
        canvas_draw_line(c, cx - 5, cy + 4, cx + 1, cy + 4);
        break;
    case BtnMore: /* tre puntini */
        canvas_draw_box(c, cx - 5, cy - 1, 2, 2);
        canvas_draw_box(c, cx - 1, cy - 1, 2, 2);
        canvas_draw_box(c, cx + 3, cy - 1, 2, 2);
        break;
    case BtnBack: /* freccia a sinistra */
        canvas_draw_line(c, cx - 5, cy, cx + 5, cy);
        canvas_draw_line(c, cx - 5, cy, cx - 2, cy - 3);
        canvas_draw_line(c, cx - 5, cy, cx - 2, cy + 3);
        break;
    }
}

static void draw_btn(Canvas* c, const Btn* b, bool focus, const AcState* s) {
    Color fg = focus ? ColorWhite : ColorBlack;
    Color bg = focus ? ColorBlack : ColorWhite;
    int cx = b->x + b->w / 2;

    canvas_set_color(c, ColorBlack);
    if(focus) canvas_draw_rbox(c, b->x, b->y, b->w, b->h, 2);
    else canvas_draw_rframe(c, b->x, b->y, b->w, b->h, 2);
    canvas_set_color(c, fg);
    canvas_set_font(c, FontSecondary);

    if(b->h < 18) { /* Temp+ / Temp-: solo icona */
        if(btn_enabled(s, b->id)) draw_icon(c, b->id, s, cx, b->y + b->h / 2, fg, bg);
        else canvas_draw_line(c, cx - 3, b->y + b->h / 2, cx + 3, b->y + b->h / 2);
    } else {
        char t[8];
        draw_icon(c, b->id, s, cx, b->y + 5, fg, bg);
        btn_text(s, b->id, t, sizeof(t));
        canvas_draw_str_aligned(c, cx, b->y + b->h - 3, AlignCenter, AlignBottom, t);
    }
    canvas_set_color(c, ColorBlack);
}

/* display temperatura (schermata principale) */
static void draw_display(Canvas* c, const AcState* s) {
    char buf[8];
    canvas_set_color(c, ColorBlack);
    if(!s->power) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 23, 36, AlignCenter, AlignBottom, "OFF");
        canvas_set_font(c, FontSecondary);
        return;
    }
    if(!temp_allowed(s)) { /* Fan: niente temperatura */
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 23, 36, AlignCenter, AlignBottom, "FAN");
        canvas_set_font(c, FontSecondary);
        return;
    }
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 23, 26, AlignCenter, AlignBottom, mode_name[s->mode]);
    canvas_set_font(c, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%u", s->temp);
    canvas_draw_str_aligned(c, 16, 45, AlignCenter, AlignBottom, buf);
    canvas_set_font(c, FontSecondary);
    canvas_draw_circle(c, 34, 33, 1);
    canvas_draw_str(c, 37, 41, "C");
}

/* riepilogo stato (schermata More) */
static void draw_info(Canvas* c, const AcState* s) {
    char fan[8], swv[8], swh[8], eco[8], l1[40], l2[40];
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 36, "Haier AC");
    uint16_t tw = canvas_string_width(c, "Haier AC");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2 + tw + 5, 36, "by Marcopiiit");

    btn_text(s, BtnFan, fan, sizeof(fan));
    btn_text(s, BtnSwingV, swv, sizeof(swv));
    btn_text(s, BtnSwingH, swh, sizeof(swh));
    btn_text(s, BtnEco, eco, sizeof(eco));
    if(temp_allowed(s))
        snprintf(
            l1,
            sizeof(l1),
            "%s %s %uC Fan:%s",
            s->power ? "ON" : "OFF",
            mode_name[s->mode],
            s->temp,
            fan);
    else snprintf(l1, sizeof(l1), "%s Fan  speed:%s", s->power ? "ON" : "OFF", fan);
    snprintf(l2, sizeof(l2), "V:%s H:%s Eco:%s", swv, swh, eco);
    canvas_draw_str(c, 2, 49, l1);
    canvas_draw_str(c, 2, 60, l2);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const AcState* s = &app->st;

    canvas_clear(canvas);
    int n;
    const Btn* bt = screen_btns(app->screen, &n);
    for(int i = 0; i < n; i++) draw_btn(canvas, &bt[i], i == app->focus, s);
    if(app->screen == ScrMain) draw_display(canvas, s);
    else draw_info(canvas, s);

    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* ev, void* ctx) {
    App* app = ctx;
    furi_message_queue_put(app->queue, ev, FuriWaitForever);
}

/* ------------------------------------------------------------------ */
/* Navigazione e azioni                                               */
/* ------------------------------------------------------------------ */

/* sposta la selezione al pulsante piu' vicino nella direzione richiesta;
 * se non ce ne sono, riparte dal lato opposto */
static int nav(const Btn* bt, int n, int cur, InputKey key) {
    int cx = bt[cur].x + bt[cur].w / 2;
    int cy = bt[cur].y + bt[cur].h / 2;
    int best = -1, best_sc = 0x7fffffff;
    int wrap = -1, wrap_sc = 0x7fffffff;

    for(int i = 0; i < n; i++) {
        if(i == cur) continue;
        int dx = bt[i].x + bt[i].w / 2 - cx;
        int dy = bt[i].y + bt[i].h / 2 - cy;
        int prim, cross;
        switch(key) {
        case InputKeyUp:
            prim = -dy;
            cross = abs(dx);
            break;
        case InputKeyDown:
            prim = dy;
            cross = abs(dx);
            break;
        case InputKeyLeft:
            prim = -dx;
            cross = abs(dy);
            break;
        default:
            prim = dx;
            cross = abs(dy);
            break;
        }
        if(prim > 4) {
            int sc = prim + 3 * cross;
            if(sc < best_sc) {
                best_sc = sc;
                best = i;
            }
        } else if(prim < -4) {
            int sc = cross * 10 + prim; /* prim < 0: piu' lontano = punteggio minore */
            if(sc < wrap_sc) {
                wrap_sc = sc;
                wrap = i;
            }
        }
    }
    if(best >= 0) return best;
    if(wrap >= 0) return wrap;
    return cur;
}

static void apply_mode_rules(AcState* s) {
    if(s->mode != ModeHeat && s->swing_v == 1) s->swing_v = 2; /* pos 1 solo in Heat */
    if(s->mode == ModeFan && s->fan == FanAuto) s->fan = FanLow; /* in Fan niente Auto */
    if(!sleep_allowed(s)) s->sleep = false;
    if(!tq_allowed(s)) s->tq = TQ_OFF;
    if(!eco_allowed(s)) s->eco = 0;
}

/* "preme" un tasto: aggiorna lo stato; ritorna true se va inviato il segnale */
static bool press(AcState* s, BtnId id, bool* clean) {
    switch(id) {
    case BtnPower:
        s->power = !s->power;
        s->last_key = KEY_POWER;
        return true;
    case BtnMode:
        s->mode = mode_next[s->mode];
        apply_mode_rules(s);
        s->last_key = KEY_MODE;
        return true;
    case BtnTempUp:
        if(!temp_allowed(s) || s->temp >= 30) return false;
        s->temp++;
        s->last_key = KEY_TEMP_UP;
        return true;
    case BtnTempDown:
        if(!temp_allowed(s) || s->temp <= 16) return false;
        s->temp--;
        s->last_key = KEY_TEMP_DOWN;
        return true;
    case BtnFan:
        if(s->tq != TQ_OFF) {
            /* FAN disattiva Turbo/Quiet e riporta la ventola ad Auto (come da cattura) */
            s->tq = TQ_OFF;
            s->fan = (s->mode == ModeFan) ? FanLow : FanAuto;
        } else if(s->mode == ModeFan) {
            s->fan = (AcFan)(FanLow + ((s->fan - FanLow + 1) % 3)); /* Low/Med/High */
        } else {
            s->fan = (AcFan)((s->fan + 1) % FanCount);
        }
        s->last_key = KEY_FAN;
        return true;
    case BtnSwingV: {
        int lo = (s->mode == ModeHeat) ? 1 : 2; /* pos 1 solo in Heat */
        int n = SWV_AUTO - lo + 1; /* lo..5 + auto */
        int idx = (s->swing_v - lo + 1) % n;
        s->swing_v = (uint8_t)(lo + idx);
        s->last_key = KEY_SWING;
        return true;
    }
    case BtnSwingH:
        s->swing_h = (s->swing_h + 1) % SWH_COUNT;
        s->last_key = KEY_SWING_H;
        return true;
    case BtnTurbo: {
        if(!tq_allowed(s)) return false;
        uint8_t prev = s->tq;
        s->tq = (s->tq + 1) % 3; /* Off -> Turbo -> Quiet -> Off */
        if(prev == TQ_OFF && s->tq == TQ_TURBO) s->swing_v = 3; /* primo clic Turbo */
        s->last_key = KEY_TURBO;
        return true;
    }
    case BtnEco:
        if(!eco_allowed(s)) return false;
        s->eco = (s->eco + 1) % 4; /* Off -> L1 -> L2 -> L3 -> Off */
        s->last_key = KEY_ECO;
        return true;
    case BtnSleep:
        if(!sleep_allowed(s)) return false;
        s->sleep = !s->sleep;
        s->last_key = KEY_SLEEP;
        return true;
    case BtnHealth:
        s->health = !s->health;
        s->last_key = KEY_HEALTH;
        return true;
    case BtnClean:
        if(!clean_allowed(s)) return false;
        *clean = true;
        return true;
    default: return false;
    }
}

int32_t haier_ac_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    app->st = (AcState){
        .power = true,
        .mode = ModeCool,
        .temp = 24,
        .fan = FanAuto,
        .swing_v = 2,
        .swing_h = SWH_CENTER,
        .health = false,
        .sleep = false,
        .tq = TQ_OFF,
        .eco = 0,
        .last_key = KEY_POWER};
    app->screen = ScrMain;
    app->focus = find_btn(main_btns, ARR_LEN(main_btns), BtnPower);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, app);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->queue, &ev, FuriWaitForever) != FuriStatusOk) continue;

        bool is_dir = ev.key == InputKeyUp || ev.key == InputKeyDown || ev.key == InputKeyLeft ||
                      ev.key == InputKeyRight;
        if(ev.type != InputTypeShort && !(is_dir && ev.type == InputTypeRepeat)) continue;

        bool tx = false;
        bool clean = false;
        AcState snap;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        int n;
        const Btn* bt = screen_btns(app->screen, &n);
        switch(ev.key) {
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
        case InputKeyRight: app->focus = nav(bt, n, app->focus, ev.key); break;
        case InputKeyOk: {
            BtnId id = bt[app->focus].id;
            if(id == BtnMore) {
                app->screen = ScrMore;
                app->focus = 0;
            } else if(id == BtnBack) {
                app->screen = ScrMain;
                app->focus = find_btn(main_btns, ARR_LEN(main_btns), BtnMore);
            } else {
                tx = press(&app->st, id, &clean);
                /* da spento solo Power invia: gli altri tasti cambiano lo stato in silenzio */
                if(!app->st.power && id != BtnPower) tx = false;
            }
            break;
        }
        case InputKeyBack:
            if(app->screen == ScrMore) {
                app->screen = ScrMain;
                app->focus = find_btn(main_btns, ARR_LEN(main_btns), BtnMore);
            } else {
                running = false;
            }
            break;
        default: break;
        }
        snap = app->st;
        furi_mutex_release(app->mutex);

        view_port_update(vp); /* l'interfaccia si aggiorna subito, poi parte l'IR */
        if(tx) send_state(&snap, clean);
    }

    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
