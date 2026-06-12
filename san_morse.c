#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

#include "i18n.h"
#include "morse.h"
#include "player.h"
#include "settings.h"

#define TREE_TEXT_MAX 64
#define INPUT_TEXT_MAX 128
#define TREE_TICK_MS 50

typedef enum {
    TreeGapNone, // sin cuenta regresiva activa
    TreeGapLetter, // esperando confirmar letra
    TreeGapWord, // letra confirmada, esperando espacio
} TreeGapPhase;

typedef enum {
    ViewIdMenu,
    ViewIdTree,
    ViewIdTextInput,
    ViewIdPlay,
    ViewIdSettings,
    ViewIdAbout,
} ViewId;

typedef enum {
    MenuIndexTree,
    MenuIndexText,
    MenuIndexSettings,
    MenuIndexAbout,
} MenuIndex;

typedef enum {
    SetItemLang,
    SetItemSound,
    SetItemVolume,
    SetItemTone,
    SetItemVibro,
    SetItemLed,
    SetItemWpm,
    SetItemDit,
    SetItemLetterGap,
    SetItemWordGap,
    SetItemCount,
} SetItem;

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* settings_list;
    VariableItem* set_items[SetItemCount];
    Widget* about;
    View* tree_view;
    View* play_view;
    MorsePlayer* player;
    FuriTimer* tree_timer;
    bool tree_tone; // altavoz adquirido mientras OK esta presionado
    MorseSettings settings;
    uint8_t settings_list_lang; // idioma con el que se construyo la lista

    char input_text[INPUT_TEXT_MAX];
    char tree_text[TREE_TEXT_MAX];
    const char* play_text;
    uint32_t play_return_view;
} SanMorseApp;

typedef struct {
    char* buffer; // apunta a app->tree_text
    const MorseSettings* settings; // apunta a app->settings
    size_t len;
    uint8_t cur; // indice del nodo actual en el arbol
    bool key_down;
    uint32_t key_down_tick;
    uint32_t last_release_tick;
    TreeGapPhase gap_phase;
} TreeModel;

typedef struct {
    const char* text;
    const I18nStrings* tr;
    MorsePlayerState state;
    size_t pos;
    int8_t sym;
    uint32_t wpm;
    bool sound;
    bool vibro;
} PlayModel;

static const I18nStrings* tr_app(SanMorseApp* app) {
    return i18n_get(&app->settings);
}

// ---------------------------------------------------------------- navegacion

static uint32_t nav_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t nav_menu_callback(void* context) {
    UNUSED(context);
    return ViewIdMenu;
}

static uint32_t nav_play_return_callback(void* context) {
    SanMorseApp* app = context;
    return app->play_return_view;
}

// ------------------------------------------------------------- vista: arbol

static uint8_t tree_node_level(uint8_t idx) {
    uint8_t level = 0;
    while(idx >= ((1u << (level + 1)) - 1)) level++;
    return level;
}

// Linea base de texto por nivel (el nivel 0 es virtual, se dibuja como punto).
static const uint8_t tree_level_y[5] = {12, 22, 35, 48, 61};

