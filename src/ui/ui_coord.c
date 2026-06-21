#include "ui_i.h"

#include <gui/view.h>

#include <stdio.h>
#include <string.h>

#define COORD_K_BACK 12
#define COORD_K_ENTER 13
#define COORD_KEY_W 12
#define COORD_KEYS_X 6
#define COORD_KEYS_Y 15
#define COORD_FRAC_N 5

typedef struct
{
    FlipperHamApp *app;
} CoordModel;

static bool coord_hide_zero;
static char coord_old[POS_LEN];

static void coord_draw(Canvas *canvas, void *model);
static bool coord_input(InputEvent *event, void *context);
static void coord_button(Canvas *canvas, uint8_t k, const char *s, int8_t sel);
static char *coord_buf(FlipperHamApp *app);
static void coord_cancel(FlipperHamApp *app);
static void coord_short(char *s);
static bool coord_zero_like(const char *s);
static uint8_t coord_int_digits(const char *s);
static uint8_t coord_frac_digits(const char *s);
static bool coord_digit_ok(FlipperHamApp *app, char c);
static bool coord_key_ok(FlipperHamApp *app, char c);
static void coord_disabled(Canvas *canvas, uint8_t x, uint8_t y, uint8_t w, uint8_t h);
static bool coord_toggle_sign(FlipperHamApp *app);
static bool coord_add(FlipperHamApp *app, char c);
static bool coord_do_key(FlipperHamApp *app);

static char *coord_buf(FlipperHamApp *app)
{
    if (app->text_mode == 8)
        return app->p_lon_edit;


    return app->p_lat_edit;
}

static void coord_cancel(FlipperHamApp *app)
{
    char *s = coord_buf(app);

    snprintf(s, POS_LEN, "%s", coord_old);
    coord_hide_zero = coord_zero_like(s);
}

static void coord_short(char *s)
{
    char *p;
    uint8_t l;
    uint8_t d;

    p = strchr(s, '.');
    if (!p) return;

    l = strlen(s);
    d = p - s;
    while (l > d && s[l - 1] == '0')
    {
        l--;
        s[l] = 0;
    }
    if (l && s[l - 1] == '.') s[l - 1] = 0;
}

static void coord_button(Canvas *canvas, uint8_t k, const char *s, int8_t sel)
{
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
    bool ok = true;

    if (k < 12)
    {
        x = COORD_KEYS_X + (k % 3) * COORD_KEY_W;
        y = COORD_KEYS_Y + (k / 3) * COORD_KEY_W;
        w = COORD_KEY_W;
        h = COORD_KEY_W;
        ok = coord_key_ok(gapp, s[0]);
    }
    else if (k == COORD_K_BACK)
    {
        x = 84;
        y = 22;
        w = 36;
        h = 11;
    }
    else
    {
        x = 87;
        y = 38;
        w = 34;
        h = 21;
    }

    if (sel)
    {
        canvas_draw_box(canvas, x + 1, y + 1, w - 1, h - 1);
        canvas_set_color(canvas, ColorWhite);
    }
    else if (k >= 12)
        canvas_draw_frame(canvas, x, y, w, h);

    canvas_set_color(canvas, sel ? ColorWhite : ColorBlack);
    canvas_draw_str_aligned(canvas, x + w / 2 + (k < 12 ? 1 : 0), y + h / 2 + (k < 12 ? 1 : 0), AlignCenter, AlignCenter, s);
    if (!ok) coord_disabled(canvas, x, y, w, h);

    if (sel) canvas_set_color(canvas, ColorBlack);
}

static void coord_keypad_frame(Canvas *canvas)
{
    uint8_t i;

    for (i = 0; i <= 3; i++)
        canvas_draw_line(canvas, COORD_KEYS_X + i * COORD_KEY_W, COORD_KEYS_Y, COORD_KEYS_X + i * COORD_KEY_W, COORD_KEYS_Y + 4 * COORD_KEY_W);
    for (i = 0; i <= 4; i++)
        canvas_draw_line(canvas, COORD_KEYS_X, COORD_KEYS_Y + i * COORD_KEY_W, COORD_KEYS_X + 3 * COORD_KEY_W, COORD_KEYS_Y + i * COORD_KEY_W);
}

