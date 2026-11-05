#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

// Entrada por casillas: arriba/abajo cambia el caracter, izquierda/derecha mueve el cursor,
// OK confirma. Sirve para el CAN del DNIe (6 cifras) y para los datos del pasaporte:
// numero (9 caracteres A-Z/0-9, '<' = vacio) y fechas DDMMAA.

#define FIELD_MAX 9

typedef struct {
    char chars[FIELD_MAX + 1];
    uint8_t len;
    uint8_t cursor;
    bool alnum;
    const char* title;
    const char* hint;
} InputModel;

static const char ALNUM[] = "<0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static char step_char(char c, bool alnum, int dir) {
    if(!alnum) {
        if(dir > 0) return c == '9' ? '0' : c + 1;
        return c == '0' ? '9' : c - 1;
    }
    const char* p = strchr(ALNUM, c);
    int i = p ? (int)(p - ALNUM) : 0;
    int n = sizeof(ALNUM) - 1;
    return ALNUM[(i + dir + n) % n];
}

static void input_draw(Canvas* canvas, void* model) {
    InputModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, m->title);

    // 6 casillas grandes o 9 pequenas
    bool big = m->len <= 6;
    int bw = big ? 17 : 12, bh = big ? 22 : 18, gap = big ? 3 : 2, by = 16;
    int total = m->len * bw + (m->len - 1) * gap;
    int x0 = (128 - total) / 2;
    canvas_set_font(canvas, big ? FontBigNumbers : FontPrimary);
    for(int i = 0; i < m->len; i++) {
        int x = x0 + i * (bw + gap);
        char d[2] = {m->chars[i] == '<' ? ' ' : m->chars[i], '\0'};
        if(i == m->cursor) {
            canvas_draw_rbox(canvas, x, by, bw, bh, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, x, by, bw, bh, 2);
        }
        canvas_draw_str_aligned(canvas, x + bw / 2, by + bh / 2 + 1, AlignCenter, AlignCenter, d);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, m->hint);
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Up/down: change, OK");
}

static bool input_input(InputEvent* event, void* context) {
    DnieApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = true;
    bool done = false;
    with_view_model(
        app->can_view,
        InputModel * m,
        {
            switch(event->key) {
            case InputKeyUp:
                m->chars[m->cursor] = step_char(m->chars[m->cursor], m->alnum, +1);
                break;
            case InputKeyDown:
                m->chars[m->cursor] = step_char(m->chars[m->cursor], m->alnum, -1);
                break;
            case InputKeyLeft:
                if(m->cursor > 0) m->cursor--;
                break;
            case InputKeyRight:
                if(m->cursor < m->len - 1) m->cursor++;
                break;
            case InputKeyOk:
                done = event->type == InputTypeShort;
                break;
            default:
                consumed = false; // Atras: lo gestiona el SceneManager
                break;
            }
            if(done) {
                char v[FIELD_MAX + 1];
                memcpy(v, m->chars, m->len);
                v[m->len] = '\0';
                switch(app->input_field) {
                case DnieInputCan:
                    strlcpy(app->can, v, sizeof(app->can));
                    break;
                case DnieInputPassNumber: {
                    // Sin los huecos del final
                    size_t n = m->len;
                    while(n > 0 && v[n - 1] == '<')
                        n--;
                    v[n] = '\0';
                    strlcpy(app->pass_number, v, sizeof(app->pass_number));
                    break;
                }
                default: {
                    // DDMMAA en pantalla -> AAMMDD como en la MRZ
                    char* dst = app->input_field == DnieInputPassBirth ? app->pass_birth :
                                                                         app->pass_expiry;
                    dst[0] = v[4];
                    dst[1] = v[5];
                    dst[2] = v[2];
                    dst[3] = v[3];
                    dst[4] = v[0];
                    dst[5] = v[1];
                    dst[6] = '\0';
                    break;
                }
                }
            }
        },
        true);
    if(done) view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventCanEntered);
    return consumed;
}

View* dnie_can_view_alloc(DnieApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(InputModel));
    view_set_context(view, app);
    view_set_draw_callback(view, input_draw);
    view_set_input_callback(view, input_input);
    return view;
}

static void fill_model(DnieApp* app, InputModel* m) {
    memset(m->chars, 0, sizeof(m->chars));
    m->cursor = 0;
    m->alnum = false;
    switch(app->input_field) {
    case DnieInputCan:
        m->len = DNIE_CAN_LENGTH;
        m->title = app->foreign_id ? "Document CAN" : "ID CAN";
        m->hint = app->foreign_id ? "6 printed digits" : "6 digits on front";
        for(int i = 0; i < m->len; i++)
            m->chars[i] = strlen(app->can) == DNIE_CAN_LENGTH ? app->can[i] : '0';
        break;
    case DnieInputPassNumber: {
        m->len = 9;
        m->alnum = true;
        m->title = app->foreign_id ? "Document No." : "Passport No.";
        m->hint = app->foreign_id ? "As in the MRZ" : "E.g.: PAA123456";
        size_t n = strlen(app->pass_number);
        for(int i = 0; i < m->len; i++)
            m->chars[i] = i < (int)n ? app->pass_number[i] : '<';
        if(n == 0) memcpy(m->chars, "PAA000000", 9);
        break;
    }
    default: {
        bool birth = app->input_field == DnieInputPassBirth;
        const char* src = birth ? app->pass_birth : app->pass_expiry;
        m->len = 6;
        m->title = birth ? "Birth date" : "Expiry";
        m->hint = "DD MM AA";
        if(strlen(src) == 6) {
            // AAMMDD guardado -> DDMMAA en pantalla
            const char s[6] = {src[4], src[5], src[2], src[3], src[0], src[1]};
            memcpy(m->chars, s, 6);
        } else {
            memcpy(m->chars, "010100", 6);
        }
        break;
    }
    }
}

static void load_field(DnieApp* app) {
    with_view_model(app->can_view, InputModel * m, { fill_model(app, m); }, true);
}

void dnie_scene_can_input_on_enter(void* context) {
    DnieApp* app = context;
    app->input_field = app->passport ? DnieInputPassNumber : DnieInputCan;
    load_field(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewCan);
}

bool dnie_scene_can_input_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == DnieCustomEventCanEntered) {
        if(app->passport && app->input_field != DnieInputPassExpiry) {
            app->input_field++; // siguiente dato del pasaporte
            load_field(app);
        } else {
            scene_manager_next_scene(app->scene_manager, DnieSceneRead);
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && app->passport &&
       app->input_field > DnieInputPassNumber) {
        app->input_field--; // Atras vuelve al dato anterior
        load_field(app);
        return true;
    }
    return false;
}

void dnie_scene_can_input_on_exit(void* context) {
    UNUSED(context);
}