static void tree_draw_callback(Canvas* canvas, void* model) {
    TreeModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    uint32_t now = furi_get_tick();

    // Barra de estado: texto escrito (izq) + secuencia actual (der)
    if(m->len == 0 && m->cur == 0 && !m->key_down) {
        canvas_draw_str(canvas, 0, 8, i18n_get(m->settings)->hint_key);
    } else {
        const char* tail = m->buffer;
        while(*tail && canvas_string_width(canvas, tail) > 88) tail++;
        canvas_draw_str(canvas, 0, 8, tail);
        uint16_t w = canvas_string_width(canvas, tail);
        canvas_draw_line(canvas, w + 1, 9, w + 4, 9); // cursor

        char seq[8];
        uint8_t sl = 0;
        for(uint8_t i = m->cur; i > 0; i = (i - 1) / 2) {
            seq[sl++] = (i & 1) ? '.' : '-';
        }
        char seq_fwd[8];
        for(uint8_t i = 0; i < sl; i++) seq_fwd[i] = seq[sl - 1 - i];
        if(m->key_down) {
            // simbolo en curso segun cuanto lleva presionado
            bool dah = (now - m->key_down_tick) >= furi_ms_to_ticks(m->settings->dit_ms);
            seq_fwd[sl++] = dah ? '-' : '.';
        }
        seq_fwd[sl] = 0;
        if(sl) canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, seq_fwd);
    }

    // Cuenta regresiva: linea continua = confirmar letra, punteada = espacio
    if(m->gap_phase == TreeGapLetter && m->settings->letter_gap_ms) {
        uint32_t total = furi_ms_to_ticks(m->settings->letter_gap_ms);
        uint32_t elapsed = now - m->last_release_tick;
        if(elapsed < total) {
            uint8_t w = (uint8_t)(128u - (elapsed * 128u) / total);
            canvas_draw_line(canvas, 0, 10, w, 10);
        }
    } else if(m->gap_phase == TreeGapWord && m->settings->word_gap_ms) {
        uint32_t total = furi_ms_to_ticks(m->settings->word_gap_ms);
        uint32_t elapsed = now - m->last_release_tick;
        if(elapsed < total) {
            uint8_t w = (uint8_t)(128u - (elapsed * 128u) / total);
            for(uint8_t x = 0; x <= w; x += 3) {
                canvas_draw_dot(canvas, x, 10);
            }
        }
    }

    // Ventana del arbol: vista completa (niveles 1-4) o, al llegar al
    // nivel 4, zoom al subarbol del ancestro dos niveles arriba para
    // mostrar el nivel 5 (numeros y signos).
    uint8_t depth = tree_node_level(m->cur);
    uint8_t anchor = 0;
    uint8_t anchor_depth = 0;
    uint8_t nrows = 5;
    static const uint8_t y_zoom[4] = {19, 33, 47, 61};
    const uint8_t* ys = tree_level_y;
    if(depth >= 4) {
        anchor = (((m->cur - 1) / 2) - 1) / 2;
        anchor_depth = depth - 2;
        nrows = 4;
        ys = y_zoom;
    }

    for(uint8_t k = 0; k < nrows; k++) {
        uint16_t first = (((uint16_t)anchor + 1) << k) - 1;
        uint16_t count = 1u << k;
        if(first >= MORSE_TREE_NODES) break;
        for(uint16_t j = 0; j < count; j++) {
            uint8_t idx = (uint8_t)(first + j);
            uint8_t x = (uint8_t)(((2u * j + 1) * 128u) >> (k + 1));
            uint8_t y = ys[k];
            if(idx == 0) {
                canvas_draw_disc(canvas, x, y, 2); // raiz (inicio)
                continue;
            }
            char letter = morse_tree_letters[idx];
            if(idx == m->cur) {
                canvas_draw_rbox(canvas, x - 4, y - 9, 9, 11, 1);
                if(letter) {
                    char s[2] = {letter, 0};
                    canvas_set_color(canvas, ColorWhite);
                    canvas_draw_str_aligned(canvas, x, y, AlignCenter, AlignBottom, s);
                    canvas_set_color(canvas, ColorBlack);
                }
            } else if(letter) {
                char s[2] = {letter, 0};
                canvas_draw_str_aligned(canvas, x, y, AlignCenter, AlignBottom, s);
            } else {
                canvas_draw_dot(canvas, x, y - 3);
            }
        }
    }

    // Hijos del nodo actual: la decision punto/raya
    uint8_t kc = depth - anchor_depth; // fila del nodo actual
    if(kc + 1 < nrows && m->cur * 2 + 2 < MORSE_TREE_NODES) {
        uint16_t first_c = (((uint16_t)anchor + 1) << kc) - 1;
        uint8_t jc = m->cur - (uint8_t)first_c;
        uint8_t cx = (uint8_t)(((2u * jc + 1) * 128u) >> (kc + 1));
        uint8_t cy = ys[kc];
        uint16_t first_h = (((uint16_t)anchor + 1) << (kc + 1)) - 1;
        for(uint8_t h = 0; h < 2; h++) {
            uint8_t child = m->cur * 2 + 1 + h;
            uint8_t jh = child - (uint8_t)first_h;
            uint8_t hx = (uint8_t)(((2u * jh + 1) * 128u) >> (kc + 2));
            uint8_t hy = ys[kc + 1];
            canvas_draw_line(canvas, cx, cy + 2, hx, hy - 10);
            canvas_draw_frame(canvas, hx - 4, hy - 9, 9, 11);
        }
    }
}