static void coord_back_shape(Canvas *canvas, uint8_t sel)
{
    uint8_t x = 84, y = 22;
    uint8_t i;

    if (sel)
    {
        canvas_draw_box(canvas, x + 5, y, 33, 11);
        for (i = 0; i <= 5; i++)
        {
            canvas_draw_line(canvas, x + i, y + 5 - i, x + 5, y + 5 - i);
            canvas_draw_line(canvas, x + i, y + 5 + i, x + 5, y + 5 + i);
        }
        canvas_set_color(canvas, ColorWhite);
    }

    canvas_draw_line(canvas, x + 5, y, x + 37, y);
    canvas_draw_line(canvas, x + 37, y, x + 37, y + 10);
    canvas_draw_line(canvas, x + 37, y + 10, x + 5, y + 10);
    canvas_draw_line(canvas, x, y + 5, x + 5, y);
    canvas_draw_line(canvas, x, y + 5, x + 5, y + 10);
    canvas_draw_str_aligned(canvas, x + 23, y + 6, AlignCenter, AlignCenter, "bksp");

    if (sel)
    {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_dot(canvas, x, y + 5);
        canvas_draw_line(canvas, x + 5, y, x + 37, y);
        canvas_draw_line(canvas, x + 37, y, x + 37, y + 10);
        canvas_draw_line(canvas, x + 37, y + 10, x + 5, y + 10);
        canvas_draw_line(canvas, x, y + 5, x + 5, y);
        canvas_draw_line(canvas, x, y + 5, x + 5, y + 10);
    }
}

static void coord_enter_shape(Canvas *canvas, uint8_t sel)
{
    uint8_t x = 87, y = 38;

    if (sel)
    {
        canvas_draw_box(canvas, x + 8, y, 27, 21);
        canvas_draw_box(canvas, x, y + 9, 35, 12);
        canvas_set_color(canvas, ColorWhite);
    }
    else
    {
        canvas_draw_line(canvas, x + 8, y, x + 34, y);
        canvas_draw_line(canvas, x + 34, y, x + 34, y + 20);
        canvas_draw_line(canvas, x + 34, y + 20, x, y + 20);
        canvas_draw_line(canvas, x, y + 20, x, y + 9);
        canvas_draw_line(canvas, x, y + 9, x + 8, y + 9);
        canvas_draw_line(canvas, x + 8, y + 9, x + 8, y);
    }

    canvas_draw_line(canvas, x + 22, y + 7, x + 22, y + 14);
    canvas_draw_line(canvas, x + 14, y + 14, x + 22, y + 14);
    canvas_draw_line(canvas, x + 14, y + 14, x + 18, y + 11);
    canvas_draw_line(canvas, x + 14, y + 14, x + 18, y + 17);

    if (sel) canvas_set_color(canvas, ColorBlack);
}

static void coord_draw(Canvas *canvas, void *model)
{
    CoordModel *m = model;
    FlipperHamApp *app;
    char *s;
    const char *show;
    const char *label;
    char h[2];
    static const char k[] = "123456789-0.";
    uint8_t i;

    if (!m) return;
    app = m->app;
    if (!app) return;
    s = coord_buf(app);
    show = coord_hide_zero ? "" : s;
    label = app->text_mode == 8 ? "Longitude:" : "Latitude:";

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 10, label);
    canvas_draw_str(canvas, 58, 10, show);
    if (show[0])
    {
        h[0] = app->text_mode == 8 ? (s[0] == '-' ? 'W' : 'E') : (s[0] == '-' ? 'S' : 'N');
        h[1] = 0;
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, h);
    }

    canvas_set_font(canvas, FontKeyboard);
    coord_keypad_frame(canvas);
    for (i = 0; i < 12; i++)
    {
        char b[2];
        b[0] = k[i];
        b[1] = 0;
        coord_button(canvas, i, b, app->coord_key == i);
    }
    coord_back_shape(canvas, app->coord_key == COORD_K_BACK);
    coord_enter_shape(canvas, app->coord_key == COORD_K_ENTER);

}