// Agrega la letra del nodo actual (si tiene) y vuelve a la raiz.
// Llamar solo dentro de with_view_model.
static void tree_commit_letter(TreeModel* m) {
    char letter = (m->cur == 0) ? 0 : morse_tree_letters[m->cur];
    if(letter && m->len < TREE_TEXT_MAX - 1) {
        m->buffer[m->len++] = letter;
        m->buffer[m->len] = 0;
    }
    m->cur = 0;
}

// Tick periodico: confirma letras y espacios por pausa, y refresca la
// cuenta regresiva y el simbolo en curso. Corre en el hilo de timers.
static void tree_tick_callback(void* context) {
    SanMorseApp* app = context;
    uint32_t now = furi_get_tick();
    bool redraw = false;
    with_view_model(
        app->tree_view,
        TreeModel * m,
        {
            if(m->key_down) {
                redraw = true; // anima el simbolo punto->raya
            } else if(m->gap_phase == TreeGapLetter) {
                redraw = true;
                uint16_t letter_gap = app->settings.letter_gap_ms;
                if(letter_gap && now - m->last_release_tick >= furi_ms_to_ticks(letter_gap)) {
                    tree_commit_letter(m);
                    m->gap_phase = app->settings.word_gap_ms ? TreeGapWord : TreeGapNone;
                }
            } else if(m->gap_phase == TreeGapWord) {
                redraw = true;
                uint32_t word_gap = app->settings.word_gap_ms;
                // el espacio nunca antes que la confirmacion de letra
                if(word_gap <= app->settings.letter_gap_ms) {
                    word_gap = app->settings.letter_gap_ms + 1000;
                }
                if(now - m->last_release_tick >= furi_ms_to_ticks(word_gap)) {
                    if(m->len > 0 && m->buffer[m->len - 1] != ' ' &&
                       m->len < TREE_TEXT_MAX - 1) {
                        m->buffer[m->len++] = ' ';
                        m->buffer[m->len] = 0;
                    }
                    m->gap_phase = TreeGapNone;
                }
            }
        },
        redraw);
}

static void tree_key_up(SanMorseApp* app) {
    furi_hal_light_set(LightRed | LightGreen | LightBlue, 0x00);
    if(app->tree_tone) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        app->tree_tone = false;
    }
}

static bool tree_input_callback(InputEvent* event, void* context) {
    SanMorseApp* app = context;
    uint32_t now = furi_get_tick();

    // OK es la llave Morse: presionar = tono+luz, soltar = punto o raya
    if(event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            with_view_model(
                app->tree_view,
                TreeModel * m,
                {
                    m->key_down = true;
                    m->key_down_tick = now;
                    m->gap_phase = TreeGapNone;
                },
                true);
            if(app->settings.led) {
                furi_hal_light_set(LightRed | LightGreen | LightBlue, 0xFF);
            }
            if(app->settings.sound && furi_hal_speaker_acquire(5)) {
                app->tree_tone = true;
                furi_hal_speaker_start(
                    (float)app->settings.tone_hz, morse_settings_volume_f(&app->settings));
            }
        } else if(event->type == InputTypeRelease) {
            tree_key_up(app);
            with_view_model(
                app->tree_view,
                TreeModel * m,
                {
                    if(m->key_down) {
                        bool dah =
                            (now - m->key_down_tick) >= furi_ms_to_ticks(app->settings.dit_ms);
                        uint8_t child = m->cur * 2 + (dah ? 2 : 1);
                        if(child < MORSE_TREE_NODES) m->cur = child;
                        m->key_down = false;
                        m->last_release_tick = now;
                        m->gap_phase =
                            app->settings.letter_gap_ms ? TreeGapLetter : TreeGapNone;
                    }
                },
                true);
        }
        return true; // consumir tambien Short/Long/Repeat de OK
    }

    if(event->key == InputKeyBack) {
        bool consumed = false;
        if(event->type == InputTypeShort) {
            with_view_model(
                app->tree_view,
                TreeModel * m,
                {
                    if(m->cur != 0 || m->gap_phase != TreeGapNone) {
                        m->cur = 0;
                        m->gap_phase = TreeGapNone;
                        consumed = true;
                    }
                },
                true);
        }
        return consumed; // sin consumir -> volver al menu
    }

    // Izquierda: confirmar la letra sin esperar la pausa
    if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        with_view_model(
            app->tree_view,
            TreeModel * m,
            {
                if(m->cur != 0) tree_commit_letter(m);
                m->gap_phase = TreeGapNone;
            },
            true);
        return true;
    }

    // Derecha: reproducir lo escrito (confirmando la letra pendiente)
    if(event->key == InputKeyRight && event->type == InputTypeShort) {
        bool has_text = false;
        with_view_model(
            app->tree_view,
            TreeModel * m,
            {
                if(m->cur != 0) tree_commit_letter(m);
                m->gap_phase = TreeGapNone;
                has_text = (m->len > 0);
            },
            true);
        if(has_text) {
            app->play_text = app->tree_text;
            app->play_return_view = ViewIdTree;
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPlay);
        }
        return true;
    }

    if(event->key == InputKeyUp &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        with_view_model(
            app->tree_view,
            TreeModel * m,
            {
                if(m->cur != 0) {
                    m->cur = (m->cur - 1) / 2;
                    if(m->cur != 0 && app->settings.letter_gap_ms) {
                        m->last_release_tick = now;
                        m->gap_phase = TreeGapLetter;
                    } else {
                        m->gap_phase = TreeGapNone;
                    }
                } else if(m->len > 0) {
                    m->buffer[--m->len] = 0;
                    m->gap_phase = TreeGapNone;
                }
            },
            true);
        return true;
    }

    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        with_view_model(
            app->tree_view,
            TreeModel * m,
            {
                if(m->len < TREE_TEXT_MAX - 1) {
                    m->buffer[m->len++] = ' ';
                    m->buffer[m->len] = 0;
                }
                m->gap_phase = TreeGapNone;
            },
            true);
        return true;
    }

    return false;
}

static void tree_enter_callback(void* context) {
    SanMorseApp* app = context;
    furi_timer_start(app->tree_timer, furi_ms_to_ticks(TREE_TICK_MS));
}

static void tree_exit_callback(void* context) {
    SanMorseApp* app = context;
    furi_timer_stop(app->tree_timer);
    tree_key_up(app);
    with_view_model(
        app->tree_view,
        TreeModel * m,
        {
            m->key_down = false;
            m->gap_phase = TreeGapNone;
        },
        false);
}

// ------------------------------------------------------- vista: reproduccion

static void play_model_refresh(SanMorseApp* app) {
    with_view_model(
        app->play_view,
        PlayModel * m,
        {
            m->text = app->play_text;
            m->tr = tr_app(app);
            m->state = morse_player_get_state(app->player);
            m->pos = morse_player_get_position(app->player);
            m->sym = morse_player_get_symbol(app->player);
            m->wpm = app->settings.wpm;
            m->sound = app->settings.sound;
            m->vibro = app->settings.vibro;
        },
        true);
}

static void player_state_callback(void* context) {
    SanMorseApp* app = context;
    play_model_refresh(app);
}