static bool coord_zero_like(const char *s)
{
    uint8_t i = 0;
    uint8_t d = 0;

    if (!s || !s[0]) return false;
    if (s[i] == '-') i++;
    while (s[i] >= '0' && s[i] <= '9')
    {
        if (s[i] != '0') return false;
        d++;
        i++;
    }
    if (!d) return false;
    if (s[i] == '.') i++;
    while (s[i])
    {
        if (s[i] != '0') return false;
        i++;
    }


    return true;
}

static uint8_t coord_int_digits(const char *s)
{
    uint8_t i = 0;
    uint8_t n = 0;

    if (s[0] == '-') i = 1;
    while (s[i] && s[i] != '.')
    {
        if (s[i] >= '0' && s[i] <= '9') n++;
        i++;
    }


    return n;
}

static uint8_t coord_frac_digits(const char *s)
{
    uint8_t i = 0;
    uint8_t n = 0;

    while (s[i] && s[i] != '.')
        i++;
    if (s[i] == '.') i++;
    while (s[i])
    {
        if (s[i] >= '0' && s[i] <= '9') n++;
        i++;
    }


    return n;
}

static int32_t coord_int_part(const char *s)
{
    uint8_t i = 0;
    int32_t n = 0;

    if (s[0] == '-') i = 1;
    while (s[i] && s[i] != '.')
    {
        if (s[i] < '0' || s[i] > '9') break;
        n = n * 10 + s[i] - '0';
        i++;
    }


    return n;
}

static bool coord_digit_ok(FlipperHamApp *app, char c)
{
    char *s = coord_buf(app);
    uint8_t i;
    int32_t n;
    int32_t max;

    for (i = 0; s[i]; i++)
        if (s[i] == '.')
            return coord_frac_digits(s) < COORD_FRAC_N;

    if (coord_int_digits(s) >= 3) return false;

    n = coord_int_part(s) * 10 + c - '0';
    max = app->text_mode == 8 ? 180 : 90;
    if (n > max) return false;


    return true;
}

static bool coord_key_ok(FlipperHamApp *app, char c)
{
    char *s;
    uint8_t i;
    uint8_t n;

    if (!app) return true;
    if (coord_hide_zero) return true;
    s = coord_buf(app);
    n = strlen(s);

    if (c == '-')
    {
        if (s[0] == '-') return true;
        return n + 1 < POS_LEN;
    }

    if (n + 1 >= POS_LEN) return false;
    if (c == '.')
    {
        for (i = 0; s[i]; i++)
            if (s[i] == '.') return false;
        return true;
    }
    if (c < '0' || c > '9') return false;


    return coord_digit_ok(app, c);
}

static void coord_disabled(Canvas *canvas, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t i;
    uint8_t j;

    canvas_set_color(canvas, ColorWhite);
    for (j = y + 1; j < y + h; j++)
        for (i = x + 1; i < x + w; i++)
            if ((i + j) & 1) canvas_draw_dot(canvas, i, j);


    canvas_set_color(canvas, ColorBlack);
}

static bool coord_toggle_sign(FlipperHamApp *app)
{
    char *s = coord_buf(app);
    uint8_t n = strlen(s);

    if (s[0] == '-')
    {
        memmove(s, s + 1, n);
        return true;
    }

    if (n + 1 >= POS_LEN) return false;
    memmove(s + 1, s, n + 1);
    s[0] = '-';


    return true;
}