static void play_draw_callback(Canvas* canvas, void* model) {
    PlayModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    const I18nStrings* tr = m->tr ? m->tr : &i18n_strings[0];
    const char* status = "...";
    switch(m->state) {
    case MorsePlayerStatePlaying:
        status = tr->play_playing;
        break;
    case MorsePlayerStatePaused:
        status = tr->play_paused;
        break;
    case MorsePlayerStateFinished:
        status = tr->play_done;
        break;
    default:
        break;
    }
    canvas_draw_str(canvas, 0, 8, status);
    char wpm_str[16];
    snprintf(wpm_str, sizeof(wpm_str), "%lu WPM", (unsigned long)m->wpm);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, wpm_str);

    if(!m->text || !m->text[0]) return;
    size_t len = strlen(m->text);
    size_t pos = (m->pos < len) ? m->pos : len - 1;

    // Caracter actual en grande + su codigo dibujado (punto = disco, raya = barra)
    char c = m->text[pos];
    if(c >= 'a' && c <= 'z') c -= 32;
    char cs[2] = {(c == ' ') ? '_' : c, 0};
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 8, 26, AlignCenter, AlignCenter, cs);
    canvas_set_font(canvas, FontSecondary);

    const char* code = (c == ' ') ? NULL : morse_lookup(c);
    if(code) {
        uint8_t x = 24;
        for(size_t j = 0; code[j]; j++) {
            bool done = (m->sym >= 0 && (size_t)m->sym >= j);
            if(code[j] == '-') {
                if(done) {
                    canvas_draw_box(canvas, x, 24, 12, 4);
                } else {
                    canvas_draw_frame(canvas, x, 24, 12, 4);
                }
                x += 15;
            } else {
                if(done) {
                    canvas_draw_disc(canvas, x + 2, 26, 2);
                } else {
                    canvas_draw_circle(canvas, x + 2, 26, 2);
                }
                x += 8;
            }
        }
    }

    // Linea de texto con el caracter actual resaltado
    size_t start = (pos > 9) ? pos - 9 : 0;
    char win[24];
    size_t n = 0;
    for(size_t i = start; i < len && n < 21; i++, n++) win[n] = m->text[i];
    win[n] = 0;
    canvas_draw_str(canvas, 2, 48, win);

    size_t pn = pos - start;
    char pre[24];
    memcpy(pre, win, pn);
    pre[pn] = 0;
    uint16_t px = canvas_string_width(canvas, pre);
    char cur[2] = {win[pn], 0};
    uint16_t cw = canvas_string_width(canvas, cur);
    if(cw < 3) cw = 3;
    canvas_draw_box(canvas, 2 + px - 1, 40, cw + 2, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 2 + px, 48, cur);
    canvas_set_color(canvas, ColorBlack);

    // Barra de progreso
    canvas_draw_frame(canvas, 0, 52, 128, 4);
    if(len > 1) {
        uint16_t w = (uint16_t)((pos * 126u) / (len - 1));
        canvas_draw_box(canvas, 1, 53, w, 2);
    }

    // Pie: estados de sonido/vibracion y accion de OK
    canvas_draw_str(canvas, 0, 63, m->sound ? tr->snd_on : tr->snd_off);
    canvas_draw_str(canvas, 40, 63, m->vibro ? tr->vib_on : tr->vib_off);
    const char* ok_hint = tr->play_ok_pause;
    if(m->state == MorsePlayerStateFinished) ok_hint = tr->play_ok_repeat;
    if(m->state == MorsePlayerStatePaused) ok_hint = tr->play_ok_resume;
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, ok_hint);
}

static bool play_input_callback(InputEvent* event, void* context) {
    SanMorseApp* app = context;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        MorsePlayerState state = morse_player_get_state(app->player);
        if(state == MorsePlayerStateFinished || state == MorsePlayerStateIdle) {
            morse_player_start(app->player, app->play_text);
        } else {
            morse_player_toggle_pause(app->player);
        }
        play_model_refresh(app);
        return true;
    }

    if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
       (event->key == InputKeyUp || event->key == InputKeyDown)) {
        uint8_t wpm = app->settings.wpm;
        if(event->key == InputKeyUp && wpm < 35) wpm++;
        if(event->key == InputKeyDown && wpm > 5) wpm--;
        app->settings.wpm = wpm;
        play_model_refresh(app);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        app->settings.sound = !app->settings.sound;
        play_model_refresh(app);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        app->settings.vibro = !app->settings.vibro;
        play_model_refresh(app);
        return true;
    }

    return false; // Back -> vista anterior (detiene en el exit callback)
}

static void play_enter_callback(void* context) {
    SanMorseApp* app = context;
    notification_message(app->notification, &sequence_display_backlight_enforce_on);
    morse_player_start(app->player, app->play_text);
    play_model_refresh(app);
}

static void play_exit_callback(void* context) {
    SanMorseApp* app = context;
    morse_player_stop(app->player);
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}

// --------------------------------------------------------------- ajustes

static const char* const volume_text[4] = {"25%", "50%", "75%", "100%"};
static const uint16_t tone_values[] = {440, 500, 600, 700, 800};
static const uint16_t dit_values[] = {150, 200, 250, 300, 350, 400};
static const uint16_t letter_gap_values[] = {0, 800, 1000, 1500, 2000, 2500, 3000};
static const uint16_t word_gap_values[] = {0, 2000, 3000, 4000, 5000, 6000, 8000};

#define VALUE_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t value_index_u16(const uint16_t* values, uint8_t count, uint16_t value) {
    for(uint8_t i = 0; i < count; i++) {
        if(values[i] == value) return i;
    }
    return 0;
}

static void item_text_ms(VariableItem* item, uint16_t ms) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u ms", ms);
    variable_item_set_current_value_text(item, buf);
}

static void item_text_sec(VariableItem* item, uint16_t ms, const char* off_text) {
    if(ms == 0) {
        variable_item_set_current_value_text(item, off_text);
        return;
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%u.%u s", ms / 1000, (ms % 1000) / 100);
    variable_item_set_current_value_text(item, buf);
}

static void item_text_onoff(VariableItem* item, SanMorseApp* app, bool on) {
    const I18nStrings* tr = tr_app(app);
    variable_item_set_current_value_text(item, on ? tr->val_on : tr->val_off);
}

// El cambio de idioma re-etiqueta menu y ayuda al instante; la propia lista
// de ajustes se reconstruye al volver a abrirla (no es seguro reconstruirla
// desde el callback de uno de sus items).
static void rebuild_menu_and_about(SanMorseApp* app);

static void setting_lang_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.lang = idx;
    variable_item_set_current_value_text(item, i18n_lang_names[idx]);
    rebuild_menu_and_about(app);
}

static void setting_sound_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.sound = idx;
    item_text_onoff(item, app, idx);
}

static void setting_volume_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, volume_text[idx]);
    app->settings.volume = idx;
}

static void setting_tone_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.tone_hz = tone_values[idx];
    char buf[12];
    snprintf(buf, sizeof(buf), "%u Hz", tone_values[idx]);
    variable_item_set_current_value_text(item, buf);
}

static void setting_vibro_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.vibro = idx;
    item_text_onoff(item, app, idx);
}

static void setting_led_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.led = idx;
    item_text_onoff(item, app, idx);
}

static void setting_wpm_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.wpm = idx + 5;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->settings.wpm);
    variable_item_set_current_value_text(item, buf);
}

static void setting_dit_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.dit_ms = dit_values[idx];
    item_text_ms(item, dit_values[idx]);
}

static void setting_letter_gap_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.letter_gap_ms = letter_gap_values[idx];
    item_text_sec(item, letter_gap_values[idx], tr_app(app)->val_off);
}

static void setting_word_gap_changed(VariableItem* item) {
    SanMorseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.word_gap_ms = word_gap_values[idx];
    item_text_sec(item, word_gap_values[idx], tr_app(app)->val_off);
}