static bool coord_add(FlipperHamApp *app, char c)
{
    char *s = coord_buf(app);
    uint8_t n;
    uint8_t i;

    if (coord_hide_zero)
    {
        s[0] = 0;
        coord_hide_zero = false;
    }

    if (c == '-')
    {
        return coord_toggle_sign(app);
    }

    n = strlen(s);
    if (n + 1 >= POS_LEN) return false;

    if (c == '.')
    {
        for (i = 0; s[i]; i++)
            if (s[i] == '.')
                return false;
        if (!n)
        {
            snprintf(s, POS_LEN, "0.");
            return true;
        }
        if (n == 1 && s[0] == '-')
        {
            snprintf(s, POS_LEN, "-0.");
            return true;
        }
        s[n] = '.';
        s[n + 1] = 0;
        return true;
    }

    if (c < '0' || c > '9') return false;
    if (!coord_digit_ok(app, c)) return false;
    s[n] = c;
    s[n + 1] = 0;


    return true;
}

static void coord_move(FlipperHamApp *app, InputKey key)
{
    uint8_t k = app->coord_key;

    if (key == InputKeyLeft)
    {
        if (k == COORD_K_BACK || k == COORD_K_ENTER) k = 2;
        else if (k % 3) k--;
        app->coord_key = k;
        return;
    }
    if (key == InputKeyRight)
    {
        if (k == 2 || k == 5) k = COORD_K_BACK;
        else if (k == 8 || k == 11) k = COORD_K_ENTER;
        else if (k < 12 && (k % 3) < 2) k++;
        app->coord_key = k;
        return;
    }
    if (key == InputKeyUp)
    {
        if (k == COORD_K_ENTER) k = COORD_K_BACK;
        else if (k >= 3 && k < 12) k -= 3;
        app->coord_key = k;
        return;
    }
    if (key == InputKeyDown)
    {
        if (k == COORD_K_BACK) k = COORD_K_ENTER;
        else if (k < 9) k += 3;
        app->coord_key = k;
    }
}

static bool coord_do_key(FlipperHamApp *app)
{
    char *s = coord_buf(app);
    char a[POS_LEN];
    uint8_t n;
    static const char k[] = "123456789-0.";

    if (app->coord_key < 12)
    {
        coord_add(app, k[app->coord_key]);
        return true;
    }

    if (app->coord_key == COORD_K_BACK)
    {
        if (coord_hide_zero)
        {
            coord_hide_zero = false;
            s[0] = 0;
            return true;
        }
        n = strlen(s);
        if (n) s[n - 1] = 0;
        return true;
    }

    if (!s[0] || aprs_ll_clamp(a, sizeof(a), s, app->text_mode == 8))
    {
        position_save(app);
        return false;
    }


    return true;
}

static bool coord_input(InputEvent *event, void *context)
{
    FlipperHamApp *app = context;
    CoordModel *m;
    bool redraw = true;

    if (!event || !app) return false;
    if (event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if (event->key == InputKeyBack)
    {
        coord_cancel(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, app->text_view);
        return true;
    }

    if (event->key == InputKeyOk)
        redraw = coord_do_key(app);
    else
        coord_move(app, event->key);

    if (redraw)
    {
        m = view_get_model(app->coord_input_view);
        UNUSED(m);
        view_commit_model(app->coord_input_view, true);
    }

    return true;
}

void coord_input_start(FlipperHamApp *app, uint8_t lon)
{
    app->text_mode = lon ? 8 : 7;
    app->coord_key = 0;
    snprintf(coord_old, sizeof(coord_old), "%s", coord_buf(app));
    coord_short(coord_buf(app));
    coord_hide_zero = coord_zero_like(coord_buf(app));
}

void coord_input_alloc(FlipperHamApp *app)
{
    CoordModel *m;

    app->coord_input_view = view_alloc();
    view_set_context(app->coord_input_view, app);
    view_allocate_model(app->coord_input_view, ViewModelTypeLocking, sizeof(CoordModel));
    m = view_get_model(app->coord_input_view);
    m->app = app;
    view_commit_model(app->coord_input_view, false);
    view_set_draw_callback(app->coord_input_view, coord_draw);
    view_set_input_callback(app->coord_input_view, coord_input);
}

void coord_input_free(FlipperHamApp *app)
{
    if (!app->coord_input_view) return;
    view_free(app->coord_input_view);
    app->coord_input_view = NULL;
}