// Refleja app->settings en la lista (al abrir Ajustes, por si WPM/sonido/
// vibracion cambiaron desde la vista de reproduccion).
static void settings_sync_items(SanMorseApp* app) {
    MorseSettings* s = &app->settings;
    const I18nStrings* tr = tr_app(app);

    variable_item_set_current_value_index(app->set_items[SetItemLang], s->lang);
    variable_item_set_current_value_text(app->set_items[SetItemLang], i18n_lang_names[s->lang]);

    variable_item_set_current_value_index(app->set_items[SetItemSound], s->sound);
    item_text_onoff(app->set_items[SetItemSound], app, s->sound);

    variable_item_set_current_value_index(app->set_items[SetItemVolume], s->volume);
    variable_item_set_current_value_text(app->set_items[SetItemVolume], volume_text[s->volume]);

    uint8_t idx = value_index_u16(tone_values, VALUE_COUNT(tone_values), s->tone_hz);
    s->tone_hz = tone_values[idx];
    variable_item_set_current_value_index(app->set_items[SetItemTone], idx);
    char buf[12];
    snprintf(buf, sizeof(buf), "%u Hz", s->tone_hz);
    variable_item_set_current_value_text(app->set_items[SetItemTone], buf);

    variable_item_set_current_value_index(app->set_items[SetItemVibro], s->vibro);
    item_text_onoff(app->set_items[SetItemVibro], app, s->vibro);

    variable_item_set_current_value_index(app->set_items[SetItemLed], s->led);
    item_text_onoff(app->set_items[SetItemLed], app, s->led);

    variable_item_set_current_value_index(app->set_items[SetItemWpm], s->wpm - 5);
    snprintf(buf, sizeof(buf), "%u", s->wpm);
    variable_item_set_current_value_text(app->set_items[SetItemWpm], buf);

    idx = value_index_u16(dit_values, VALUE_COUNT(dit_values), s->dit_ms);
    s->dit_ms = dit_values[idx];
    variable_item_set_current_value_index(app->set_items[SetItemDit], idx);
    item_text_ms(app->set_items[SetItemDit], s->dit_ms);

    idx = value_index_u16(letter_gap_values, VALUE_COUNT(letter_gap_values), s->letter_gap_ms);
    s->letter_gap_ms = letter_gap_values[idx];
    variable_item_set_current_value_index(app->set_items[SetItemLetterGap], idx);
    item_text_sec(app->set_items[SetItemLetterGap], s->letter_gap_ms, tr->val_off);

    idx = value_index_u16(word_gap_values, VALUE_COUNT(word_gap_values), s->word_gap_ms);
    s->word_gap_ms = word_gap_values[idx];
    variable_item_set_current_value_index(app->set_items[SetItemWordGap], idx);
    item_text_sec(app->set_items[SetItemWordGap], s->word_gap_ms, tr->val_off);
}

static void settings_build_items(SanMorseApp* app) {
    VariableItemList* list = app->settings_list;
    const I18nStrings* tr = tr_app(app);
    app->settings_list_lang = app->settings.lang;
    app->set_items[SetItemLang] =
        variable_item_list_add(list, tr->set_lang, 2, setting_lang_changed, app);
    app->set_items[SetItemSound] =
        variable_item_list_add(list, tr->set_sound, 2, setting_sound_changed, app);
    app->set_items[SetItemVolume] =
        variable_item_list_add(list, tr->set_volume, 4, setting_volume_changed, app);
    app->set_items[SetItemTone] = variable_item_list_add(
        list, tr->set_tone, VALUE_COUNT(tone_values), setting_tone_changed, app);
    app->set_items[SetItemVibro] =
        variable_item_list_add(list, tr->set_vibro, 2, setting_vibro_changed, app);
    app->set_items[SetItemLed] =
        variable_item_list_add(list, tr->set_led, 2, setting_led_changed, app);
    app->set_items[SetItemWpm] =
        variable_item_list_add(list, tr->set_wpm, 31, setting_wpm_changed, app);
    app->set_items[SetItemDit] = variable_item_list_add(
        list, tr->set_dit, VALUE_COUNT(dit_values), setting_dit_changed, app);
    app->set_items[SetItemLetterGap] = variable_item_list_add(
        list, tr->set_letter_gap, VALUE_COUNT(letter_gap_values), setting_letter_gap_changed, app);
    app->set_items[SetItemWordGap] = variable_item_list_add(
        list, tr->set_word_gap, VALUE_COUNT(word_gap_values), setting_word_gap_changed, app);
    settings_sync_items(app);
}

// --------------------------------------------------------- menu y texto

static void text_input_done_callback(void* context) {
    SanMorseApp* app = context;
    app->play_text = app->input_text;
    app->play_return_view = ViewIdTextInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPlay);
}

static void menu_callback(void* context, uint32_t index) {
    SanMorseApp* app = context;
    switch(index) {
    case MenuIndexTree:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdTree);
        break;
    case MenuIndexText:
        text_input_set_header_text(app->text_input, tr_app(app)->input_header);
        text_input_set_result_callback(
            app->text_input,
            text_input_done_callback,
            app,
            app->input_text,
            INPUT_TEXT_MAX,
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdTextInput);
        break;
    case MenuIndexSettings:
        if(app->settings_list_lang != app->settings.lang) {
            variable_item_list_reset(app->settings_list);
            settings_build_items(app);
        } else {
            settings_sync_items(app);
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSettings);
        break;
    case MenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdAbout);
        break;
    default:
        break;
    }
}

// Reconstruye los textos que dependen del idioma fuera de la lista de
// ajustes (menu principal y ayuda); es seguro porque no estan activos.
static void rebuild_menu_and_about(SanMorseApp* app) {
    const I18nStrings* tr = tr_app(app);
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "San Morse");
    submenu_add_item(app->submenu, tr->menu_tree, MenuIndexTree, menu_callback, app);
    submenu_add_item(app->submenu, tr->menu_text, MenuIndexText, menu_callback, app);
    submenu_add_item(app->submenu, tr->menu_settings, MenuIndexSettings, menu_callback, app);
    submenu_add_item(app->submenu, tr->menu_about, MenuIndexAbout, menu_callback, app);
    widget_reset(app->about);
    widget_add_text_scroll_element(app->about, 0, 0, 128, 64, tr->about);
}

// ----------------------------------------------------------------- app

int32_t san_morse_app(void* p) {
    UNUSED(p);
    SanMorseApp* app = malloc(sizeof(SanMorseApp));
    memset(app, 0, sizeof(SanMorseApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    morse_settings_load(&app->settings);

    app->player = morse_player_alloc(app->notification, &app->settings);
    morse_player_set_callback(app->player, player_state_callback, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Menu (los items se agregan en rebuild_menu_and_about)
    app->submenu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->submenu), nav_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMenu, submenu_get_view(app->submenu));

    // Arbol
    app->tree_view = view_alloc();
    view_set_context(app->tree_view, app);
    view_allocate_model(app->tree_view, ViewModelTypeLocking, sizeof(TreeModel));
    with_view_model(
        app->tree_view,
        TreeModel * m,
        {
            m->buffer = app->tree_text;
            m->settings = &app->settings;
            m->len = 0;
            m->cur = 0;
        },
        false);
    view_set_draw_callback(app->tree_view, tree_draw_callback);
    view_set_input_callback(app->tree_view, tree_input_callback);
    view_set_enter_callback(app->tree_view, tree_enter_callback);
    view_set_exit_callback(app->tree_view, tree_exit_callback);
    view_set_previous_callback(app->tree_view, nav_menu_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdTree, app->tree_view);
    app->tree_timer = furi_timer_alloc(tree_tick_callback, FuriTimerTypePeriodic, app);

    // Entrada de texto
    app->text_input = text_input_alloc();
    view_set_previous_callback(text_input_get_view(app->text_input), nav_menu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdTextInput, text_input_get_view(app->text_input));

    // Reproduccion
    app->play_view = view_alloc();
    view_set_context(app->play_view, app);
    view_allocate_model(app->play_view, ViewModelTypeLocking, sizeof(PlayModel));
    view_set_draw_callback(app->play_view, play_draw_callback);
    view_set_input_callback(app->play_view, play_input_callback);
    view_set_enter_callback(app->play_view, play_enter_callback);
    view_set_exit_callback(app->play_view, play_exit_callback);
    view_set_previous_callback(app->play_view, nav_play_return_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdPlay, app->play_view);

    // Ajustes
    app->settings_list = variable_item_list_alloc();
    settings_build_items(app);
    view_set_previous_callback(
        variable_item_list_get_view(app->settings_list), nav_menu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdSettings, variable_item_list_get_view(app->settings_list));

    // Ayuda (el texto se agrega en rebuild_menu_and_about)
    app->about = widget_alloc();
    view_set_previous_callback(widget_get_view(app->about), nav_menu_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdAbout, widget_get_view(app->about));

    rebuild_menu_and_about(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
    view_dispatcher_run(app->view_dispatcher);

    // Limpieza
    morse_settings_save(&app->settings);
    furi_timer_free(app->tree_timer);
    morse_player_free(app->player);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTree);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdPlay);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSettings);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdAbout);
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    variable_item_list_free(app->settings_list);
    widget_free(app->about);
    view_free(app->tree_view);
    view_free(app->play_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
